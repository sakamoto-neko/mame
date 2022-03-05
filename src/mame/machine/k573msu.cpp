// license:BSD-3-Clause
// copyright-holders:smf
/*
 * Konami 573 Multi Session Unit
 *
 */

#include <iostream>

#include "emu.h"
#include "k573msu.h"
#include "bus/rs232/rs232.h"

#define LOG_GENERAL  (1 << 0)
#define VERBOSE      (LOG_GENERAL)
#define LOG_OUTPUT_STREAM std::cout

#define LOGGENERAL(...)        LOGMASKED(LOG_GENERAL, __VA_ARGS__)

#include "logmacro.h"

/*

  PCB Layout of External Multisession Box
  ---------------------------------------

  GXA25-PWB(A)(C)2000 KONAMI
  |--------------------------------------------------------------------------|
  |CN9  ADM232  LS273        PC16552          PC16552         XC9536(1)  CN13|
  |DSW(8)  LS245   LS273            18.432MHz                        DS2401  |
  |LEDX16   |-------|      |-------|       |-------|      |-------|          |
  | MB3793  |TOSHIBA|      |TOSHIBA|       |TOSHIBA|      |TOSHIBA|M48T58Y.6T|
  |         |TC9446F|      |TC9446F|       |TC9446F|      |TC9446F|          |
  |         |-016   |      |-016   |       |-016   |      |-016   |      CN12|
  |         |-------|      |-------|       |-------|      |-------|          |
  |       LV14                    XC9572XL                                   |
  | CN16                 CN17                 CN18             CN19 XC9536(2)|
  |PQ30RV21        LCX245   LCX245                                       CN11|
  |                                  33.8688MHz              PQ30RV21        |
  |    8.25MHz   HY57V641620                                                 |
  |  |------------|     HY57V641620   XC2S200                                |
  |  |TOSHIBA     |                                          FLASH.20T       |
  |  |TMPR3927AF  |                                                      CN10|
  |  |            |                                                          |
  |  |            |                                     LS245   F245  F245   |
  |  |            |HY57V641620  LCX245     DIP40                             |
  |  |------------|     HY57V641620  LCX245                   ATAPI44        |
  |                             LCX245              LED(HDD)  ATAPI40        |
  |    CN7                      LCX245      CN14    LED(CD)           CN5    |
  |--------------------------------------------------------------------------|
  Notes: (all IC's shown)
          TMPR3927     - Toshiba TMPR3927AF Risc Microprocessor (QFP240)
          FLASH.20T    - Fujitsu 29F400TC Flash ROM (TSOP48)
          ATAPI44      - IDE44 44-pin laptop type HDD connector (not used)
          ATAPI40      - IDE40 40-pin flat cable HDD connector used for connection of CDROM drive
          XC9572XL     - XILINX XC9572XL In-system Programmable CPLD stamped 'XA25A1' (TQFP100)
          XC9536(1)    - XILINX CPLD stamped 'XA25A3' (PLCC44)
          XC9536(2)    - XILINX CPLD stamped 'XA25A2' (PLCC44)
          XC2S200      - XILINX XC2S200 SPARTAN FPGA (QFP208)
          DS2401       - MAXIM Dallas DS2401 Silicon Serial Number (SOIC6)
          M48T58Y      - ST M48T58Y Timekeeper NVRAM 8k bytes x8-bit (DIP28). Chip appears empty (0x04 fill) or unused
          MB3793       - Fujitsu MB3793 Power-Voltage Monitoring IC with Watchdog Timer (SOIC8)
          DIP40        - Empty DIP40 socket
          HY57V641620  - Hyundai/Hynix HY57V641620 4 Banks x 1M x 16Bit Synchronous DRAM
          PC16552D     - National PC16552D Dual Universal Asynchronous Receiver/Transmitter with FIFO's
          TC9446F      - Toshiba TC9446F-016 Audio Digital Processor for Decode of Dolby Digital (AC-3) MPEG2 Audio
          CN16-CN19    - Connector for sub board (3 of them are present). One board connects via a thin cable from
                         CN1 to the main board to a connector on the security board labelled 'AMP BOX'.

  Sub Board Layout
  ----------------

  GXA25-PWB(B) (C) 2000 KONAMI
  |-------------------|  |----------|
  | TLP2630  LV14     |__| ADM232   |
  |CN2                           CN1|
  |A2430         AK5330             |
  |                          RCA L/R|
  |                          RCA L/R|
  |ZUS1R50505   6379A  __           |
  |                   |  |   LM358  |
  |-------------------|  |----------|



  Notes:
  CPU IRQs
	IRQ handler is called by calculating irq_handlers[(cause >> 8) & 0x3c](...)
	IRQ 0 = HDD interrupt (= ?)
	IRQ 1 = CD-ROM interrupt (= IRQ 0)
	IRQ 4 = Serial/RS232 interrupt (= IRQ 2)
	IRQ 5 = DSP interrupt (= IRQ 3)
	IRQ 13 = Timer interrupt (= IRQ 0 + IRQ 2 + IRQ 3)
*/

