// license:BSD-3-Clause
// copyright-holders:windyfairy
/*
 * Konami Windy2 I/O (JVS)
 *
 */
#ifndef MAME_MACHINE_KWINDY2_H
#define MAME_MACHINE_KWINDY2_H

#pragma once

#include "machine/jvsdev.h"
#include "machine/timer.h"

class kwindy2_device : public jvs_device
{
public:
	template <typename T>
	kwindy2_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock, T &&jvs_host_tag)
		: kwindy2_device(mconfig, tag, owner, clock)
	{
		host.set_tag(std::forward<T>(jvs_host_tag));
	}

	kwindy2_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	virtual ioport_constructor device_input_ports() const override;

	DECLARE_INPUT_CHANGED_MEMBER(coin_inserted);

protected:
	template <uint8_t First> void set_port_tags() { }

	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_add_mconfig(machine_config &config) override;

	// JVS device overrides
	virtual const char *device_id() override;
	virtual uint8_t command_format_version() override;
	virtual uint8_t jvs_standard_version() override;
	virtual uint8_t comm_method_version() override;
	virtual void function_list(uint8_t*& buf) override;
	virtual bool coin_counters(uint8_t*& buf, uint8_t count) override;
	virtual bool switches(uint8_t*& buf, uint8_t count_players, uint8_t bytes_per_switch) override;
	virtual int handle_message(const uint8_t *send_buffer, uint32_t send_size, uint8_t *&recv_buffer) override;

private:
	required_ioport m_in1;
	required_ioport m_test_port;
	required_ioport_array<2> m_player_ports;

	int m_coin_counter[2];
};

DECLARE_DEVICE_TYPE(KONAMI_WINDY2_JVS_IO, kwindy2_device)

#endif // MAME_MACHINE_KWINDY2_H
