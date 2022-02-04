// license:BSD-3-Clause
// copyright-holders:windyfairy
#include "emu.h"
#include "speaker.h"

#include "k573fpga.h"

#define LOG_GENERAL  (1 << 0)
#define VERBOSE      (LOG_GENERAL)
// #define LOG_OUTPUT_STREAM std::cout

#include "logmacro.h"

#include <algorithm>

// The higher the number, the more the chart/visuals will be delayed
attotime sample_skip_offset = attotime::zero;

k573fpga_device::k573fpga_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, KONAMI_573_DIGITAL_FPGA, tag, owner, clock),
	ram(*this, finder_base::DUMMY_TAG),
	mas3507d(*this, "mpeg"),
	is_ddrsbm_fpga(false)
{
}

void k573fpga_device::set_audio_offset(int32_t offset)
{
	sample_skip_offset = attotime::from_msec(offset);
}

void k573fpga_device::device_add_mconfig(machine_config &config)
{
	MAS3507D(config, mas3507d);
	mas3507d->set_stream_flags(STREAM_SYNCHRONOUS);
	mas3507d->mpeg_frame_sync_cb().set(*this, FUNC(k573fpga_device::mpeg_frame_sync));
	mas3507d->demand_cb().set(*this, FUNC(k573fpga_device::mas3507d_demand));
}

void k573fpga_device::device_start()
{
	save_item(NAME(crypto_key1));
	save_item(NAME(crypto_key2));
	save_item(NAME(crypto_key3));
	save_item(NAME(mp3_start_addr));
	save_item(NAME(mp3_end_addr));
	save_item(NAME(mp3_cur_start_addr));
	save_item(NAME(mp3_cur_end_addr));
	save_item(NAME(mp3_cur_addr));
	save_item(NAME(is_ddrsbm_fpga));
	save_item(NAME(is_stream_enabled));
	save_item(NAME(mpeg_status));
	save_item(NAME(fpga_status));
	save_item(NAME(frame_counter));
	save_item(NAME(frame_counter_base));
	save_item(NAME(counter_value));
	save_item(NAME(counter_current));
	save_item(NAME(counter_base));

	m_stream_bit_duration = attotime::from_nsec(attotime::from_hz(clock()).as_attoseconds() / 32000000);
	m_stream_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(k573fpga_device::update_stream), this));
	m_stream_timer->adjust(attotime::zero, 0, m_stream_bit_duration);
}

void k573fpga_device::device_reset()
{
	mp3_start_addr = 0;
	mp3_end_addr = 0;
	mp3_cur_start_addr = 0;
	mp3_cur_end_addr = 0;
	mp3_cur_addr = 0;

	crypto_key1 = 0;
	crypto_key2 = 0;
	crypto_key3 = 0;

	is_stream_enabled = false;

	counter_current = counter_base = machine().time();

	mpeg_status = PLAYBACK_STATE_IDLE;
	frame_counter = frame_counter_base = 0;
	counter_value = 0;

	mas3507d->reset_playback();
}

void k573fpga_device::reset_counter()
{
	// There is a delay when resetting the timer but I don't know exactly how long it is.
	// DDR Extreme: when this register is reset, the game expects to be able to read back 0 from the counter for 2 consecutive reads
	// or else it'll keep writing 0 to the register. Uses VSync(-1) to force timing to vblanks.
	// Drummania 5th mix seems to not like it when the counter is reset immediately because it isn't able to read 
	counter_current = counter_base = machine().time();
	counter_value = 0;
	frame_counter_base = frame_counter;
}

void k573fpga_device::update_counter()
{
	if (is_ddrsbm_fpga) {
		// The counter for Solo Bass Mix is used differently than other games.
		// DDR Solo Bass Mix will sync the internal playback timer to the first second of the MP3 using the MP3 frame counter.
		// After that the playback timer is incremented using the difference between the last counter value and the current counter value.
		// This counter register itself is always running even when no audio is playing.
		// TODO: What happens when mp3_counter_low_w is written to on Solo Bass Mix?
		counter_value = (machine().time() - counter_base).as_double();
		return;
	}

	// The timer in any game outside of DDR Solo Bass Mix is both tied to the MP3 playback and independent.
	// The timer will only start when an MP3 begins playback (seems to be synced to when the MP3 frame counter increments).
	// The timer will keep going long after the MP3 has stopped playing.
	// If the timer is zero'd out while it's running (k573dio mp3_counter_low_w), it will start counting up from zero again.
	// TODO: What happens if a non-zero value is written to mp3_counter_low_w?
	// TODO: How exactly do you stop the timer? Can it even be stopped once it's started?
	if (frame_counter - frame_counter_base == 0) {
		return;
	}

	counter_base = counter_current;
	counter_current = machine().time();

	auto diff = (counter_current - counter_base).as_double();
	counter_value += diff;
}

