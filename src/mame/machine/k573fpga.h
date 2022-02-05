// license:BSD-3-Clause
// copyright-holders:windyfairy
#ifndef MAME_MACHINE_K573FPGA_H
#define MAME_MACHINE_K573FPGA_H

#pragma once

#include "sound/mas3507d.h"
#include "machine/ds2401.h"
#include "machine/timer.h"

DECLARE_DEVICE_TYPE(KONAMI_573_DIGITAL_FPGA, k573fpga_device)

class k573fpga_device : public device_t
{
public:
	k573fpga_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	template <typename... T> void add_route(T &&... args) { subdevice<mas3507d_device>("mpeg")->add_route(std::forward<T>(args)...); }
	template <typename T> void set_ram(T &&tag) { ram.set_tag(std::forward<T>(tag)); }

	void set_ddrsbm_fpga(bool flag) { is_ddrsbm_fpga = flag; }

	TIMER_DEVICE_CALLBACK_MEMBER(update_counter_callback);
	TIMER_CALLBACK_MEMBER(update_stream);

	uint32_t get_decrypted();
	DECLARE_WRITE_LINE_MEMBER(mpeg_frame_sync);
	DECLARE_WRITE_LINE_MEMBER(mas3507d_demand);

	void set_crypto_key1(uint16_t v) { crypto_key1 = v; }
	void set_crypto_key2(uint16_t v) { crypto_key2 = v; }
	void set_crypto_key3(uint8_t v) { crypto_key3 = v; }

	uint32_t get_mp3_start_addr() { return mp3_start_addr; }
	void set_mp3_start_addr(uint32_t v) { mp3_start_addr = v; }

	uint32_t get_mp3_end_addr() { return mp3_end_addr; }
	void set_mp3_end_addr(uint32_t v) { mp3_end_addr = v; }

	uint16_t mas_i2c_r();
	void mas_i2c_w(uint16_t data);

	uint16_t get_fpga_ctrl();
	void set_fpga_ctrl(uint16_t data);

	uint16_t get_mpeg_ctrl();

	uint32_t get_counter();
	uint32_t get_counter_diff();
	uint16_t get_mp3_frame_count();

	void reset_counter();

	void set_audio_offset(int32_t offset);
	void update_clock(uint32_t speed);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_add_mconfig(machine_config &config) override;

private:
	uint16_t decrypt_default(uint16_t data);
	uint16_t decrypt_ddrsbm(uint16_t data);

	void update_counter();

	emu_timer* m_stream_timer;

	enum {
		PLAYBACK_STATE_UNKNOWN = 0x8000,

		// The only time demand shouldn't be set is when the MAS3507D's MP3 buffer is full and isn't requesting more data through the demand pin
		PLAYBACK_STATE_DEMAND = 0x1000,

		// Set when the mas3507d's frame counter isn't being updated anymore.
		// Shortly after the last MP3 frame is played the state goes back to idle.
		// TODO: Add mas3507d MP3 frame sync pin callback
		PLAYBACK_STATE_IDLE = PLAYBACK_STATE_UNKNOWN | 0x2000,

		// Set when the mas3507d's frame counter is being updated still.
		PLAYBACK_STATE_PLAYING = PLAYBACK_STATE_UNKNOWN | 0x4000,
	};

	required_shared_ptr<uint16_t> ram;
	required_device<mas3507d_device> mas3507d;

	uint16_t crypto_key1, crypto_key2;
	uint8_t crypto_key3;

	uint32_t mp3_start_addr, mp3_end_addr;
	uint32_t mp3_cur_start_addr, mp3_cur_end_addr, mp3_cur_addr;
	uint16_t mp3_data;
	int mp3_remaining_bytes;
	bool is_ddrsbm_fpga;

	bool is_stream_enabled;
	attotime counter_current, counter_base;

	uint16_t mpeg_status, fpga_status;
	uint32_t frame_counter, frame_counter_base;
	double counter_value;
};

#endif // MAME_MACHINE_K573FPGA_H
