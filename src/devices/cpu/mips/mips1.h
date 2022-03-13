// license:BSD-3-Clause
// copyright-holders:Aaron Giles, Patrick Mackinlay

#ifndef MAME_CPU_MIPS_MIPS1_H
#define MAME_CPU_MIPS_MIPS1_H

#pragma once

#include "machine/ram.h"

class mips1core_device_base : public cpu_device
{
public:
	// device configuration
	void set_endianness(endianness_t endianness) { m_endianness = endianness; }

	// input lines
	template <unsigned Coprocessor> auto in_brcond() { return m_in_brcond[Coprocessor].bind(); }
	void berr_w(int state) { m_bus_error = bool(state); }

protected:
	mips1core_device_base(machine_config const &mconfig, device_type type, char const *tag, device_t *owner, u32 clock, u32 cpurev, size_t icache_size, size_t dcache_size);

	enum registers : unsigned
	{
		MIPS1_R0   = 0,
		MIPS1_COP0 = 32,
		MIPS1_F0   = 64,

		MIPS1_PC   = 80,
		MIPS1_HI,
		MIPS1_LO,
		MIPS1_FCR30,
		MIPS1_FCR31,
	};

	enum exception : u32
	{
		EXCEPTION_INTERRUPT = 0x00000000,
		EXCEPTION_TLBMOD    = 0x00000004,
		EXCEPTION_TLBLOAD   = 0x00000008,
		EXCEPTION_TLBSTORE  = 0x0000000c,
		EXCEPTION_ADDRLOAD  = 0x00000010,
		EXCEPTION_ADDRSTORE = 0x00000014,
		EXCEPTION_BUSINST   = 0x00000018,
		EXCEPTION_BUSDATA   = 0x0000001c,
		EXCEPTION_SYSCALL   = 0x00000020,
		EXCEPTION_BREAK     = 0x00000024,
		EXCEPTION_INVALIDOP = 0x00000028,
		EXCEPTION_BADCOP    = 0x0000002c,
		EXCEPTION_OVERFLOW  = 0x00000030,
		EXCEPTION_TRAP      = 0x00000034,

		EXCEPTION_BADCOP0   = 0x0000002c,
		EXCEPTION_BADCOP1   = 0x1000002c,
		EXCEPTION_BADCOP2   = 0x2000002c,
		EXCEPTION_BADCOP3   = 0x3000002c,
	};

	enum cop0_reg : u8
	{
		COP0_Index    = 0,
		COP0_Random   = 1,
		COP0_EntryLo  = 2,
		COP0_BusCtrl  = 2,  // r3041 only
		COP0_Config   = 3,  // r3041/r3071/r3081 only
		COP0_Context  = 4,
		COP0_BadVAddr = 8,
		COP0_Count    = 9,  // r3041 only
		COP0_EntryHi  = 10,
		COP0_PortSize = 10, // r3041 only
		COP0_Compare  = 11, // r3041 only
		COP0_Status   = 12,
		COP0_Cause    = 13,
		COP0_EPC      = 14,
		COP0_PRId     = 15,
	};

	enum sr_mask : u32
	{
		SR_IEc   = 0x00000001, // interrupt enable (current)
		SR_KUc   = 0x00000002, // user mode (current)
		SR_IEp   = 0x00000004, // interrupt enable (previous)
		SR_KUp   = 0x00000008, // user mode (previous)
		SR_IEo   = 0x00000010, // interrupt enable (old)
		SR_KUo   = 0x00000020, // user mode (old)
		SR_IMSW0 = 0x00000100, // software interrupt 0 enable
		SR_IMSW1 = 0x00000200, // software interrupt 1 enable
		SR_IMEX0 = 0x00000400, // external interrupt 0 enable
		SR_IMEX1 = 0x00000800, // external interrupt 1 enable
		SR_IMEX2 = 0x00001000, // external interrupt 2 enable
		SR_IMEX3 = 0x00002000, // external interrupt 3 enable
		SR_IMEX4 = 0x00004000, // external interrupt 4 enable
		SR_IMEX5 = 0x00008000, // external interrupt 5 enable
		SR_IsC   = 0x00010000, // isolate (data) cache
		SR_SwC   = 0x00020000, // swap caches
		SR_PZ    = 0x00040000, // cache parity zero
		SR_CM    = 0x00080000, // cache match
		SR_PE    = 0x00100000, // cache parity error
		SR_TS    = 0x00200000, // tlb shutdown
		SR_BEV   = 0x00400000, // boot exception vectors
		SR_RE    = 0x02000000, // reverse endianness in user mode
		SR_COP0  = 0x10000000, // coprocessor 0 usable
		SR_COP1  = 0x20000000, // coprocessor 1 usable
		SR_COP2  = 0x40000000, // coprocessor 2 usable
		SR_COP3  = 0x80000000, // coprocessor 3 usable