uint32_t k573fpga_device::get_counter()
{
	// Potential for a bug here?
	// When reading the counter on real hardware consecutively the value returned changes so I think it's always running.
	// It may be possible that the counter can go from 0xffff to 0x10000 between reading the upper and lower values,
	// which may result in the game seeing 0x1ffff before it goes back down to something like 0x10001 on the next read.
	update_counter();

	auto t = std::max(0.0, counter_value - sample_skip_offset.as_double());
	return t * 44100;
}

uint32_t k573fpga_device::get_counter_diff()
{
	// Delta playback time since last counter update.
	// I couldn't find any active usages of this register but it exists in some code paths.
	// The functionality was tested using custom code running on real hardware.
	// When this is called, it will return the difference between the current counter value
	// and the last read counter value, and then reset the counter back to the previously read counter's value.
	auto diff = machine().time() - counter_current;
	return diff.as_double() * 44100;
}

uint16_t k573fpga_device::get_mp3_frame_count()
{
	// All games can read this but only DDR Solo Bass Mix actively uses it.
	// Returns the same value as using a default read to get the frame counter from the MAS3507D over i2c.
	return frame_counter & 0xffff;
}

uint16_t k573fpga_device::mas_i2c_r()
{
	uint16_t scl = mas3507d->i2c_scl_r() << 13;
	uint16_t sda = mas3507d->i2c_sda_r() << 12;
	return scl | sda;
}

void k573fpga_device::mas_i2c_w(uint16_t data)
{
	mas3507d->i2c_scl_w(data & 0x2000);
	mas3507d->i2c_sda_w(data & 0x1000);
}

uint16_t k573fpga_device::get_mpeg_ctrl()
{
	return mpeg_status;
}

uint16_t k573fpga_device::get_fpga_ctrl()
{
	// 0x0000 Not Streaming
	// 0x1000 Streaming
	return (is_stream_enabled && mp3_cur_addr >= mp3_cur_start_addr && mp3_cur_addr < mp3_cur_end_addr) << 12;
}

void k573fpga_device::set_fpga_ctrl(uint16_t data)
{
	/*
	TODO: what's the difference between the two pre-stream start up sequences?

	Unique sequences:
	On start up and before starting a new stream:
		x x 1
		x x 0
		x x 1
		x x 1
		x x 1

	Also on start up and before starting a new stream:
		0 x x
		1 x x

	Stop streaming:
		x 0 x

	Start streaming:
		x 1 x

	x is remains the same as the previous command
	*/

	printf("FPGA MPEG control %c%c%c | %04x\n",
				data & 0x8000 ? '#' : '.',
				data & 0x4000 ? '#' : '.', // "Active" flag. The FPGA will never start streaming data without this bit set
				data & 0x2000 ? '#' : '.',
				data);

	if (BIT(data, 14) && (is_ddrsbm_fpga || !BIT(fpga_status, 14))) {
		// Start streaming
		is_stream_enabled = true;
		mp3_cur_addr = mp3_start_addr;
		mp3_cur_start_addr = mp3_start_addr;
		mp3_cur_end_addr = mp3_end_addr;
		frame_counter = 0;
		reset_counter();
	} else if (!BIT(data, 14) && (is_ddrsbm_fpga || BIT(fpga_status, 14))) {
		// Stop stream
		is_stream_enabled = false;

		if (!is_ddrsbm_fpga)
			reset_counter();
	}

	fpga_status = data;
}

uint16_t k573fpga_device::decrypt_default(uint16_t v)
{
	uint16_t m = crypto_key1 ^ crypto_key2;

	v = bitswap<16>(
		v,
		15 - BIT(m, 0xF),
		14 + BIT(m, 0xF),
		13 - BIT(m, 0xE),
		12 + BIT(m, 0xE),
		11 - BIT(m, 0xB),
		10 + BIT(m, 0xB),
		9 - BIT(m, 0x9),
		8 + BIT(m, 0x9),
		7 - BIT(m, 0x8),
		6 + BIT(m, 0x8),
		5 - BIT(m, 0x5),
		4 + BIT(m, 0x5),
		3 - BIT(m, 0x3),
		2 + BIT(m, 0x3),
		1 - BIT(m, 0x2),
		0 + BIT(m, 0x2)
	);

	v ^= (BIT(m, 0xD) << 14) ^
		(BIT(m, 0xC) << 12) ^
		(BIT(m, 0xA) << 10) ^
		(BIT(m, 0x7) << 8) ^
		(BIT(m, 0x6) << 6) ^
		(BIT(m, 0x4) << 4) ^
		(BIT(m, 0x1) << 2) ^
		(BIT(m, 0x0) << 0);

	v ^= bitswap<16>(
		(uint16_t)crypto_key3,
		7, 0, 6, 1,
		5, 2, 4, 3,
		3, 4, 2, 5,
		1, 6, 0, 7
	);

	crypto_key1 = (crypto_key1 & 0x8000) | ((crypto_key1 << 1) & 0x7FFE) | ((crypto_key1 >> 14) & 1);

	if(((crypto_key1 >> 15) ^ crypto_key1) & 1)
		crypto_key2 = (crypto_key2 << 1) | (crypto_key2 >> 15);

	crypto_key3++;

	return v;
}

