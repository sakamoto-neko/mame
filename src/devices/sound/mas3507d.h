// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
#ifndef MAME_SOUND_MAS3507D_H
#define MAME_SOUND_MAS3507D_H

#pragma once

#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_STDIO
#include "minimp3/minimp3.h"

class mas3507d_device : public device_t, public device_sound_interface
{
public:
	enum {
		PLAYBACK_STATE_IDLE,
		PLAYBACK_STATE_BUFFER_FULL,
		PLAYBACK_STATE_DEMAND_BUFFER
	};

	// construction/destruction
	mas3507d_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	auto sample_cb() { return cb_sample.bind(); }
	auto mpeg_frame_sync_cb() { return cb_mpeg_frame_sync.bind(); }
	auto demand_cb() { return cb_demand.bind(); }

	int i2c_scl_r();
	int i2c_sda_r();
	void i2c_scl_w(bool line);
	void i2c_sda_w(bool line);

	uint32_t get_frame_count() const { return decoded_frame_count; }
	uint32_t get_samples() const { return decoded_samples; }

	void update_stream() { stream->update(); }

	void set_stream_flags(sound_stream_flags new_stream_flags) { stream_flags = new_stream_flags; }

	void reset_playback();

	void set_playback_speed(uint32_t speed) {
		switch (speed) {
			case 2:
				playback_speed = 1.1;
				break;

			case 3:
				playback_speed = 1.2;
				break;

			case 1:
			default:
				playback_speed = 1.0;
				break;
		}

		printf("Setting playback speed to %d (%lfx)\n", speed, playback_speed);

		stream->set_sample_rate(current_rate * playback_speed);
	}

protected:
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void sound_stream_update(sound_stream &stream, std::vector<read_stream_view> const &inputs, std::vector<write_stream_view> &outputs) override;

private:
	void i2c_nak();
	bool i2c_device_got_address(uint8_t address);
	void i2c_device_got_byte(uint8_t byte);
	void i2c_device_got_stop();

	void mem_write(int bank, uint32_t adr, uint32_t val);
	void run_program(uint32_t adr);
	void reg_write(uint32_t adr, uint32_t val);

	void fill_buffer();
	void append_buffer(std::vector<write_stream_view> &outputs, int &pos, int scount);

	devcb_read32 cb_sample;
	devcb_write_line cb_mpeg_frame_sync;
	devcb_write_line cb_demand;

	enum {
		CMD_DEV_WRITE = 0x3a,
		CMD_DEV_READ = 0x3b,

		CMD_DATA_WRITE = 0x68,
		CMD_DATA_READ = 0x69,
		CMD_CONTROL_WRITE = 0x6a
	};

	enum i2c_bus_state_t : uint8_t { IDLE = 0, STARTED, NAK, ACK, ACK2 };
	enum i2c_bus_address_t : uint8_t { UNKNOWN = 0, VALIDATED, WRONG };
	enum i2c_subdest_t : uint8_t { UNDEFINED = 0, CONTROL, DATA_READ, DATA_WRITE, BAD };
	enum i2c_command_t : uint8_t { CMD_BAD = 0, CMD_RUN, CMD_READ_CTRL, CMD_WRITE_REG, CMD_WRITE_MEM, CMD_READ_REG, CMD_READ_MEM };

	i2c_bus_state_t i2c_bus_state;
	i2c_bus_address_t i2c_bus_address;
	i2c_subdest_t i2c_subdest;
	i2c_command_t i2c_command;

	mp3dec_t mp3_dec;
	mp3dec_frame_info_t mp3_info;

	sound_stream *stream;
	sound_stream_flags stream_flags;

	std::array<uint8_t, 0x900> mp3data;
	std::array<mp3d_sample_t, MINIMP3_MAX_SAMPLES_PER_FRAME> samples;

	bool i2c_scli, i2c_sclo, i2c_sdai, i2c_sdao;
	int i2c_bus_curbit;
	uint8_t i2c_bus_curval;
	int i2c_bytecount;
	uint32_t i2c_io_bank, i2c_io_adr, i2c_io_count, i2c_io_val;
	uint32_t i2c_sdao_data;

	uint32_t mp3data_count, current_rate;
	uint32_t decoded_frame_count, decoded_samples;
	int32_t sample_count, samples_idx;

	bool is_muted;
	float gain_ll, gain_rr;

	uint32_t playback_status;
	double playback_speed;
};


// device type definition
DECLARE_DEVICE_TYPE(MAS3507D, mas3507d_device)

#endif // MAME_SOUND_MAS3507D_H