		SR_KUIE   = 0x0000003f, // all interrupt enable and user mode bits
		SR_KUIEpc = 0x0000000f, // previous and current interrupt enable and user mode bits
		SR_KUIEop = 0x0000003c, // old and previous interrupt enable and user mode bits
		SR_IM     = 0x0000ff00, // all interrupt mask bits
	};

	enum cause_mask : u32
	{
		CAUSE_EXCCODE = 0x0000007c, // exception code
		CAUSE_IPSW0   = 0x00000100, // software interrupt 0 pending
		CAUSE_IPSW1   = 0x00000200, // software interrupt 1 pending
		CAUSE_IPEX0   = 0x00000400, // external interrupt 0 pending
		CAUSE_IPEX1   = 0x00000800, // external interrupt 1 pending
		CAUSE_IPEX2   = 0x00001000, // external interrupt 2 pending
		CAUSE_IPEX3   = 0x00002000, // external interrupt 3 pending
		CAUSE_IPEX4   = 0x00004000, // external interrupt 4 pending
		CAUSE_IPEX5   = 0x00008000, // external interrupt 5 pending
		CAUSE_IP      = 0x0000ff00, // interrupt pending
		CAUSE_CE      = 0x30000000, // co-processor error
		CAUSE_BD      = 0x80000000, // branch delay

		CAUSE_IPEX    = 0x0000fc00, // external interrupt pending
	};

	enum entryhi_mask : u32
	{
		EH_VPN  = 0xfffff000, // virtual page number
		EH_ASID = 0x00000fc0, // address space identifier

		EH_WM   = 0xffffffc0, // write mask
	};
	enum entrylo_mask : u32
	{
		EL_PFN = 0xfffff000, // physical frame
		EL_N   = 0x00000800, // noncacheable
		EL_D   = 0x00000400, // dirty
		EL_V   = 0x00000200, // valid
		EL_G   = 0x00000100, // global

		EL_WM  = 0xffffff00, // write mask
	};
	enum context_mask : u32
	{
		PTE_BASE = 0xffe00000, // base address of page table
		BAD_VPN  = 0x001ffffc, // virtual address bits 30..12
	};

	// device_t overrides
	virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;
	virtual void device_reset() override;

	// device_execute_interface overrides
	virtual u32 execute_min_cycles() const noexcept override { return 1; }
	virtual u32 execute_max_cycles() const noexcept override { return 40; }
	virtual u32 execute_input_lines() const noexcept override { return 6; }
	virtual void execute_run() override;
	virtual void execute_set_input(int inputnum, int state) override;

	// device_memory_interface overrides
	virtual space_config_vector memory_space_config() const override;
	virtual bool memory_translate(int spacenum, int intention, offs_t &address) override;

	// device_disasm_interface overrides
	virtual std::unique_ptr<util::disasm_interface> create_disassembler() override;

	void icache_map(address_map &map);
	void dcache_map(address_map &map);

	// exceptions
	void generate_exception(u32 exception, bool refill = false);
	void address_error(int intention, u32 const address);

	// cop0
	virtual void handle_cop0(u32 const op);
	virtual u32 get_cop0_reg(unsigned const reg);
	virtual void set_cop0_reg(unsigned const reg, u32 const data);

	// other coprocessors
	virtual void handle_cop1(u32 const op);
	virtual void handle_cop2(u32 const op);
	virtual void handle_cop3(u32 const op);

	// load/store left/right opcodes
	void lwl(u32 const op);
	void lwr(u32 const op);
	void swl(u32 const op);
	void swr(u32 const op);

	// memory accessors
	template <typename T, bool Aligned = true, typename U> std::enable_if_t<std::is_convertible<U, std::function<void(T)>>::value, void> load(u32 address, U &&apply);
	template <typename T, bool Aligned = true, typename U> std::enable_if_t<std::is_convertible<U, T>::value, void> store(u32 address, U data, T mem_mask = ~T(0));
	bool fetch(u32 address, std::function<void(u32)> &&apply);

	// debug helpers
	std::string debug_string(u32 string_pointer, unsigned const limit = 0);
	std::string debug_string_array(u32 array_pointer);

