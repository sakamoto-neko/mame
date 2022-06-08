// license:BSD-3-Clause
// copyright-holders:Carl
#ifndef MAME_MACHINE_MIDIKBD_H
#define MAME_MACHINE_MIDIKBD_H

#pragma once

#include "midi.h"
#include "imagedev/midiin.h"

class midi_keyboard_device : public device_t,
	public device_midi_port_interface
{
public:
	midi_keyboard_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
	ioport_constructor device_input_ports() const override;

	auto tx_callback() { return m_out_tx_func.bind(); }

protected:
	virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;
	virtual void device_reset() override { }

	TIMER_CALLBACK_MEMBER(scan_keyboard);

private:
	DECLARE_WRITE_LINE_MEMBER( read ) { output_rxd(state); }

	required_device<midiin_device> m_midiin;

	emu_timer *m_keyboard_timer;
	devcb_write_line m_out_tx_func;
	required_ioport m_keyboard;
	uint32_t m_keyboard_state;
};

DECLARE_DEVICE_TYPE(MIDI_KBD, midi_keyboard_device)

#endif // MAME_MACHINE_MIDIKBD_H