k573msu_device::k573msu_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, KONAMI_573_MULTI_SESSION_UNIT, tag, owner, clock),
	digital_id(*this, "digital_id"),
	m_maincpu(*this, "maincpu"),
	m_ram(*this, "ram"),
	m_duart_com(*this, "duart_com_%d", 0L),
	m_ata_cdrom(*this, "ata_cdrom")
{
}

static void k573msu_ata_devices(device_slot_interface& device)
{
	device.option_add("cdrom", ATAPI_FIXED_CDROM);
}

void k573msu_device::device_add_mconfig(machine_config &config)
{
	TX3927(config, m_maincpu, 20_MHz_XTAL);
	m_maincpu->set_addrmap(AS_PROGRAM, &k573msu_device::amap);
	m_maincpu->in_brcond<0>().set([]() { return 1; }); // writeback complete
	m_maincpu->in_brcond<1>().set([]() { return 1; }); // writeback complete
	m_maincpu->in_brcond<2>().set([]() { return 1; }); // writeback complete
	m_maincpu->in_brcond<3>().set([]() { return 1; }); // writeback complete
	m_maincpu->timer_callback().set(FUNC(k573msu_device::timer_interrupt));

	RAM(config, m_ram).set_default_size("32M").set_default_value(0);

	ATA_INTERFACE(config, m_ata_cdrom).options(k573msu_ata_devices, "cdrom", nullptr, true);
	m_ata_cdrom->irq_handler().set(FUNC(k573msu_device::ata_interrupt<0>));
	m_ata_cdrom->slot(0).set_fixed(true);

	M48T58(config, "m48t58y", 0);

	DS2401(config, digital_id);

	// Serial for the 4 subboards
	PC16552D(config, "duart_com_0", 0);
	auto& duart_chan0(NS16550(config, "duart_com_0:chan1", 18.432_MHz_XTAL));
	duart_chan0.out_int_callback().set(FUNC(k573msu_device::serial_interrupt<0>));

	auto& duart_chan1(NS16550(config, "duart_com_0:chan0", 18.432_MHz_XTAL));
	duart_chan1.out_int_callback().set(FUNC(k573msu_device::serial_interrupt<1>));

	PC16552D(config, "duart_com_1", 0);
	auto& duart_chan2(NS16550(config, "duart_com_1:chan1", 18.432_MHz_XTAL));
	duart_chan2.out_int_callback().set(FUNC(k573msu_device::serial_interrupt<2>));

	auto& duart_chan3(NS16550(config, "duart_com_1:chan0", 18.432_MHz_XTAL));
	duart_chan3.out_int_callback().set(FUNC(k573msu_device::serial_interrupt<3>));
}

void k573msu_device::device_reset()
{
	dsp_data_cnt = 0;
	m_fpgasoft_unk = 0;
	std::fill(std::begin(m_fpgasoft_transfer_len), std::end(m_fpgasoft_transfer_len), 0);
}

void k573msu_device::device_start()
{
}

template<unsigned N>
void k573msu_device::ata_interrupt(int state)
{
	//LOGGENERAL("ata_interrupt<%d> %d\n", N, state);

	m_maincpu->set_input_line(INPUT_LINE_IRQ0 + N, state);
}

template<unsigned N>
void k573msu_device::serial_interrupt(int state)
{
	//LOGGENERAL("serial_interrupt<%d> %d\n", N, state);

	m_maincpu->set_input_line(INPUT_LINE_IRQ2, state);
}

template<unsigned N>
void k573msu_device::dsp_interrupt(int state)
{
	//LOGGENERAL("dsp_interrupt %d\n", state);

	m_maincpu->set_input_line(INPUT_LINE_IRQ3, state);
}

void k573msu_device::timer_interrupt(int state)
{
	//LOGGENERAL("timer_interrupt %d\n", state);

	m_maincpu->set_input_line(INPUT_LINE_IRQ0, state);
	m_maincpu->set_input_line(INPUT_LINE_IRQ2, state);
	m_maincpu->set_input_line(INPUT_LINE_IRQ3, state);
}