	// address spaces
	address_space_config const m_program_config_be;
	address_space_config const m_program_config_le;
	address_space_config const m_icache_config;
	address_space_config const m_dcache_config;

	int m_data_spacenum;

	// configuration
	u32 const m_cpurev;
	endianness_t m_endianness;

	// core registers
	u32 m_pc;
	u32 m_r[32];
	u32 m_hi;
	u32 m_lo;

	// cop0 registers
	u32 m_cop0[32];

	// internal stuff
	int m_icount;
	enum branch_state : unsigned
	{
		NONE      = 0,
		DELAY     = 1, // delay slot instruction active
		BRANCH    = 2, // branch instruction active
		EXCEPTION = 3, // exception triggered
	}
	m_branch_state;
	u32 m_branch_target;

	// cache memory
	size_t const m_icache_size;
	size_t const m_dcache_size;

	// I/O
	devcb_read_line::array<4> m_in_brcond;
	bool m_bus_error;
};

class mips1_device_base : public mips1core_device_base
{
public:
	// floating point coprocessor revision numbers recognised by RISC/os 4.52 and IRIX
	enum fpu_rev : u32
	{
		MIPS_R2360    = 0x0100, // MIPS R2360 Floating Point Board
		MIPS_R2010    = 0x0200, // MIPS R2010 VLSI Floating Point Chip
		MIPS_R2010A   = 0x0310, // MIPS R2010A VLSI Floating Point Chip
		MIPS_R3010    = 0x0320, // MIPS R3010 VLSI Floating Point Chip
		MIPS_R3010A   = 0x0330, // MIPS R3010A VLSI Floating Point Chip
		MIPS_R3010Av4 = 0x0340, // MIPS R3010A VLSI Floating Point Chip
		MIPS_R6010    = 0x0400, // MIPS R6010 Floating Point Chip
	};

	void set_fpu(u32 revision, unsigned interrupt = 3) { m_fcr0 = revision; m_fpu_irq = interrupt; }

protected:
	mips1_device_base(machine_config const &mconfig, device_type type, char const *tag, device_t *owner, u32 clock, u32 cpurev, size_t icache_size, size_t dcache_size);

	enum cp1_fcr31_mask : u32
	{
		FCR31_RM = 0x00000003, // rounding mode

		FCR31_FI = 0x00000004, // inexact operation flag
		FCR31_FU = 0x00000008, // underflow flag
		FCR31_FO = 0x00000010, // overflow flag
		FCR31_FZ = 0x00000020, // divide by zero flag
		FCR31_FV = 0x00000040, // invalid operation flag

		FCR31_EI = 0x00000080, // inexact operation enable
		FCR31_EU = 0x00000100, // underflow enable
		FCR31_EO = 0x00000200, // overflow enable
		FCR31_EZ = 0x00000400, // divide by zero enable
		FCR31_EV = 0x00000800, // invalid operation enable

		FCR31_CI = 0x00001000, // inexact operation cause
		FCR31_CU = 0x00002000, // underflow cause
		FCR31_CO = 0x00004000, // overflow cause
		FCR31_CZ = 0x00008000, // divide by zero cause
		FCR31_CV = 0x00010000, // invalid operation cause
		FCR31_CE = 0x00020000, // unimplemented operation cause

		FCR31_C  = 0x00800000, // condition

		FCR31_FM = 0x0000007c, // flag mask
		FCR31_EM = 0x00000f80, // enable mask
		FCR31_CM = 0x0001f000, // cause mask (except unimplemented)
	};

	// device_t overrides
	virtual void device_start() override;
	virtual void device_reset() override;

	// device_memory_interface overrides
	virtual bool memory_translate(int spacenum, int intention, offs_t &address) override;

	virtual void handle_cop0(u32 const op) override;
	virtual u32 get_cop0_reg(unsigned const reg) override;
	virtual void set_cop0_reg(unsigned const reg, u32 const data) override;

	virtual void handle_cop1(u32 const op) override;
	template <typename T> void set_cop1_reg(unsigned const reg, T const data);

private:
	u64 m_reset_time;
	u32 m_tlb[64][2]; // 0 is hi, 1 is lo
	unsigned m_tlb_mru[3][64];

	// cop1 registers
	u64 m_f[16];
	u32 m_fcr0;
	u32 m_fcr30;
	u32 m_fcr31;

	unsigned m_fpu_irq;
};

class r2000_device : public mips1_device_base
{
public:
	r2000_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock, size_t icache_size = 0, size_t dcache_size = 0);
};