uint16_t k573fpga_device::decrypt_ddrsbm(uint16_t data)
{
	// TODO: Work out the proper algorithm here.
	// ddrsbm is capable of sending a pre-mutated key, similar to the other games, that is used to simulate seeking.
	// I couldn't find evidence that the game ever seeks in the MP3 so the game doesn't break from lack of support from what I can tell.
	// The proper algorithm for mutating the key is: crypto_key1 = rol(crypto_key1, offset & 0x0f)
	// A hack such as rotating the key back to its initial state could be done if ever required, until the proper algorithm is worked out.

	uint8_t key[16] = {0};
	uint16_t key_state = bitswap<16>(
		crypto_key1,
		13, 11, 9, 7,
		5, 3, 1, 15,
		14, 12, 10, 8,
		6, 4, 2, 0
	);

	for(int i = 0; i < 8; i++) {
		key[i * 2] = key_state & 0xff;
		key[i * 2 + 1] = (key_state >> 8) & 0xff;
		key_state = ((key_state & 0x8080) >> 7) | ((key_state & 0x7f7f) << 1);
	}

	uint16_t output_word = 0;
	for(int cur_bit = 0; cur_bit < 8; cur_bit++) {
		int even_bit_shift = cur_bit * 2;
		int odd_bit_shift = cur_bit * 2 + 1;
		bool is_even_bit_set = data & (1 << even_bit_shift);
		bool is_odd_bit_set = data & (1 << odd_bit_shift);
		bool is_key_bit_set = key[crypto_key3 & 15] & (1 << cur_bit);
		bool is_scramble_bit_set = key[(crypto_key3 - 1) & 15] & (1 << cur_bit);

		if(is_scramble_bit_set)
			std::swap(is_even_bit_set, is_odd_bit_set);

		if(is_even_bit_set ^ is_key_bit_set)
			output_word |= 1 << even_bit_shift;

		if(is_odd_bit_set)
			output_word |= 1 << odd_bit_shift;
	}

	crypto_key3++;

	return output_word;
}

TIMER_CALLBACK_MEMBER(k573fpga_device::update_stream)
{
	if (!(mpeg_status & PLAYBACK_STATE_DEMAND)) {
		// If the data isn't being demanded currently then it has enough data to decode a few frames already
		return;
	}

	// Note: The FPGA code seems to have an off by 1 error where it'll always decrypt and send an extra word at the end of every MP3 which corresponds to decrypting the value 0x0000.
	if (!is_stream_enabled || mp3_cur_addr < mp3_cur_start_addr || mp3_cur_addr > mp3_cur_end_addr) {
		return;
	}

	if (mp3_data_bits <= 0) {
		uint16_t src = ram[mp3_cur_addr >> 1];
		mp3_data = is_ddrsbm_fpga ? decrypt_ddrsbm(src) : decrypt_default(src);
		mp3_data = ((mp3_data >> 8) & 0xff) | ((mp3_data & 0xff) << 8);
		mp3_cur_addr += 2;
		mp3_data_bits = 16;
	}

	mas3507d->sic_w(true);
	mas3507d->sid_w(mp3_data & 1);
	mas3507d->sic_w(false);
	mp3_data >>= 1;
	mp3_data_bits--;
}

WRITE_LINE_MEMBER(k573fpga_device::mpeg_frame_sync)
{
	mpeg_status &= ~(PLAYBACK_STATE_PLAYING | PLAYBACK_STATE_IDLE);

	if (state) {
		mpeg_status |= PLAYBACK_STATE_PLAYING;
		frame_counter++;
	}
	else {
		mpeg_status |= PLAYBACK_STATE_IDLE;
	}
}

WRITE_LINE_MEMBER(k573fpga_device::mas3507d_demand)
{
	// This will be set when the MAS3507D is requesting more data
	if (state && !(mpeg_status & PLAYBACK_STATE_DEMAND)) {
		mpeg_status |= PLAYBACK_STATE_DEMAND;
	}
	else if (!state && (mpeg_status & PLAYBACK_STATE_DEMAND)) {
		mpeg_status &= ~PLAYBACK_STATE_DEMAND;
	}
}

DEFINE_DEVICE_TYPE(KONAMI_573_DIGITAL_FPGA, k573fpga_device, "k573fpga", "Konami 573 Digital I/O FPGA")