uint16_t k573msu_device::fpgasoft_read(offs_t offset, uint16_t mem_mask)
{
	if (offset != 0x10 && offset != 0x780) {
		LOGGENERAL("%s: fpgasoft_read %08x %08x\n", machine().describe_context().c_str(), offset * 2, mem_mask);
	}

	switch (offset * 2) {
	case 0x0a:
		// Some kind of flag? Seems to have 3 bits * 4 so I think it might be a combined flag for all 4 DSPs
		return m_fpgasoft_unk;

	case 0x0c:
		// 0x0c and 0x0e are length registers
		// A length is an 8-bit value. Each register packs 2 sound chips worth of data.
		// DSP 0 = 0x0c upper
		// DSP 1 = 0x0c lower
		// DSP 2 = 0x0e upper
		// DSP 3 = 0x0e lower
		return m_fpgasoft_transfer_len[0];
	case 0x0e:
		return m_fpgasoft_transfer_len[1];

	case 0x20:
		// MIACK
		// TODO: Implement ack properly
		if (dsp_data_cnt > 1) {
			dsp_data_cnt = 0;
			return 1;
		}
		return 0;

	case 0x60:
		// Something to do with IRQs for the DSP chips?
		// Setting 0x10 will trigger thread 0x0c.
		// Setting the bottom 4 bits will trigger for DSPs 0 to 3.
		// This only seems to be read when CPU IRQ 5 is triggered.
		return 0;

	case 0xe00:
		// Setting this to 2 makes the code go down a path that seems to expect a device to exist on the serial port.
		return 2;

	case 0xf00:
		// Xilinx FPGA version?
		// 5963 = XC9536?
		return 0x5963;
	}

	return 0;
}

void k573msu_device::fpgasoft_write(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	if (offset != 0x12 && offset != 0x13) {
		LOGGENERAL("%s: fpgasoft_write %08x %08x %08x\n", machine().describe_context().c_str(), offset * 2, data, mem_mask);
	}

	switch (offset * 2) {
	case 0x08:
		// Writes flags:
		// ffbf
		// ffef
		// fffb
		// fffe
		break;

	case 0x0a:
		m_fpgasoft_unk = data;
		break;

	case 0x0c:
		// 0x0c and 0x0e are length registers
		// A length is an 8-bit value. Each register packs 2 sound chips worth of data.
		// DSP 0 = 0x0c upper
		// DSP 1 = 0x0c lower
		// DSP 2 = 0x0e upper
		// DSP 3 = 0x0e lower
		m_fpgasoft_transfer_len[0] = data;
		break;
	case 0x0e:
		m_fpgasoft_transfer_len[1] = data;
		break;

	case 0x20:
		// Seems to be used to trigger broadcasts to the enabled DSP chips?
		// Writes flags:
		// fff7
		// fff3
		// fff1
		// fff0
		break;

	case 0x22:
		// ?
		break;

	case 0x24:
		// upper 8 bits of 24-bit command
		//dsp_data = (dsp_data & 0xffff) | ((data & 0xff) << 16);
		dsp_data_cnt++;
		break;

	case 0x26:
		// bottom 16 bits of 24-bit command
		//dsp_data = (dsp_data & 0xff0000) | data;
		dsp_data_cnt++;
		break;

	case 0x28:
		// MICS?
		break;

	case 0x40:
		// ?
		// 0080
		// 8080
		// c080
		break;

	case 0x4c:
		// ?
		break;

	case 0x50:
		// ?
		// 7fff
		// 5fff
		// 57ff
		// 55ff
		break;

	case 0x5a:
		// Some kind of flags?
		// 0008
		// 000c
		// 000e
		// 000f
		break;

	case 0x68:
		// ?
		break;
	}
}

uint16_t k573msu_device::fpga_read(offs_t offset, uint16_t mem_mask)
{
	if (offset == 1)
		return 1;

	LOGGENERAL("%s: fpga_read %08x %08x\n", machine().describe_context().c_str(), offset * 2, mem_mask);

	return 0;
}

void k573msu_device::fpga_write(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	if (offset != 4)
		LOGGENERAL("%s: fpga_write %08x %08x %08x\n", machine().describe_context().c_str(), offset * 2, data, mem_mask);
}