class r2000a_device : public mips1_device_base
{
public:
	r2000a_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock, size_t icache_size = 0, size_t dcache_size = 0);
};

class r3000_device : public mips1_device_base
{
public:
	r3000_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock, size_t icache_size = 0, size_t dcache_size = 0);
};

class r3000a_device : public mips1_device_base
{
public:
	r3000a_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock, size_t icache_size = 0, size_t dcache_size = 0);
};

class r3041_device : public mips1core_device_base
{
public:
	r3041_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;
};

class r3051_device : public mips1core_device_base
{
public:
	r3051_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock);
};

class r3052_device : public mips1core_device_base
{
public:
	r3052_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock);
};

class r3052e_device : public mips1_device_base
{
public:
	r3052e_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock);
};

class r3071_device : public mips1_device_base
{
public:
	r3071_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock, size_t icache_size = 16384, size_t dcache_size = 4096);
};

class r3081_device : public mips1_device_base
{
public:
	r3081_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock, size_t icache_size = 16384, size_t dcache_size = 4096);
};

class iop_device : public mips1core_device_base
{
public:
	iop_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock);
};

#include "diserial.h"
class tx3927_sio : public device_t,
	public device_serial_interface
{
public:
	// construction/destruction
	tx3927_sio(const machine_config& mconfig, const char* tag, device_t* owner, uint32_t clock);

	// configuration helpers
	auto irq_handler() { return m_irq_handler.bind(); }
	auto txd_handler() { return m_txd_handler.bind(); }
	auto dtr_handler() { return m_dtr_handler.bind(); }
	auto rts_handler() { return m_rts_handler.bind(); }

	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_resolve_objects() override;

	virtual void device_timer(emu_timer& timer, device_timer_id id, int param) override;

	DECLARE_WRITE_LINE_MEMBER(write_rxd);
	DECLARE_WRITE_LINE_MEMBER(write_cts);

	uint32_t read(offs_t offset, uint32_t mem_mask);
	void write(offs_t offset, uint32_t data, uint32_t mem_mask);

private:
	enum : u32 {
		SIFCR_SWRST = 15, // Software Reset
		SIFCR_RDIL = 7, // Receive FIFO Request Trigger Level
		SIFCR_TDIL = 3, // Transmit FIFO Request Trigger Level
		SIFCR_TFRST = 2, // Transmit FIFO Reset
		SIFCR_RFRST = 1, // Receive FIFO Reset
		SIFCR_FRSTE = 0, // FIFO Reset Enable

		SIDISR_UBRK = 15, // UART Break Reception
		SIDISR_UVALID = 14, // UART Receiver FIFO Available Status
		SIDISR_UFER = 13, // UART Frame Error
		SIDISR_UPER = 12, // UART Parity Error
		SIDISR_UOER = 11, // UART Overrun Error
		SIDISR_ERI = 10, // Error Interrupt
		SIDISR_TOUT = 9, // Receive Timeout
		SIDISR_TDIS = 8, // Transmit Data Empty
		SIDISR_RDIS = 7, // Receive Data Full
		SIDISR_STIS = 6, // Status Change Interrupt status

		SIDICR_TDE = 15, // Transmit DMA Enable (SITXDREQ)
		SIDICR_RDE = 14, // Receive DMA Enable (SIRXDREQ)
		SIDICR_TIE = 13, // Transmit Data Interrupt Enable (SITXIREQ)
		SIDICR_RIE = 12, // Receive Data Interrupt Enable (SIRXIREQ)
		SIDICR_SPIE = 11, // Special Receive Interrupt Enable (SISPIREQ)
		SIDICR_CTSAC = 9, // CTSS Active Condition (2 bits)
		SIDICR_STIE_OERS = 5,
		SIDICR_STIE_CTSAC = 4,
		SIDICR_STIE_RBRKD = 3,
		SIDICR_STIE_TRDY = 2,
		SIDICR_STIE_TXALS = 1,
		SIDICR_STIE_UBRKD = 0,

		SIFLCR_RCS = 12, // RTS Control Select
		SIFLCR_TES = 11, // Transmit Enable Select
		SIFLCR_RTSSC = 9, // RTS Software Control
		SIFLCR_RSDE = 8, // Receive Serial Data Enable
		SIFLCR_TSDE = 7, // Transmit Serial Data Enable
		SIFLCR_RTSTL = 1, // RTS Active Trigger Level
		SIFLCR_TBRK = 0, // Break Transmit


		SISCISR_OERS = 5, // Overrun Error Status
		SISCISR_CTSS = 4, // CTS* Terminal Status
		SISCISR_RBRKD = 3, // Receive Break
		SISCISR_TRDY = 2, // Transmit Data Empty
		SISCISR_TXALS = 1, // Transmission Completed
		SISCISR_UBRKD = 0, // UART Break Detection
	};

	devcb_write_line m_irq_handler;
	devcb_write_line m_txd_handler;
	devcb_write_line m_dtr_handler;
	devcb_write_line m_rts_handler;

	void sio_timer_adjust();
	void transmit_clock(bool is_cts);
	void start_tx();

	emu_timer* m_timer;

	uint32_t m_sifcr = 0;
	uint32_t m_sidisr = 0x2000; // DMA/Interrupt Status Registers (SIDISRn)

	uint32_t m_silcr = 0;
	uint32_t m_sidicr = 0;
	uint32_t m_siscisr = 0b000110;
	uint32_t m_siflcr = 0b0000000110000010;
	uint32_t m_sibgr = 0x3ff;
	uint8_t m_sitfifo[8] = { 0 };
	uint32_t m_sitfifo_len = 0;
	uint16_t m_sirfifo[16] = { 0 };
	uint32_t m_sirfifo_len = 0;

	uint8_t m_rx_data = 0;
	uint8_t m_tx_data = 0;
	int m_data_bits_count = 0;

	int m_cts = 0;

	// Clock select values:
	// 0 = internal (IMCLK, 1/4th of 133MHz CPU clock)
	// 1 = baud rate generator (IMCLK)
	// 2 = external clock (SCLK)
	// 3 = baud rate generator (SCLK)
	int m_clock_sel = 0;

	attotime m_recv_timeout_counter = attotime::never;
	int m_transmit_bit = 0;
};

