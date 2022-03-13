// license:BSD-3-Clause
// copyright-holders:windyfairy
/*
* Toshiba TC9446F, Audio Digital Processor for Decode of Dolby Digital (AC-3), MPEG2 Audio
*/

#ifndef MAME_SOUND_TC9446F_H
#define MAME_SOUND_TC9446F_H

#pragma once

class tc9446f_device : public device_t,
						public device_sound_interface
{
public:
	// construction/destruction
	tc9446f_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	int midio_r();
	int miack_r();

	void mimd_w(bool line);
	void mics_w(bool line);
	void midio_w(bool line);
	void mick_w(bool line);

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;

	virtual void sound_stream_update(sound_stream &stream, std::vector<read_stream_view> const &inputs, std::vector<write_stream_view> &outputs) override;

private:
	enum mode_select_t : uint8_t { SERIAL = 0, I2C };
	mode_select_t m_mode_select;
	int m_miack;

	int m_indata, m_inbits;
};

DECLARE_DEVICE_TYPE(TC9446F, tc9446f_device)

#endif // MAME_SOUND_TC9446F_H