void k573msu_device::amap(address_map& map)
{
	map(0x00000000, 0x0fffffff).ram().share("ram");
	map(0x80000000, 0x8fffffff).ram().share("ram");
	map(0xa0000000, 0xafffffff).ram().share("ram");

	// Encountered unknown ranges:
	// 10340000
	// 10343000

	//map(0x10000000, 0x1000000f).rw(m_ata_hdd, FUNC(ata_interface_device::cs0_r), FUNC(ata_interface_device::cs0_w)); // HDD - unused on real hardware
	//map(0x10000080, 0x1000008f).rw(m_ata_hdd, FUNC(ata_interface_device::cs1_r), FUNC(ata_interface_device::cs1_w));
	map(0x10100000, 0x1010000f).rw(m_ata_cdrom, FUNC(ata_interface_device::cs0_r), FUNC(ata_interface_device::cs0_w)); // CD
	map(0x10100080, 0x1010008f).rw(m_ata_cdrom, FUNC(ata_interface_device::cs1_r), FUNC(ata_interface_device::cs1_w));
	map(0x10200000, 0x1020000f).rw(FUNC(k573msu_device::fpga_read), FUNC(k573msu_device::fpga_write));
	// 0x10220000 Seems to be related to the ATA drives in some way. Will write 1 for ata[0], 4 for ata[1], and 5 for both?
	map(0x10240004, 0x10240007).portr("IN1").nopw(); // write = LEDx16 near dipsw?
	// 0x10260000 might be related to 0x10220000???
	map(0x10300000, 0x1030001f).rw(m_duart_com[0], FUNC(pc16552_device::read), FUNC(pc16552_device::write)).umask16(0xff);
	map(0x10320000, 0x1032001f).rw(m_duart_com[1], FUNC(pc16552_device::read), FUNC(pc16552_device::write)).umask16(0xff);
	map(0x10400000, 0x10400fff).rw(FUNC(k573msu_device::fpgasoft_read), FUNC(k573msu_device::fpgasoft_write));

	map(0x1f400800, 0x1f400bff).rw("m48t58y", FUNC(timekeeper_device::read), FUNC(timekeeper_device::write)).umask32(0x00ff00ff);
	map(0x1fc00000, 0x1fc7ffff).rom().region("tmpr3927", 0);
}

static INPUT_PORTS_START( k573msu )
	PORT_START( "IN1" )
	PORT_BIT(0xff00ffff, IP_ACTIVE_LOW, IPT_UNKNOWN)
	PORT_DIPUNKNOWN_DIPLOC( 0x00010000, 0x00010000, "SW:1" )
	PORT_DIPUNKNOWN_DIPLOC( 0x00020000, 0x00020000, "SW:2" )
	PORT_DIPUNKNOWN_DIPLOC( 0x00040000, 0x00040000, "SW:3" )
	PORT_DIPUNKNOWN_DIPLOC( 0x00080000, 0x00080000, "SW:4" )
	PORT_DIPNAME( 0x00100000, 0x00100000, "Start Up Device" ) PORT_DIPLOCATION( "DIP SW:5" )
	PORT_DIPSETTING(          0x00100000, "CD-ROM Drive" )
	PORT_DIPSETTING(          0x00000000, "Hard Drive" )
	PORT_DIPUNKNOWN_DIPLOC( 0x00200000, 0x00200000, "SW:6" )
	PORT_DIPUNKNOWN_DIPLOC( 0x00400000, 0x00400000, "SW:7" )
	PORT_DIPUNKNOWN_DIPLOC( 0x00800000, 0x00800000, "SW:8" )
INPUT_PORTS_END

ROM_START( k573msu )
	ROM_REGION32_BE( 0x080000, "tmpr3927", 0 )
	ROM_LOAD16_WORD_SWAP( "flash.20t", 0x000000, 0x080000, CRC(b70c65b0) SHA1(d3b2bf9d3f8b1caf70755a0d7fa50ef8bbd758b8) ) // from "GXA25-PWB(A)(C)2000 KONAMI"

	ROM_REGION( 0x002000, "m48t58y", 0 )
	ROM_LOAD( "m48t58y.6t",   0x000000, 0x002000, CRC(609ef020) SHA1(71b87c8b25b9613b4d4511c53d0a3a3aacf1499d) )

	ROM_REGION( 0x000008, "digital_id", 0 )
	ROM_LOAD( "digital-id.bin",   0x000000, 0x000008, CRC(2b977f4d) SHA1(2b108a56653f91cb3351718c45dfcf979bc35ef1) )
ROM_END

const tiny_rom_entry* k573msu_device::device_rom_region() const
{
	return ROM_NAME(k573msu);
}

ioport_constructor k573msu_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(k573msu);
}

DEFINE_DEVICE_TYPE(KONAMI_573_MULTI_SESSION_UNIT, k573msu_device, "k573msu", "Konami 573 Multi Session Unit")