class tx3927_device : public mips1_device_base
{
public:
	tx3927_device(machine_config const& mconfig, char const* tag, device_t* owner, u32 clock, size_t icache_size = 8192, size_t dcache_size = 4096);

	void trigger_irq(int irq, int state);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_add_mconfig(machine_config& config) override;
	virtual void device_resolve_objects() override;

	virtual space_config_vector memory_space_config() const override;

	template <int N> TIMER_CALLBACK_MEMBER(update_timer);
	emu_timer* m_timer[3];

	template <int N> DECLARE_WRITE_LINE_MEMBER(ata_interrupt);
	DECLARE_WRITE_LINE_MEMBER(ata_dmarq);

	void amap(address_map& map);

	const address_space_config m_program_config;
	address_space *m_program;

private:
	required_device_array<tx3927_sio, 2> m_sio;

	void update_timer_speed();
	void update_rom_config(int idx);

	uint32_t tmr_read(offs_t offset, uint32_t mem_mask);
	void tmr_write(offs_t offset, uint32_t data, uint32_t mem_mask);

	uint32_t ccfg_read(offs_t offset, uint32_t mem_mask);
	void ccfg_write(offs_t offset, uint32_t data, uint32_t mem_mask);

	uint32_t sdram_read(offs_t offset, uint32_t mem_mask);
	void sdram_write(offs_t offset, uint32_t data, uint32_t mem_mask);

	uint32_t rom_read(offs_t offset, uint32_t mem_mask);
	void rom_write(offs_t offset, uint32_t data, uint32_t mem_mask);

	uint32_t dma_read(offs_t offset, uint32_t mem_mask);
	void dma_write(offs_t offset, uint32_t data, uint32_t mem_mask);

	uint32_t irc_read(offs_t offset, uint32_t mem_mask);
	void irc_write(offs_t offset, uint32_t data, uint32_t mem_mask);

	uint32_t pci_read(offs_t offset, uint32_t mem_mask);
	void pci_write(offs_t offset, uint32_t data, uint32_t mem_mask);

	uint32_t pio_read(offs_t offset, uint32_t mem_mask);
	void pio_write(offs_t offset, uint32_t data, uint32_t mem_mask);

	// ROM
	uint32_t m_rom_rccr[8];

	// TMR
	enum : u32 {
		TMTCR_TCE = 7, // Timer Count Enable
		TMTCR_CCDE = 6, // Counter Clock Divide Enable
		TMTCR_CRE = 5, // Counter Reset Enable
		TMTCR_ECES = 3, // External Clock Edge Select
		TMTCR_CCS = 2, // Counter Clock Select
		TMTCR_TMODE = 0, // Timer Mode

