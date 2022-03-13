// license:BSD-3-Clause
// copyright-holders:windyfairy
/*
* Toshiba TC9446F, Audio Digital Processor for Decode of Dolby Digital (AC-3), MPEG2 Audio
*/

#include "emu.h"
#include "tc9446f.h"

#define LOG_GENERAL  (1 << 0)
#define VERBOSE      (LOG_GENERAL)
#define LOG_OUTPUT_STREAM std::cout

#include <iostream>
#include "logmacro.h"

DEFINE_DEVICE_TYPE(TC9446F, tc9446f_device, "tc9446f", "Toshiba TC9446F")

ALLOW_SAVE_TYPE(tc9446f_device::mode_select_t)

tc9446f_device::tc9446f_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, TC9446F, tag, owner, clock),
	device_sound_interface(mconfig, *this)
{
}

void tc9446f_device::device_start()
{
	save_item(NAME(m_mode_select));
	save_item(NAME(m_miack));
	save_item(NAME(m_indata));
	save_item(NAME(m_inbits));
}

void tc9446f_device::device_reset()
{
	m_mode_select = SERIAL;
	m_miack = 0;
	m_indata = m_inbits = 0;
}

int tc9446f_device::midio_r()
{
	// Serial: Data output
	// I2C: Data output (SDA)
	auto r = 0;
	//LOGMASKED(LOG_GENERAL, "midio_r: %d\n", r);
	return r;
}

int tc9446f_device::miack_r()
{
	// Serial: Acknowledge signal output and out of control detection output
	// I2C: Out of control detection output
	auto r = m_miack;
	m_miack = 0;
	//LOGMASKED(LOG_GENERAL, "miack_r: %d\n", r);
	return r;
}

void tc9446f_device::mimd_w(bool line)
{
	// Mode select input for MCU interface
	m_mode_select = line ? I2C : SERIAL;
	//LOGMASKED(LOG_GENERAL, "mimd_w: %d\n", line);
}

void tc9446f_device::mics_w(bool line)
{
	if (m_mode_select == I2C)
		return;

	m_miack = 0;
	//LOGMASKED(LOG_GENERAL, "mics_w: %d\n", line);
}

void tc9446f_device::midio_w(bool line)
{
	// Serial: Data input
	// I2C: Data input (SDA)
	m_indata = (m_indata << 1) | line;
	//LOGMASKED(LOG_GENERAL, "midio_w: %d | %d %06x\n", line, m_inbits, m_indata);
	m_inbits++;

	if (m_inbits >= 24) {
		// Found full word
		m_miack = 1;
		m_indata = 0;
		m_inbits = 0;
	}
}

void tc9446f_device::mick_w(bool line)
{
	// Serial: Clock input
	// I2C: Clock input (SCL)
	//LOGMASKED(LOG_GENERAL, "mick_w: %d\n", line);
}

void tc9446f_device::sound_stream_update(sound_stream &stream, std::vector<read_stream_view> const &inputs, std::vector<write_stream_view> &outputs)
{
	for (auto& output : outputs) {
		output.fill(0, output.samples());
	}
}
