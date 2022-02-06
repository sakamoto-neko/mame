// license:BSD-3-Clause
// copyright-holders:Carl
#include "emu.h"
#include "midikbd.h"

DEFINE_DEVICE_TYPE(MIDI_KBD, midi_keyboard_device, "midi_kbd", "Generic MIDI Keyboard")

midi_keyboard_device::midi_keyboard_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, MIDI_KBD, tag, owner, clock),
	device_midi_port_interface(mconfig, *this),
	m_midiin(*this, "midiinimg"),
	m_out_tx_func(*this),
	m_keyboard(*this, "KEYBOARD")
{
}

void midi_keyboard_device::device_add_mconfig(machine_config &config)
{
	MIDIIN(config, m_midiin, 0);
	m_midiin->input_callback().set(FUNC(midi_keyboard_device::read));
}

void midi_keyboard_device::device_start()
{
	m_owner = dynamic_cast<midi_port_device *>(owner());
	m_out_tx_func.resolve_safe();
	m_keyboard_timer = timer_alloc();
	m_keyboard_timer->adjust(attotime::from_msec(10), 0, attotime::from_msec(10));
}

void midi_keyboard_device::device_timer(emu_timer &timer, device_timer_id id, int param)
{
	if(!id)
	{
		const int keyboard_notes[24] =
		{
			0x3c,   // C1
			0x3d,   // C1#
			0x3e,   // D1
			0x3f,   // D1#
			0x40,   // E1
			0x41,   // F1
			0x42,   // F1#
			0x43,   // G1
			0x44,   // G1#
			0x45,   // A1
			0x46,   // A1#
			0x47,   // B1
			0x48,   // C2
			0x49,   // C2#
			0x4a,   // D2
			0x4b,   // D2#
			0x4c,   // E2
			0x4d,   // F2
			0x4e,   // F2#
			0x4f,   // G2
			0x50,   // G2#
			0x51,   // A2
			0x52,   // A2#
			0x53,   // B2
		};

		int i;

		uint32_t kbstate = m_keyboard->read();
		if(kbstate != m_keyboard_state)
		{
			for (i=0; i < 24; i++)
			{
				int kbnote = keyboard_notes[i];

				if ((m_keyboard_state & (1 << i)) != 0 && (kbstate & (1 << i)) == 0)
				{
					// key was on, now off -> send Note Off message
					m_midiin->xmit_char(0x80);
					m_midiin->xmit_char(kbnote);
					m_midiin->xmit_char(0x7f);
				}
				else if ((m_keyboard_state & (1 << i)) == 0 && (kbstate & (1 << i)) != 0)
				{
					// key was off, now on -> send Note On message
					m_midiin->xmit_char(0x90);
					m_midiin->xmit_char(kbnote);
					m_midiin->xmit_char(0x7f);
				}
			}
		}
		else
			// no messages, send Active Sense message instead
			m_midiin->xmit_char(0xfe);

		m_keyboard_state = kbstate;
	}
}

INPUT_PORTS_START(midi_keyboard)
	PORT_START("KEYBOARD")
	PORT_BIT( 0x000001, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("C1") PORT_CODE(KEYCODE_Q)
	PORT_BIT( 0x000002, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("C1#") PORT_CODE(KEYCODE_W)
	PORT_BIT( 0x000004, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("D1") PORT_CODE(KEYCODE_E)
	PORT_BIT( 0x000008, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("D1#") PORT_CODE(KEYCODE_R)
	PORT_BIT( 0x000010, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("E1") PORT_CODE(KEYCODE_T)
	PORT_BIT( 0x000020, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("F1") PORT_CODE(KEYCODE_Y)
	PORT_BIT( 0x000040, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("F1#") PORT_CODE(KEYCODE_U)
	PORT_BIT( 0x000080, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("G1") PORT_CODE(KEYCODE_I)
	PORT_BIT( 0x000100, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("G1#") PORT_CODE(KEYCODE_O)
	PORT_BIT( 0x000200, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("A1") PORT_CODE(KEYCODE_A)
	PORT_BIT( 0x000400, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("A1#") PORT_CODE(KEYCODE_S)
	PORT_BIT( 0x000800, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("B1") PORT_CODE(KEYCODE_D)
	PORT_BIT( 0x001000, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("C2") PORT_CODE(KEYCODE_F)
	PORT_BIT( 0x002000, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("C2#") PORT_CODE(KEYCODE_G)
	PORT_BIT( 0x004000, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("D2") PORT_CODE(KEYCODE_H)
	PORT_BIT( 0x008000, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("D2#") PORT_CODE(KEYCODE_J)
	PORT_BIT( 0x010000, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("E2") PORT_CODE(KEYCODE_K)
	PORT_BIT( 0x020000, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("F2") PORT_CODE(KEYCODE_L)
	PORT_BIT( 0x040000, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("F2#") PORT_CODE(KEYCODE_Z)
	PORT_BIT( 0x080000, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("G2") PORT_CODE(KEYCODE_X)
	PORT_BIT( 0x100000, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("G2#") PORT_CODE(KEYCODE_C)
	PORT_BIT( 0x200000, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("A2") PORT_CODE(KEYCODE_V)
	PORT_BIT( 0x400000, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("A2#") PORT_CODE(KEYCODE_B)
	PORT_BIT( 0x800000, IP_ACTIVE_HIGH, IPT_OTHER ) PORT_NAME("B2") PORT_CODE(KEYCODE_N)
INPUT_PORTS_END

ioport_constructor midi_keyboard_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(midi_keyboard);
}
