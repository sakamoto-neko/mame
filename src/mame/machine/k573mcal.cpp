// license:BSD-3-Clause
// copyright-holders:windyfairy
/*
 * Konami 573 Master Calendar
 *
 * Not much is known about the actual details of the device.
 * The device itself allows for reprogramming the security cartridge.
 */

#include "emu.h"
#include "k573mcal.h"

#include "machine/timehelp.h"

k573mcal_device::k573mcal_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	jvs_device(mconfig, KONAMI_573_MASTER_CALENDAR, tag, owner, clock),
	m_in1(*this, "IN1")
{
}

void k573mcal_device::device_start()
{
	jvs_device::device_start();
}

void k573mcal_device::device_reset()
{
	jvs_device::device_reset();
}

void k573mcal_device::device_add_mconfig(machine_config &config)
{
}

const char *k573mcal_device::device_id()
{
	return "KONAMI CO.,LTD.;Master Calendar;Ver1.0;";
}

uint8_t k573mcal_device::command_format_version()
{
	return 0x11;
}

uint8_t k573mcal_device::jvs_standard_version()
{
	return 0x20;
}

uint8_t k573mcal_device::comm_method_version()
{
	return 0x10;
}

uint8_t seconds = 0;
int k573mcal_device::handle_message(const uint8_t* send_buffer, uint32_t send_size, uint8_t*& recv_buffer)
{
	printf("k573mcal msg: ");
	for (uint32_t i = 0; i < send_size; i++)
		printf("%02x ", send_buffer[i]);
	printf("\n");

	switch (send_buffer[0]) {
	case 0xf0:
		// msg: f0 d9
		device_reset();
		break;

	case 0x70: {
		// msg: 70
		// Writes to RTC chip

		system_time systime;
		machine().base_datetime(systime);

		uint8_t resp[] = {
			0x01, // status, must be 1
			uint8_t(systime.local_time.year % 100),
			uint8_t(systime.local_time.month + 1),
			systime.local_time.mday,
			systime.local_time.weekday,
			systime.local_time.hour,
			systime.local_time.minute,
			seconds // Can't be the same value twice in a row
		};

		seconds = (seconds + 1) % 60;

		memcpy(recv_buffer, resp, sizeof(resp));
		recv_buffer += sizeof(resp);
		return 1;
	}

	case 0x71: {
		// msg: 71 ff ff 01

		uint8_t resp[] = {
			0x01, // status, must be 1
			m_in1->read() & 0x0f, // Area specification
		};

		memcpy(recv_buffer, resp, sizeof(resp));
		recv_buffer += sizeof(resp);
		return 4;
	}

	case 0x7e:
		// This builds some buffer that creates data like this: @2B0001:020304050607:BC9A78563412:000000000000B5
		// 2B0001 is ???
		// 020304050607 is the machine SID
		// BC9A78563412 is the machine XID
		// 000000000000B5 is ???

		// msg: 7e xx
		uint8_t resp[] = {
			// 0x01 - Breaks loop, sends next byte
			// 0x04 - Resends byte
			0x01,
		};

		memcpy(recv_buffer, resp, sizeof(resp));
		recv_buffer += sizeof(resp);

		return 2;
	}

	// Command not recognized, pass it off to the base message handler
	return jvs_device::handle_message(send_buffer, send_size, recv_buffer);
}

INPUT_PORTS_START( k573mcal )
	PORT_START("IN1")
	PORT_DIPNAME(0x0f, 0x00, "Area")
	PORT_DIPSETTING(0x00, "JA")
	PORT_DIPSETTING(0x01, "UA")
	PORT_DIPSETTING(0x02, "EA")
	PORT_DIPSETTING(0x03, "3")
	PORT_DIPSETTING(0x04, "AA")
	PORT_DIPSETTING(0x05, "KA")
	PORT_DIPSETTING(0x06, "JY")
	PORT_DIPSETTING(0x07, "JR")
	PORT_DIPSETTING(0x08, "JB")
	PORT_DIPSETTING(0x09, "UB")
	PORT_DIPSETTING(0x0a, "EB")
	PORT_DIPSETTING(0x0b, "11")
	PORT_DIPSETTING(0x0c, "AB")
	PORT_DIPSETTING(0x0d, "KB")
	PORT_DIPSETTING(0x0e, "JZ")
	PORT_DIPSETTING(0x0f, "JS")
INPUT_PORTS_END

ioport_constructor k573mcal_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(k573mcal);
}

DEFINE_DEVICE_TYPE(KONAMI_573_MASTER_CALENDAR, k573mcal_device, "k573mcal", "Konami 573 Master Calendar")