		TMITMR_TIIE = 15, // Timer Interval Interrupt Enable
		TMITMR_TZCE = 0, // Interval Timer Zero Clear Enable

		TMTISR_TWIS = 3, // Timer Watchdog Interrupt Status
		TMTISR_TPIBS = 2, // Timer Pulse Generator Interrupt by TMCPRB Status
		TMTISR_TIPAS = 1, // Timer Pulse Generator Interrupt by TMCPRA Status
		TMTISR_TIIS = 0, // Timer Interval Interrupt Status
	};

	typedef struct {
		uint32_t TMTCR;  // 0x00 Timer Control Register
		uint32_t TMTISR; // 0x04 Timer Interrupt Status Register
		uint32_t TMCPRA; // 0x08 Compare Register A
		uint32_t TMCPRB; // 0x0c Compare Register B
		uint32_t TMITMR; // 0x10 Interval Timer Mode Register
		uint32_t TMCCDR; // 0x20 Clock Divider Register
		uint32_t TMPGMR; // 0x30 Pulse Generator Mode Register
		uint32_t TMWTMR; // 0x40 Watchdog Timer Mode Register
		uint32_t TMTRR;  // 0xf0 Timer Read Register
	} TMR;
	TMR m_tmr[3] = {};

	// IRC
	enum : u32 {
		IRCSR_IF = 16, // Interrupt Flag
		IRCSR_ILV = 8, // Interrupt Level Vector
		IRCSR_IVL = 0, // Interrupt Vector
	};
	uint32_t m_irc_irssr;  // Interrupt Source Status Register
	uint32_t m_irc_irscr; // Interrupt Status/Control Register
	uint32_t m_irc_ircsr; // Interrupt Current Status Register
	uint32_t m_irc_ircer;
	uint32_t m_irc_irimr;
	uint32_t m_irc_irilr[16];
	uint32_t m_irc_irilr_full[8];
	uint32_t m_irc_ircr[16];

	// CCFG
	uint32_t m_ccfg;
	uint32_t m_crir;
	uint32_t m_pcfg;
	uint32_t m_tear;
	uint32_t m_pdcr;

	// PIO
	uint32_t m_pio_flags[64] = {};

	// PCIC
	uint32_t m_pci_istat;
	uint16_t m_pci_pcistat;
	uint16_t m_pcicmd;
	uint32_t m_pci_iba; // I/O Space Base Address
	uint32_t m_pci_mba; // Memory Base Address
	uint16_t m_pci_svid; // System Vendor ID
	uint16_t m_pci_ssvid; // Subsystem Vendor ID
	uint8_t m_pci_ml; // Maximum Latency
	uint8_t m_pci_mg; // Minimum Grant
	uint8_t m_pci_ip; // Interrupt Pin
	uint8_t m_pci_il; // Interrupt Line
	uint32_t m_ipcidata; // Initiator Indirect Data
	uint8_t m_pci_icmd; // Initiator Indirect Command
	uint8_t m_pci_ibe; // Initiator Indirect Byte Enable
	uint32_t m_pci_lbc; // Local Bus Control Register (LBC)
	uint32_t m_pci_mmas; // Initiator Memory Mapping Address Size
	uint32_t m_pci_iomas; // Initiator IO Mapping Address Size
	uint32_t m_pci_ipciaddr; // Initiator Indirect Address
	uint32_t m_pci_ipcidata; // Initiator Indirect Data
};

DECLARE_DEVICE_TYPE(TX3927_SIO,  tx3927_sio)

DECLARE_DEVICE_TYPE(R2000,       r2000_device)
DECLARE_DEVICE_TYPE(R2000A,      r2000a_device)
DECLARE_DEVICE_TYPE(R3000,       r3000_device)
DECLARE_DEVICE_TYPE(R3000A,      r3000a_device)
DECLARE_DEVICE_TYPE(R3041,       r3041_device)
DECLARE_DEVICE_TYPE(R3051,       r3051_device)
DECLARE_DEVICE_TYPE(R3052,       r3052_device)
DECLARE_DEVICE_TYPE(R3052E,      r3052e_device)
DECLARE_DEVICE_TYPE(R3071,       r3071_device)
DECLARE_DEVICE_TYPE(R3081,       r3081_device)
DECLARE_DEVICE_TYPE(SONYPS2_IOP, iop_device)
DECLARE_DEVICE_TYPE(TX3927,      tx3927_device)

#endif // MAME_CPU_MIPS_MIPS1_H
