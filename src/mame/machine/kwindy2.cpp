// license:BSD-3-Clause
// copyright-holders:windyfairy
/*
 * Konami Windy2 I/O (JVS)
 */

#include "emu.h"
#include "kwindy2.h"

kwindy2_device::kwindy2_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	jvs_device(mconfig, KONAMI_WINDY2_JVS_IO, tag, owner, clock),
	m_in1(*this, "IN1"),
	m_test_port(*this, "TEST"),
	m_player_ports(*this, "P%u", 1U)
{
}

void kwindy2_device::device_start()
{
	jvs_device::device_start();
}

void kwindy2_device::device_reset()
{
	jvs_device::device_reset();

	std::fill(std::begin(m_coin_counter), std::end(m_coin_counter), 0);
}

void kwindy2_device::device_add_mconfig(machine_config &config)
{
}

const char *kwindy2_device::device_id()
{
	return "KONAMI CO.,LTD.;Windy2;Ver1.0;Windy2 I/O Ver1.0";
}

uint8_t kwindy2_device::command_format_version()
{
	return 0x11;
}

uint8_t kwindy2_device::jvs_standard_version()
{
	return 0x20;
}

uint8_t kwindy2_device::comm_method_version()
{
	return 0x10;
}

void kwindy2_device::function_list(uint8_t*& buf)
{
	// SW input - 2 players, 16 bits?
	*buf++ = 0x01;
	*buf++ = 2;
	*buf++ = 16;
	*buf++ = 0;

	/*
	// Coin input - 1 slot?
	*buf++ = 0x02;
	*buf++ = 1;
	*buf++ = 0;
	*buf++ = 0;
	*/
}

bool kwindy2_device::coin_counters(uint8_t*& buf, uint8_t count)
{
	if (count > 1)
		return false;

	// TODO: Coins are broken
	for (int i = 0; i < count; i++) {
		*buf++ = m_coin_counter[i] >> 8;
		*buf++ = m_coin_counter[i];
	}

	return true;
}

bool kwindy2_device::switches(uint8_t*& buf, uint8_t count_players, uint8_t bytes_per_switch)
{
	if (count_players > 2 || bytes_per_switch > 2)
		return false;

	*buf++ = m_test_port->read();

	for (int i = 0; i < count_players; i++)
	{
		uint32_t pval = m_player_ports[i]->read();
		for (int j = 0; j < bytes_per_switch; j++)
		{
			*buf++ = (uint8_t)(pval >> ((1 - j) * 8));
		}
	}

	return true;
}

INPUT_CHANGED_MEMBER(kwindy2_device::coin_inserted)
{
	m_coin_counter[param & 1]++;
}

int kwindy2_device::handle_message(const uint8_t* send_buffer, uint32_t send_size, uint8_t*& recv_buffer)
{
	switch (send_buffer[0]) {
	case 0xf0:
		// msg: f0 d9
		device_reset();
		break;
	}

	// Command not recognized, pass it off to the base message handler
	return jvs_device::handle_message(send_buffer, send_size, recv_buffer);
}

INPUT_PORTS_START( kwindy2 )
	PORT_START("IN1")
	PORT_DIPNAME(0x00000001, 0x00000001, "Unused 1") PORT_DIPLOCATION("DIP SW:1")
	PORT_DIPNAME(0x00000002, 0x00000002, "Unused 2") PORT_DIPLOCATION("DIP SW:2")
	PORT_DIPNAME(0x00000004, 0x00000004, "Unused 3") PORT_DIPLOCATION("DIP SW:3")
	PORT_DIPNAME(0x00000008, 0x00000008, "Unused 4") PORT_DIPLOCATION("DIP SW:4")
	PORT_DIPNAME(0x00000010, 0x00000010, "Unused 1") PORT_DIPLOCATION("DIP SW:5")
	PORT_DIPNAME(0x00000020, 0x00000020, "Unused 2") PORT_DIPLOCATION("DIP SW:6")
	PORT_DIPNAME(0x00000040, 0x00000040, "Unused 3") PORT_DIPLOCATION("DIP SW:7")
	PORT_DIPNAME(0x00000080, 0x00000080, "Unused 4") PORT_DIPLOCATION("DIP SW:8")

	PORT_START("TEST")
	PORT_SERVICE_NO_TOGGLE(0x80, IP_ACTIVE_HIGH)            /* Test Button */
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START("P1")
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_START1) PORT_PLAYER(1)
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_PLAYER(1)
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_BUTTON2) PORT_PLAYER(1)
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_BUTTON3) PORT_PLAYER(1)
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_BUTTON4) PORT_PLAYER(1)
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_BUTTON5) PORT_PLAYER(1)
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_BUTTON6) PORT_PLAYER(1)
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_BUTTON7) PORT_PLAYER(1)
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_BUTTON8) PORT_PLAYER(1)
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_BUTTON9) PORT_PLAYER(1)
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_BUTTON10) PORT_PLAYER(1)

	PORT_START("P2")
	PORT_BIT(0x8000, IP_ACTIVE_HIGH, IPT_START2) PORT_PLAYER(1)
	PORT_BIT(0x4000, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT(0x2000, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT(0x1000, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT(0x0800, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT(0x0400, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT(0x0200, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_PLAYER(2)
	PORT_BIT(0x0100, IP_ACTIVE_HIGH, IPT_BUTTON2) PORT_PLAYER(2)
	PORT_BIT(0x0080, IP_ACTIVE_HIGH, IPT_BUTTON3) PORT_PLAYER(2)
	PORT_BIT(0x0040, IP_ACTIVE_HIGH, IPT_BUTTON4) PORT_PLAYER(2)
	PORT_BIT(0x0020, IP_ACTIVE_HIGH, IPT_BUTTON5) PORT_PLAYER(2)
	PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_BUTTON6) PORT_PLAYER(2)
	PORT_BIT(0x0008, IP_ACTIVE_HIGH, IPT_BUTTON7) PORT_PLAYER(2)
	PORT_BIT(0x0004, IP_ACTIVE_HIGH, IPT_BUTTON8) PORT_PLAYER(2)
	PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_BUTTON9) PORT_PLAYER(2)
	PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_BUTTON10) PORT_PLAYER(2)

	PORT_START("COINS")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_COIN1) PORT_CHANGED_MEMBER(DEVICE_SELF, kwindy2_device, coin_inserted, 0)
INPUT_PORTS_END

ioport_constructor kwindy2_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(kwindy2);
}

DEFINE_DEVICE_TYPE(KONAMI_WINDY2_JVS_IO, kwindy2_device, "kwindy2", "Konami Windy2 I/O")
