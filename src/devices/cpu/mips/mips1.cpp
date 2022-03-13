// license:BSD-3-Clause
// copyright-holders:Aaron Giles, Patrick Mackinlay

/*
 * MIPS-I emulation, including R2000[A], R3000[A] and IDT R30xx devices. The
 * IDT devices come in two variations: those with an "E" suffix include a TLB,
 * while those without have hard-wired address translation.
 *
 * TODO
 *   - R3041 features
 *   - cache emulation
 *
 */

#include "emu.h"
#include "mips1.h"
#include "mips1dsm.h"
#include "debugger.h"
#include "softfloat3/source/include/softfloat.h"

#define LOG_GENERAL (1U << 0)
#define LOG_TLB     (1U << 1)
#define LOG_IOP     (1U << 2)
#define LOG_RISCOS  (1U << 3)

#define LOG_TX39_TMR (1U << 4)
#define LOG_TX39_SIO (1U << 5)
#define LOG_TX39_IRC (1U << 6)
#define LOG_TX39_CCFG (1U << 7)
#define LOG_TX39_SDRAM (1U << 8)
#define LOG_TX39_ROM (1U << 9)
#define LOG_TX39_DMA (1U << 10)
#define LOG_TX39_PCI (1U << 11)
#define LOG_TX39_PIO (1U << 12)

#include <iostream>

//#define VERBOSE     (LOG_GENERAL|LOG_TLB)
//#define VERBOSE (LOG_TX39_SIO)
//#define VERBOSE (LOG_TX39_TMR|LOG_TX39_SIO|LOG_TX39_IRC|LOG_TX39_CCFG|LOG_TX39_SDRAM|LOG_TX39_ROM|LOG_TX39_DMA|LOG_TX39_PCI|LOG_TX39_PIO)
#define LOG_OUTPUT_STREAM std::cout

#include "logmacro.h"

#define RSREG           ((op >> 21) & 31)
#define RTREG           ((op >> 16) & 31)
#define RDREG           ((op >> 11) & 31)
#define SHIFT           ((op >> 6) & 31)

#define FTREG           ((op >> 16) & 31)
#define FSREG           ((op >> 11) & 31)
#define FDREG           ((op >> 6) & 31)

#define SIMMVAL         s16(op)
#define UIMMVAL         u16(op)
#define LIMMVAL         (op & 0x03ffffff)

#define SR              m_cop0[COP0_Status]
#define CAUSE           m_cop0[COP0_Cause]

DEFINE_DEVICE_TYPE(R2000,       r2000_device,     "r2000",   "MIPS R2000")
DEFINE_DEVICE_TYPE(R2000A,      r2000a_device,    "r2000a",  "MIPS R2000A")
DEFINE_DEVICE_TYPE(R3000,       r3000_device,     "r3000",   "MIPS R3000")
DEFINE_DEVICE_TYPE(R3000A,      r3000a_device,    "r3000a",  "MIPS R3000A")
DEFINE_DEVICE_TYPE(R3041,       r3041_device,     "r3041",   "IDT R3041")
DEFINE_DEVICE_TYPE(R3051,       r3051_device,     "r3051",   "IDT R3051")
DEFINE_DEVICE_TYPE(R3052,       r3052_device,     "r3052",   "IDT R3052")
DEFINE_DEVICE_TYPE(R3052E,      r3052e_device,    "r3052e",  "IDT R3052E")
DEFINE_DEVICE_TYPE(R3071,       r3071_device,     "r3071",   "IDT R3071")
DEFINE_DEVICE_TYPE(R3081,       r3081_device,     "r3081",   "IDT R3081")
DEFINE_DEVICE_TYPE(SONYPS2_IOP, iop_device,       "sonyiop", "Sony Playstation 2 IOP")
DEFINE_DEVICE_TYPE(TX3927,	    tx3927_device,    "tx3927",  "Toshiba TX3927")

ALLOW_SAVE_TYPE(mips1core_device_base::branch_state);

mips1core_device_base::mips1core_device_base(machine_config const &mconfig, device_type type, char const *tag, device_t *owner, u32 clock, u32 cpurev, size_t icache_size, size_t dcache_size)
	: cpu_device(mconfig, type, tag, owner, clock)
	, m_program_config_be("program", ENDIANNESS_BIG, 32, 32)
	, m_program_config_le("program", ENDIANNESS_LITTLE, 32, 32)
	, m_icache_config("icache", ENDIANNESS_BIG, 32, 32)
	, m_dcache_config("dcache", ENDIANNESS_BIG, 32, 32)
	, m_cpurev(cpurev)
	, m_endianness(ENDIANNESS_BIG)
	, m_icount(0)
	, m_icache_size(icache_size)
	, m_dcache_size(dcache_size)
	, m_in_brcond(*this)
{
}

mips1_device_base::mips1_device_base(machine_config const &mconfig, device_type type, char const *tag, device_t *owner, u32 clock, u32 cpurev, size_t icache_size, size_t dcache_size)
	: mips1core_device_base(mconfig, type, tag, owner, clock, cpurev, icache_size, dcache_size)
	, m_fcr0(0)
{
}

r2000_device::r2000_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock, size_t icache_size, size_t dcache_size)
	: mips1_device_base(mconfig, R2000, tag, owner, clock, 0x0100, icache_size, dcache_size)
{
}

r2000a_device::r2000a_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock, size_t icache_size, size_t dcache_size)
	: mips1_device_base(mconfig, R2000A, tag, owner, clock, 0x0210, icache_size, dcache_size)
{
}

r3000_device::r3000_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock, size_t icache_size, size_t dcache_size)
	: mips1_device_base(mconfig, R3000, tag, owner, clock, 0x0220, icache_size, dcache_size)
{
}

r3000a_device::r3000a_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock, size_t icache_size, size_t dcache_size)
	: mips1_device_base(mconfig, R3000A, tag, owner, clock, 0x0230, icache_size, dcache_size)
{
}

r3041_device::r3041_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
	: mips1core_device_base(mconfig, R3041, tag, owner, clock, 0x0700, 2048, 512)
{
}

r3051_device::r3051_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
	: mips1core_device_base(mconfig, R3051, tag, owner, clock, 0x0200, 4096, 2048)
{
}

r3052_device::r3052_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
	: mips1core_device_base(mconfig, R3052, tag, owner, clock, 0x0200, 8192, 2048)
{
}

r3052e_device::r3052e_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
	: mips1_device_base(mconfig, R3052E, tag, owner, clock, 0x0200, 8192, 2048)
{
}

r3071_device::r3071_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock, size_t icache_size, size_t dcache_size)
	: mips1_device_base(mconfig, R3071, tag, owner, clock, 0x0200, icache_size, dcache_size)
{
}

r3081_device::r3081_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock, size_t icache_size, size_t dcache_size)
	: mips1_device_base(mconfig, R3081, tag, owner, clock, 0x0200, icache_size, dcache_size)
{
	set_fpu(0x0300);
}

iop_device::iop_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
	: mips1core_device_base(mconfig, SONYPS2_IOP, tag, owner, clock, 0x001f, 4096, 1024)
{
	m_endianness = ENDIANNESS_LITTLE;
}

/*
 * Two additional address spaces are defined to represent the instruction and
 * data caches. These are only used to simulate cache isolation functionality
 * at this point, but could simulate other behaviour as needed in future.
 */
void mips1core_device_base::device_add_mconfig(machine_config &config)
{
	set_addrmap(1, &mips1core_device_base::icache_map);
	set_addrmap(2, &mips1core_device_base::dcache_map);
}

void mips1core_device_base::device_start()
{
	// set our instruction counter
	set_icountptr(m_icount);

	// resolve conditional branch input handlers
	m_in_brcond.resolve_all_safe(0);

	// register our state for the debugger
	state_add(STATE_GENPC,      "GENPC",     m_pc).noshow();
	state_add(STATE_GENPCBASE,  "CURPC",     m_pc).noshow();

	state_add(MIPS1_PC,                   "PC",        m_pc);
	state_add(MIPS1_COP0 + COP0_Status,   "SR",        m_cop0[COP0_Status]);

	for (unsigned i = 0; i < std::size(m_r); i++)
		state_add(MIPS1_R0 + i, util::string_format("R%d", i).c_str(), m_r[i]);

	state_add(MIPS1_HI, "HI", m_hi);
	state_add(MIPS1_LO, "LO", m_lo);

	// cop0 exception registers
	state_add(MIPS1_COP0 + COP0_BadVAddr, "BadVAddr", m_cop0[COP0_BadVAddr]);
	state_add(MIPS1_COP0 + COP0_Cause,    "Cause",    m_cop0[COP0_Cause]);
	state_add(MIPS1_COP0 + COP0_EPC,      "EPC",      m_cop0[COP0_EPC]);

	// register our state for saving
	save_item(NAME(m_pc));
	save_item(NAME(m_hi));
	save_item(NAME(m_lo));
	save_item(NAME(m_r));
	save_item(NAME(m_cop0));
	save_item(NAME(m_branch_state));
	save_item(NAME(m_branch_target));

	// initialise cpu id register
	m_cop0[COP0_PRId] = m_cpurev;

	m_cop0[COP0_Cause] = 0;

	m_r[0] = 0;
}

void r3041_device::device_start()
{
	mips1core_device_base::device_start();

	// cop0 r3041 registers
	state_add(MIPS1_COP0 + COP0_BusCtrl,  "BusCtrl", m_cop0[COP0_BusCtrl]);
	state_add(MIPS1_COP0 + COP0_Config,   "Config", m_cop0[COP0_Config]);
	state_add(MIPS1_COP0 + COP0_Count,    "Count", m_cop0[COP0_Count]);
	state_add(MIPS1_COP0 + COP0_PortSize, "PortSize", m_cop0[COP0_PortSize]);
	state_add(MIPS1_COP0 + COP0_Compare,  "Compare", m_cop0[COP0_Compare]);

	m_cop0[COP0_BusCtrl] = 0x20130b00U;
	m_cop0[COP0_Config] = 0x40000000U;
	m_cop0[COP0_PortSize] = 0;
}

void mips1core_device_base::device_reset()
{
	// initialize the state
	m_pc = 0xbfc00000;
	m_branch_state = NONE;

	// non-tlb devices have tlb shut down
	m_cop0[COP0_Status] = SR_BEV | SR_TS;

	m_data_spacenum = 0;
	m_bus_error = false;
}

void r3041_device::device_reset()
{
	mips1core_device_base::device_reset();

	m_cop0[COP0_Count] = 0;
	m_cop0[COP0_Compare] = 0x00ffffffU;
}

void mips1core_device_base::execute_run()
{
	// core execution loop
	while (m_icount-- > 0)
	{
		// debugging
		debugger_instruction_hook(m_pc);

		if (VERBOSE & LOG_IOP)
		{
			if ((m_pc & 0x1fffffff) == 0x00012C48 || (m_pc & 0x1fffffff) == 0x0001420C || (m_pc & 0x1fffffff) == 0x0001430C)
			{
				u32 ptr = m_r[5];
				u32 length = m_r[6];
				if (length >= 4096)
					length = 4095;
				while (length)
				{
					load<u8>(ptr, [](char c) { printf("%c", c); });
					ptr++;
					length--;
				}
				fflush(stdout);
			}
		}

		// fetch instruction
		fetch(m_pc, [this](u32 const op)
		{
			// check for interrupts
			if ((CAUSE & SR & SR_IM) && (SR & SR_IEc))
			{
				generate_exception(EXCEPTION_INTERRUPT);
				return;
			}

			// decode and execute instruction
			switch (op >> 26)
			{
			case 0x00: // SPECIAL
				switch (op & 63)
				{
				case 0x00: // SLL
					m_r[RDREG] = m_r[RTREG] << SHIFT;
					break;
				case 0x02: // SRL
					m_r[RDREG] = m_r[RTREG] >> SHIFT;
					break;
				case 0x03: // SRA
					m_r[RDREG] = s32(m_r[RTREG]) >> SHIFT;
					break;
				case 0x04: // SLLV
					m_r[RDREG] = m_r[RTREG] << (m_r[RSREG] & 31);
					break;
				case 0x06: // SRLV
					m_r[RDREG] = m_r[RTREG] >> (m_r[RSREG] & 31);
					break;
				case 0x07: // SRAV
					m_r[RDREG] = s32(m_r[RTREG]) >> (m_r[RSREG] & 31);
					break;
				case 0x08: // JR
					m_branch_state = BRANCH;
					m_branch_target = m_r[RSREG];
					break;
				case 0x09: // JALR
					m_branch_state = BRANCH;
					m_branch_target = m_r[RSREG];
					m_r[RDREG] = m_pc + 8;
					break;
				case 0x0c: // SYSCALL
					generate_exception(EXCEPTION_SYSCALL);
					break;
				case 0x0d: // BREAK
					generate_exception(EXCEPTION_BREAK);
					break;
				case 0x0e: // SDBBP
					// TODO: Implement properly?
					generate_exception(EXCEPTION_BREAK);
					break;
				case 0x0f:
					// SYNC
					break;
				case 0x10: // MFHI
					m_r[RDREG] = m_hi;
					break;
				case 0x11: // MTHI
					m_hi = m_r[RSREG];
					break;
				case 0x12: // MFLO
					m_r[RDREG] = m_lo;
					break;
				case 0x13: // MTLO
					m_lo = m_r[RSREG];
					break;
				case 0x18: // MULT
					{
						u64 product = mul_32x32(m_r[RSREG], m_r[RTREG]);
						m_r[RDREG] = product & 0xffffffff;

						m_lo = product;
						m_hi = product >> 32;
						m_icount -= 11;
					}
					break;
				case 0x19: // MULTU
					{
						u64 product = mulu_32x32(m_r[RSREG], m_r[RTREG]);
						m_r[RDREG] = product & 0xffffffff;

						m_lo = product;
						m_hi = product >> 32;
						m_icount -= 11;
					}
					break;
				case 0x1a: // DIV
					if (m_r[RTREG])
					{
						m_lo = s32(m_r[RSREG]) / s32(m_r[RTREG]);
						m_hi = s32(m_r[RSREG]) % s32(m_r[RTREG]);
					}
					m_icount -= 34;
					break;
				case 0x1b: // DIVU
					if (m_r[RTREG])
					{
						m_lo = m_r[RSREG] / m_r[RTREG];
						m_hi = m_r[RSREG] % m_r[RTREG];
					}
					m_icount -= 34;
					break;
				case 0x20: // ADD
					{
						u32 const sum = m_r[RSREG] + m_r[RTREG];

						// overflow: (sign(addend0) == sign(addend1)) && (sign(addend0) != sign(sum))
						if (!BIT(m_r[RSREG] ^ m_r[RTREG], 31) && BIT(m_r[RSREG] ^ sum, 31))
							generate_exception(EXCEPTION_OVERFLOW);
						else
							m_r[RDREG] = sum;
					}
					break;
				case 0x21: // ADDU
					m_r[RDREG] = m_r[RSREG] + m_r[RTREG];
					break;
				case 0x22: // SUB
					{
						u32 const difference = m_r[RSREG] - m_r[RTREG];

						// overflow: (sign(minuend) != sign(subtrahend)) && (sign(minuend) != sign(difference))
						if (BIT(m_r[RSREG] ^ m_r[RTREG], 31) && BIT(m_r[RSREG] ^ difference, 31))
							generate_exception(EXCEPTION_OVERFLOW);
						else
							m_r[RDREG] = difference;
					}
					break;
				case 0x23: // SUBU
					m_r[RDREG] = m_r[RSREG] - m_r[RTREG];
					break;
				case 0x24: // AND
					m_r[RDREG] = m_r[RSREG] & m_r[RTREG];
					break;
				case 0x25: // OR
					m_r[RDREG] = m_r[RSREG] | m_r[RTREG];
					break;
				case 0x26: // XOR
					m_r[RDREG] = m_r[RSREG] ^ m_r[RTREG];
					break;
				case 0x27: // NOR
					m_r[RDREG] = ~(m_r[RSREG] | m_r[RTREG]);
					break;
				case 0x2a: // SLT
					m_r[RDREG] = s32(m_r[RSREG]) < s32(m_r[RTREG]);
					break;
				case 0x2b: // SLTU
					m_r[RDREG] = u32(m_r[RSREG]) < u32(m_r[RTREG]);
					break;
				default:
					generate_exception(EXCEPTION_INVALIDOP);
					break;
				}
				break;
			case 0x01: // REGIMM
				/*
				 * Hardware testing has established that MIPS-1 processors do
				 * not decode bit 17 of REGIMM format instructions. This bit is
				 * used to add the "branch likely" instructions for MIPS-2 and
				 * later architectures.
				 *
				 * IRIX 5.3 inst(1M) uses this behaviour to distinguish MIPS-1
				 * from MIPS-2 processors; the latter nullify the delay slot
				 * instruction if the branch is not taken, whereas the former
				 * execute the delay slot instruction regardless.
				 */
				switch (RTREG & 0x1f)
				{
				case 0x00: // BLTZ
					if (s32(m_r[RSREG]) < 0)
					{
						m_branch_state = BRANCH;
						m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
					}
					break;
				case 0x01: // BGEZ
					if (s32(m_r[RSREG]) >= 0)
					{
						m_branch_state = BRANCH;
						m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
					}
					break;
				case 0x02: // BLTZL
					if (s32(m_r[RSREG]) < 0)
					{
						m_branch_state = BRANCH;
						m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
					}
					else
					{
						m_pc += 4;
					}
					break;
				case 0x03: // BGEZL
					if (s32(m_r[RSREG]) >= 0)
					{
						m_branch_state = BRANCH;
						m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
					}
					else
					{
						m_pc += 4;
					}
					break;
				case 0x10: // BLTZAL
					m_r[31] = m_pc + 8;

					if (s32(m_r[RSREG]) < 0)
					{
						m_branch_state = BRANCH;
						m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
					}
					break;
				case 0x11: // BGEZAL
					m_r[31] = m_pc + 8;

					if (s32(m_r[RSREG]) >= 0)
					{
						m_branch_state = BRANCH;
						m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
					}
					break;
				case 0x12: // BLTZALL
					m_r[31] = m_pc + 8;

					if (s32(m_r[RSREG]) < 0)
					{
						m_branch_state = BRANCH;
						m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
					}
					else
					{
						m_pc += 4;
					}
					break;
				case 0x13: // BGEZALL
					m_r[31] = m_pc + 8;

					if (s32(m_r[RSREG]) >= 0)
					{
						m_branch_state = BRANCH;
						m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
					}
					else
					{
						m_pc += 4;
					}
					break;
				default:
					generate_exception(EXCEPTION_INVALIDOP);
					break;
				}
				break;
			case 0x02: // J
				m_branch_state = BRANCH;
				m_branch_target = ((m_pc + 4) & 0xf0000000) | (LIMMVAL << 2);
				break;
			case 0x03: // JAL
				m_branch_state = BRANCH;
				m_branch_target = ((m_pc + 4) & 0xf0000000) | (LIMMVAL << 2);
				m_r[31] = m_pc + 8;
				break;
			case 0x04: // BEQ
				if (m_r[RSREG] == m_r[RTREG])
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				break;
			case 0x05: // BNE
				if (m_r[RSREG] != m_r[RTREG])
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				break;
			case 0x06: // BLEZ
				if (s32(m_r[RSREG]) <= 0)
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				break;
			case 0x07: // BGTZ
				if (s32(m_r[RSREG]) > 0)
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				break;
			case 0x08: // ADDI
				{
					u32 const sum = m_r[RSREG] + SIMMVAL;

					// overflow: (sign(addend0) == sign(addend1)) && (sign(addend0) != sign(sum))
					if (!BIT(m_r[RSREG] ^ s32(SIMMVAL), 31) && BIT(m_r[RSREG] ^ sum, 31))
						generate_exception(EXCEPTION_OVERFLOW);
					else
						m_r[RTREG] = sum;
				}
				break;
			case 0x09: // ADDIU
				m_r[RTREG] = m_r[RSREG] + SIMMVAL;
				break;
			case 0x0a: // SLTI
				m_r[RTREG] = s32(m_r[RSREG]) < s32(SIMMVAL);
				break;
			case 0x0b: // SLTIU
				m_r[RTREG] = u32(m_r[RSREG]) < u32(SIMMVAL);
				break;
			case 0x0c: // ANDI
				m_r[RTREG] = m_r[RSREG] & UIMMVAL;
				break;
			case 0x0d: // ORI
				m_r[RTREG] = m_r[RSREG] | UIMMVAL;
				break;
			case 0x0e: // XORI
				m_r[RTREG] = m_r[RSREG] ^ UIMMVAL;
				break;
			case 0x0f: // LUI
				m_r[RTREG] = UIMMVAL << 16;
				break;
			case 0x10: // COP0
				if (!(SR & SR_KUc) || (SR & SR_COP0))
					handle_cop0(op);
				else
					generate_exception(EXCEPTION_BADCOP0);
				break;
			case 0x11: // COP1
				handle_cop1(op);
				break;
			case 0x12: // COP2
				handle_cop2(op);
				break;
			case 0x13: // COP3
				handle_cop3(op);
				break;
			case 0x14: // BEQL
				if (m_r[RSREG] == m_r[RTREG])
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				else
				{
					m_pc += 4;
				}
				break;
			case 0x15: // BNEL
				if (m_r[RSREG] != m_r[RTREG])
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				else
				{
					m_pc += 4;
				}
				break;
			case 0x16: // BLEZL
				if (s32(m_r[RSREG]) <= 0)
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				else
				{
					m_pc += 4;
				}
				break;
			case 0x17: // BGTZL
				if (s32(m_r[RSREG]) > 0)
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				else
				{
					m_pc += 4;
				}
				break;
			case 0x1c: // MADD/MADDU
				{
					// Note: "To guarantee correct operation even if an interrupt occurs,
					// neither of the two instructions following MADD should be DIV or DIVU
					// instructions which modify the HI and LO register contents"
					u64 product = 0;

					if (op & 1) // MADDU
						product += mulu_32x32(m_r[RSREG], m_r[RTREG]);
					else // MADD
						product += mul_32x32(m_r[RSREG], m_r[RTREG]);

					product += (uint64_t(m_hi) << 32) + m_lo;

					m_lo = product;
					m_hi = product >> 32;
					m_r[RDREG] = m_lo;
					m_icount -= 11;
				}
				break;
			case 0x20: // LB
				load<u8>(SIMMVAL + m_r[RSREG], [this, op](s8 temp) { m_r[RTREG] = temp; });
				break;
			case 0x21: // LH
				load<u16>(SIMMVAL + m_r[RSREG], [this, op](s16 temp) { m_r[RTREG] = temp; });
				break;
			case 0x22: // LWL
				lwl(op);
				break;
			case 0x23: // LW
				load<u32>(SIMMVAL + m_r[RSREG], [this, op](u32 temp) { m_r[RTREG] = temp; });
				break;
			case 0x24: // LBU
				load<u8>(SIMMVAL + m_r[RSREG], [this, op](u8 temp) { m_r[RTREG] = temp; });
				break;
			case 0x25: // LHU
				load<u16>(SIMMVAL + m_r[RSREG], [this, op](u16 temp) { m_r[RTREG] = temp; });
				break;
			case 0x26: // LWR
				lwr(op);
				break;
			case 0x28: // SB
				store<u8>(SIMMVAL + m_r[RSREG], m_r[RTREG]);
				break;
			case 0x29: // SH
				store<u16>(SIMMVAL + m_r[RSREG], m_r[RTREG]);
				break;
			case 0x2a: // SWL
				swl(op);
				break;
			case 0x2b: // SW
				store<u32>(SIMMVAL + m_r[RSREG], m_r[RTREG]);
				break;
			case 0x2e: // SWR
				swr(op);
				break;
			case 0x2f: // CACHE
				break;
			case 0x31: // LWC1
				handle_cop1(op);
				break;
			case 0x32: // LWC2
				handle_cop2(op);
				break;
			case 0x33: // LWC3
				handle_cop3(op);
				break;
			case 0x39: // SWC1
				handle_cop1(op);
				break;
			case 0x3a: // SWC2
				handle_cop2(op);
				break;
			case 0x3b: // SWC3
				handle_cop3(op);
				break;
			default:
				generate_exception(EXCEPTION_INVALIDOP);
				break;
			}

			// clear register 0
			m_r[0] = 0;
		});

		// update pc and branch state
		switch (m_branch_state)
		{
		case NONE:
			m_pc += 4;
			break;

		case DELAY:
			m_branch_state = NONE;
			m_pc = m_branch_target;
			break;

		case BRANCH:
			m_branch_state = DELAY;
			m_pc += 4;
			break;

		case EXCEPTION:
			m_branch_state = NONE;
			break;
		}
	}
}

void mips1core_device_base::execute_set_input(int irqline, int state)
{
	if (state != CLEAR_LINE)
	{
		CAUSE |= CAUSE_IPEX0 << irqline;

		// enable debugger interrupt breakpoints
		if ((SR & SR_IEc) && (SR & (SR_IMEX0 << irqline)))
			standard_irq_callback(irqline);
	}
	else
		CAUSE &= ~(CAUSE_IPEX0 << irqline);
}

device_memory_interface::space_config_vector mips1core_device_base::memory_space_config() const
{
	return space_config_vector {
		std::make_pair(AS_PROGRAM, (m_endianness == ENDIANNESS_BIG) ? &m_program_config_be : &m_program_config_le),
		std::make_pair(1, &m_icache_config),
		std::make_pair(2, &m_dcache_config)
	};
}

bool mips1core_device_base::memory_translate(int spacenum, int intention, offs_t &address)
{
	// check for kernel memory address
	if (BIT(address, 31))
	{
		// check debug or kernel mode
		if ((intention & TRANSLATE_DEBUG_MASK) || !(SR & SR_KUc))
		{
			switch (address & 0xe0000000)
			{
			case 0x80000000: // kseg0: unmapped, cached, privileged
			case 0xa0000000: // kseg1: unmapped, uncached, privileged
				address &= ~0xe0000000;
				break;

			case 0xc0000000: // kseg2: mapped, cached, privileged
			case 0xe0000000:
				break;
			}
		}
		else if (SR & SR_KUc)
		{
			address_error(intention, address);

			return false;
		}
	}
	else
		// kuseg physical addresses have a 1GB offset
		address += 0x40000000;

	return true;
}

std::unique_ptr<util::disasm_interface> mips1core_device_base::create_disassembler()
{
	return std::make_unique<mips1_disassembler>();
}

void mips1core_device_base::icache_map(address_map &map)
{
	if (m_icache_size)
		map(0, m_icache_size - 1).ram().mirror(~(m_icache_size - 1));
}

void mips1core_device_base::dcache_map(address_map &map)
{
	if (m_dcache_size)
		map(0, m_dcache_size - 1).ram().mirror(~(m_dcache_size - 1));
}

void mips1core_device_base::generate_exception(u32 exception, bool refill)
{
	if ((VERBOSE & LOG_RISCOS) && (exception == EXCEPTION_SYSCALL))
	{
		static char const *const sysv_syscalls[] =
		{
			"syscall",      "exit",         "fork",         "read",         "write",        "open",         "close",        "wait",         "creat",        "link",
			"unlink",       "execv",        "chdir",        "time",         "mknod",        "chmod",        "chown",        "brk",          "stat",         "lseek",
			"getpid",       "mount",        "umount",       "setuid",       "getuid",       "stime",        "ptrace",       "alarm",        "fstat",        "pause",
			"utime",        "stty",         "gtty",         "access",       "nice",         "statfs",       "sync",         "kill",         "fstatfs",      "setpgrp",
			nullptr,        "dup",          "pipe",         "times",        "profil",       "plock",        "setgid",       "getgid",       "signal",       "msgsys",
			"sysmips",      "acct",         "shmsys",       "semsys",       "ioctl",        "uadmin",       nullptr,        "utssys",       nullptr,        "execve",
			"umask",        "chroot",       "ofcntl",       "ulimit",       nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr,
			"advfs",        "unadvfs",      "rmount",       "rumount",      "rfstart",      nullptr,        "rdebug",       "rfstop",       "rfsys",        "rmdir",
			"mkdir",        "getdents",     nullptr,        nullptr,        "sysfs",        "getmsg",       "putmsg",       "poll",         "sigreturn",    "accept",
			"bind",         "connect",      "gethostid",    "getpeername",  "getsockname",  "getsockopt",   "listen",       "recv",         "recvfrom",     "recvmsg",
			"select",       "send",         "sendmsg",      "sendto",       "sethostid",    "setsockopt",   "shutdown",     "socket",       "gethostname",  "sethostname",
			"getdomainname","setdomainname","truncate",     "ftruncate",    "rename",       "symlink",      "readlink",     "lstat",        "nfsmount",     "nfssvc",
			"getfh",        "async_daemon", "old_exportfs", "mmap",         "munmap",       "getitimer",    "setitimer",    nullptr,        nullptr,        nullptr,
			nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr,
			nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr,        nullptr,
			"cacheflush",   "cachectl",     "fchown",       "fchmod",       "wait3",        "mmap",         "munmap",       "madvise",      "getpagesize",  "setreuid",
			"setregid",     "setpgid",      "getgroups",    "setgroups",    "gettimeofday", "getrusage",    "getrlimit",    "setrlimit",    "exportfs",     "fcntl"
		};

		static char const *const bsd_syscalls[] =
		{
			"syscall",      "exit",         "fork",         "read",         "write",        "open",         "close",        nullptr,        "creat",        "link",
			"unlink",       "execv",        "chdir",        nullptr,        "mknod",        "chmod",        "chown",        "brk",          nullptr,        "lseek",
			"getpid",       "omount",       "oumount",      nullptr,        "getuid",       nullptr,        "ptrace",       nullptr,        nullptr,        nullptr,
			nullptr,        nullptr,        nullptr,        "access",       nullptr,        nullptr,        "sync",         "kill",         "stat",         nullptr,
			"lstat",        "dup",          "pipe",         nullptr,        "profil",       nullptr,        nullptr,        "getgid",       nullptr,        nullptr,
			nullptr,        "acct",         nullptr,        nullptr,        "ioctl",        "reboot",       nullptr,        "symlink",      "readlink",     "execve",
			"umask",        "chroot",       "fstat",        nullptr,        "getpagesize",  "mremap",       "vfork",        nullptr,        nullptr,        "sbrk",
			"sstk",         "mmap",         "vadvise",      "munmap",       "mprotec",      "madvise",      "vhangup",      nullptr,        "mincore",      "getgroups",
			"setgroups",    "getpgrp",      "setpgrp",      "setitimer",    "wait3",        "swapon",       "getitimer",    "gethostname",  "sethostname",  "getdtablesize",
			"dup2",         "getdopt",      "fcntl",        "select",       "setdopt",      "fsync",        "setpriority",  "socket",       "connect",      "accept",
			"getpriority",  "send",         "recv",         "sigreturn",    "bind",         "setsockopt",   "listen",       nullptr,        "sigvec",       "sigblock",
			"sigsetmask",   "sigpause",     "sigstack",     "recvmsg",      "sendmsg",      nullptr,        "gettimeofday", "getrusage",    "getsockopt",   nullptr,
			"readv",        "writev",       "settimeofday", "fchown",       "fchmod",       "recvfrom",     "setreuid",     "setregid",     "rename",       "truncate",
			"ftruncate",    "flock",        nullptr,        "sendto",       "shutdown",     "socketpair",   "mkdir",        "rmdir",        "utimes",       "sigcleanup",
			"adjtime",      "getpeername",  "gethostid",    "sethostid",    "getrlimit",    "setrlimit",    "killpg",       nullptr,        "setquota",     "quota",
			"getsockname",  "sysmips",      "cacheflush",   "cachectl",     "debug",        nullptr,        nullptr,        nullptr,        "nfssvc",       "getdirentries",
			"statfs",       "fstatfs",      "unmount",      "async_daemon", "getfh",        "getdomainname","setdomainname",nullptr,        "quotactl",     "old_exportfs",
			"mount",        "hdwconf",      "exportfs",     "nfsfh_open",   "libattach",    "libdetach"
		};

		static char const *const msg_syscalls[] = { "msgget", "msgctl", "msgrcv", "msgsnd" };
		static char const *const shm_syscalls[] = { "shmat", "shmctl", "shmdt", "shmget" };
		static char const *const sem_syscalls[] = { "semctl", "semget", "semop" };
		static char const *const mips_syscalls[] = { "mipskopt", "mipshwconf", "mipsgetrusage", "mipswait", "mipscacheflush", "mipscachectl" };

		unsigned const asid = (m_cop0[COP0_EntryHi] & EH_ASID) >> 6;
		switch (m_r[2])
		{
		case 1000: // indirect
			switch (m_r[4])
			{
			case 1049: // msgsys
				LOGMASKED(LOG_RISCOS, "asid %d syscall msgsys:%s() (%s)\n",
					asid, (m_r[5] < std::size(msg_syscalls)) ? msg_syscalls[m_r[5]] : "unknown", machine().describe_context());
				break;

			case 1052: // shmsys
				LOGMASKED(LOG_RISCOS, "asid %d syscall shmsys:%s() (%s)\n",
					asid, (m_r[5] < std::size(shm_syscalls)) ? shm_syscalls[m_r[5]] : "unknown", machine().describe_context());
				break;

			case 1053: // semsys
				LOGMASKED(LOG_RISCOS, "asid %d syscall semsys:%s() (%s)\n",
					asid, (m_r[5] < std::size(sem_syscalls)) ? sem_syscalls[m_r[5]] : "unknown", machine().describe_context());
				break;

			case 2151: // bsd_sysmips
				switch (m_r[5])
				{
				case 0x100: // mipskopt
					LOGMASKED(LOG_RISCOS, "asid %d syscall bsd_sysmips:mipskopt(\"%s\") (%s)\n",
						asid, debug_string(m_r[6]), machine().describe_context());
					break;

				default:
					if ((m_r[5] > 0x100) && (m_r[5] - 0x100) < std::size(mips_syscalls))
						LOGMASKED(LOG_RISCOS, "asid %d syscall bsd_sysmips:%s() (%s)\n",
							asid, mips_syscalls[m_r[5] - 0x100], machine().describe_context());
					else
						LOGMASKED(LOG_RISCOS, "asid %d syscall bsd_sysmips:unknown %d (%s)\n",
							asid, m_r[5], machine().describe_context());
					break;
				}
				break;

			default:
				if ((m_r[4] > 2000) && (m_r[4] - 2000 < std::size(bsd_syscalls)) && bsd_syscalls[m_r[4] - 2000])
					LOGMASKED(LOG_RISCOS, "asid %d syscall bsd_%s() (%s)\n",
						asid, bsd_syscalls[m_r[4] - 2000], machine().describe_context());
				else
					LOGMASKED(LOG_RISCOS, "asid %d syscall indirect:unknown %d (%s)\n",
						asid, m_r[4], machine().describe_context());
				break;
			}
			break;

		case 1003: // read
		case 1006: // close
		case 1054: // ioctl
		case 1169: // fcntl
			LOGMASKED(LOG_RISCOS, "asid %d syscall %s(%d) (%s)\n",
				asid, sysv_syscalls[m_r[2] - 1000], m_r[4], machine().describe_context());
			break;

		case 1004: // write
			if (m_r[4] == 1 || m_r[4] == 2)
				LOGMASKED(LOG_RISCOS, "asid %d syscall %s(%d, \"%s\") (%s)\n",
					asid, sysv_syscalls[m_r[2] - 1000], m_r[4], debug_string(m_r[5], m_r[6]), machine().describe_context());
			else
				LOGMASKED(LOG_RISCOS, "asid %d syscall %s(%d) (%s)\n",
					asid, sysv_syscalls[m_r[2] - 1000], m_r[4], machine().describe_context());
			break;

		case 1005: // open
		case 1008: // creat
		case 1009: // link
		case 1010: // unlink
		case 1012: // chdir
		case 1018: // stat
		case 1033: // access
			LOGMASKED(LOG_RISCOS, "asid %d syscall %s(\"%s\") (%s)\n",
				asid, sysv_syscalls[m_r[2] - 1000], debug_string(m_r[4]), machine().describe_context());
			break;

		case 1059: // execve
			LOGMASKED(LOG_RISCOS, "asid %d syscall execve(\"%s\", [ %s ], [ %s ]) (%s)\n",
				asid, debug_string(m_r[4]), debug_string_array(m_r[5]), debug_string_array(m_r[6]), machine().describe_context());
			break;

		case 1060: // umask
			LOGMASKED(LOG_RISCOS, "asid %d syscall umask(%#o) (%s)\n",
				asid, m_r[4] & 0777, machine().describe_context());
			break;

		default:
			if ((m_r[2] > 1000) && (m_r[2] - 1000 < std::size(sysv_syscalls)) && sysv_syscalls[m_r[2] - 1000])
				LOGMASKED(LOG_RISCOS, "asid %d syscall %s() (%s)\n", asid, sysv_syscalls[m_r[2] - 1000], machine().describe_context());
			else
				LOGMASKED(LOG_RISCOS, "asid %d syscall unknown %d (%s)\n", asid, m_r[2], machine().describe_context());
			break;
		}
	}

	// set the exception PC
	m_cop0[COP0_EPC] = m_pc;

	// load the cause register
	CAUSE = (CAUSE & CAUSE_IP) | exception;

	// if in a branch delay slot, restart the branch
	if (m_branch_state == DELAY)
	{
		m_cop0[COP0_EPC] -= 4;
		CAUSE |= CAUSE_BD;
	}
	m_branch_state = EXCEPTION;

	// shift the exception bits
	SR = (SR & ~SR_KUIE) | ((SR << 2) & SR_KUIEop);

	if (refill)
		m_pc = (SR & SR_BEV) ? 0xbfc00100 : 0x80000000;
	else
		m_pc = (SR & SR_BEV) ? 0xbfc00180 : 0x80000080;

	debugger_exception_hook(exception);

	if (SR & SR_KUp)
		debugger_privilege_hook();
}

void mips1core_device_base::address_error(int intention, u32 const address)
{
	if (!machine().side_effects_disabled() && !(intention & TRANSLATE_DEBUG_MASK))
	{
		logerror("address_error 0x%08x (%s)\n", address, machine().describe_context());

		m_cop0[COP0_BadVAddr] = address;

		generate_exception((intention & TRANSLATE_WRITE) ? EXCEPTION_ADDRSTORE : EXCEPTION_ADDRLOAD);

		// address errors shouldn't typically occur, so a breakpoint is handy
		machine().debug_break();
	}
}

void mips1core_device_base::handle_cop0(u32 const op)
{
	switch (RSREG)
	{
	case 0x00: // MFC0
		m_r[RTREG] = get_cop0_reg(RDREG);
		break;
	case 0x04: // MTC0
		set_cop0_reg(RDREG, m_r[RTREG]);
		break;
	case 0x08: // BC0
		switch (RTREG)
		{
		case 0x00: // BC0F
			if (!m_in_brcond[0]())
			{
				m_branch_state = BRANCH;
				m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
			}
			break;
		case 0x01: // BC0T
			if (m_in_brcond[0]())
			{
				m_branch_state = BRANCH;
				m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
			}
			break;
		case 0x02: // BC0FL
			if (!m_in_brcond[0]())
			{
				m_branch_state = BRANCH;
				m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
			}
			else
			{
				m_pc += 4;
			}
			break;
		case 0x03: // BC0TL
			if (m_in_brcond[0]())
			{
				m_branch_state = BRANCH;
				m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
			}
			else
			{
				m_pc += 4;
			}
			break;
		default:
			generate_exception(EXCEPTION_INVALIDOP);
			break;
		}
		break;
	case 0x10: // COP0
		switch (op & 31)
		{
			case 0x10: // RFE
				SR = (SR & ~SR_KUIE) | ((SR >> 2) & SR_KUIEpc);
				if (bool(SR & SR_KUc) ^ bool(SR & SR_KUp))
					debugger_privilege_hook();
				break;
			default:
				generate_exception(EXCEPTION_INVALIDOP);
				break;
		}
		break;
	default:
		generate_exception(EXCEPTION_INVALIDOP);
		break;
	}
}

u32 mips1core_device_base::get_cop0_reg(unsigned const reg)
{
	return m_cop0[reg];
}

void mips1core_device_base::set_cop0_reg(unsigned const reg, u32 const data)
{
	switch (reg)
	{
	case COP0_Status:
		{
			u32 const delta = SR ^ data;

			m_cop0[COP0_Status] = data;

			// handle cache isolation and swap
			m_data_spacenum = (data & SR_IsC) ? ((data & SR_SwC) ? 1 : 2) : 0;

			if ((delta & SR_KUc) && (m_branch_state != EXCEPTION))
				debugger_privilege_hook();
		}
		break;

	case COP0_Cause:
		CAUSE = (CAUSE & CAUSE_IPEX) | (data & ~CAUSE_IPEX);
		break;

	case COP0_PRId:
		// read-only register
		break;

	default:
		m_cop0[reg] = data;
		break;
	}
}

void mips1core_device_base::handle_cop1(u32 const op)
{
	if (!(SR & SR_COP1))
		generate_exception(EXCEPTION_BADCOP1);
}

void mips1core_device_base::handle_cop2(u32 const op)
{
	if (SR & SR_COP2)
	{
		switch (RSREG)
		{
		case 0x08: // BC2
			switch (RTREG)
			{
			case 0x00: // BC2F
				if (!m_in_brcond[2]())
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				break;
			case 0x01: // BC2T
				if (m_in_brcond[2]())
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				break;
			case 0x02: // BC2FL
				if (!m_in_brcond[2]())
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				else
				{
					m_pc += 4;
				}
				break;
			case 0x03: // BC2TL
				if (m_in_brcond[2]())
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				else
				{
					m_pc += 4;
				}
				break;
			default:
				generate_exception(EXCEPTION_INVALIDOP);
				break;
			}
			break;
		default:
			generate_exception(EXCEPTION_INVALIDOP);
			break;
		}
	}
	else
		generate_exception(EXCEPTION_BADCOP2);
}

void mips1core_device_base::handle_cop3(u32 const op)
{
	if (SR & SR_COP3)
	{
		switch (RSREG)
		{
		case 0x08: // BC3
			switch (RTREG)
			{
			case 0x00: // BC3F
				if (!m_in_brcond[3]())
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				break;
			case 0x01: // BC3T
				if (m_in_brcond[3]())
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				break;
			case 0x02: // BC3FL
				if (!m_in_brcond[3]())
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				else
				{
					m_pc += 4;
				}
				break;
			case 0x03: // BC3TL
				if (m_in_brcond[3]())
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				else
				{
					m_pc += 4;
				}
				break;
			default:
				generate_exception(EXCEPTION_INVALIDOP);
				break;
			}
			break;
		default:
			generate_exception(EXCEPTION_INVALIDOP);
			break;
		}
	}
	else
		generate_exception(EXCEPTION_BADCOP3);
}

void mips1core_device_base::lwl(u32 const op)
{
	offs_t const offset = SIMMVAL + m_r[RSREG];
	load<u32, false>(offset, [this, op, offset](u32 temp)
	{
		unsigned const shift = ((offset & 3) ^ (m_endianness == ENDIANNESS_LITTLE ? 3 : 0)) << 3;

		m_r[RTREG] = (m_r[RTREG] & ~u32(0xffffffffU << shift)) | (temp << shift);
	});
}

void mips1core_device_base::lwr(u32 const op)
{
	offs_t const offset = SIMMVAL + m_r[RSREG];
	load<u32, false>(offset, [this, op, offset](u32 temp)
	{
		unsigned const shift = ((offset & 0x3) ^ (m_endianness == ENDIANNESS_LITTLE ? 0 : 3)) << 3;

		m_r[RTREG] = (m_r[RTREG] & ~u32(0xffffffffU >> shift)) | (temp >> shift);
	});
}

void mips1core_device_base::swl(u32 const op)
{
	offs_t const offset = SIMMVAL + m_r[RSREG];
	unsigned const shift = ((offset & 3) ^ (m_endianness == ENDIANNESS_LITTLE ? 3 : 0)) << 3;

	store<u32, false>(offset, m_r[RTREG] >> shift, 0xffffffffU >> shift);
}

void mips1core_device_base::swr(u32 const op)
{
	offs_t const offset = SIMMVAL + m_r[RSREG];
	unsigned const shift = ((offset & 3) ^ (m_endianness == ENDIANNESS_LITTLE ? 0 : 3)) << 3;

	store<u32, false>(offset, m_r[RTREG] << shift, 0xffffffffU << shift);
}

template <typename T, bool Aligned, typename U> std::enable_if_t<std::is_convertible<U, std::function<void(T)>>::value, void> mips1core_device_base::load(u32 address, U &&apply)
{
	// alignment error
	if (Aligned && (address & (sizeof(T) - 1)))
	{
		address_error(TRANSLATE_READ, address);
		return;
	}

	if (memory_translate(m_data_spacenum, TRANSLATE_READ, address))
	{
		// align address for ld[lr] instructions
		if (!Aligned)
			address &= ~(sizeof(T) - 1);

		T const data
			= (sizeof(T) == 1) ? space(m_data_spacenum).read_byte(address)
			: (sizeof(T) == 2) ? space(m_data_spacenum).read_word(address)
			: space(m_data_spacenum).read_dword(address);

		if (m_bus_error)
		{
			m_bus_error = false;
			generate_exception(EXCEPTION_BUSDATA);
		}
		else
			apply(data);
	}
}

template <typename T, bool Aligned, typename U> std::enable_if_t<std::is_convertible<U, T>::value, void> mips1core_device_base::store(u32 address, U data, T mem_mask)
{
	// alignment error
	if (Aligned && (address & (sizeof(T) - 1)))
	{
		address_error(TRANSLATE_WRITE, address);
		return;
	}

	if (memory_translate(m_data_spacenum, TRANSLATE_WRITE, address))
	{
		// align address for sd[lr] instructions
		if (!Aligned)
			address &= ~(sizeof(T) - 1);

		switch (sizeof(T))
		{
		case 1: space(m_data_spacenum).write_byte(address, T(data)); break;
		case 2: space(m_data_spacenum).write_word(address, T(data), mem_mask); break;
		case 4: space(m_data_spacenum).write_dword(address, T(data), mem_mask); break;
		}
	}
}

bool mips1core_device_base::fetch(u32 address, std::function<void(u32)> &&apply)
{
	// alignment error
	if (address & 3)
	{
		address_error(TRANSLATE_FETCH, address);
		return false;
	}

	if (memory_translate(0, TRANSLATE_FETCH, address))
	{
		u32 const data = space(0).read_dword(address);

		if (m_bus_error)
		{
			m_bus_error = false;
			generate_exception(EXCEPTION_BUSINST);

			return false;
		}

		apply(data);
		return true;
	}
	else
		return false;
}

std::string mips1core_device_base::debug_string(u32 string_pointer, unsigned const limit)
{
	auto const suppressor(machine().disable_side_effects());

	bool done = false;
	bool mapped = false;
	std::string result("");

	while (!done)
	{
		done = true;
		load<u8>(string_pointer++, [limit, &done, &mapped, &result](u8 byte)
		{
			mapped = true;
			if (byte != 0)
			{
				result += byte;

				done = result.length() == limit;
			}
		});
	}

	if (!mapped)
		result.assign("[unmapped]");

	return result;
}

std::string mips1core_device_base::debug_string_array(u32 array_pointer)
{
	auto const suppressor(machine().disable_side_effects());

	bool done = false;
	std::string result("");

	while (!done)
	{
		done = true;
		load<u32>(array_pointer, [this, &done, &result](u32 string_pointer)
		{
			if (string_pointer != 0)
			{
				if (!result.empty())
					result += ", ";

				result += '\"' + debug_string(string_pointer) + '\"';

				done = false;
			}
		});

		array_pointer += 4;
	}

	return result;
}

void mips1_device_base::device_start()
{
	mips1core_device_base::device_start();

	// cop0 tlb registers
	state_add(MIPS1_COP0 + COP0_Index, "Index", m_cop0[COP0_Index]);
	state_add(MIPS1_COP0 + COP0_Random, "Random", m_cop0[COP0_Random]);
	state_add(MIPS1_COP0 + COP0_EntryLo, "EntryLo", m_cop0[COP0_EntryLo]);
	state_add(MIPS1_COP0 + COP0_EntryHi, "EntryHi", m_cop0[COP0_EntryHi]);
	state_add(MIPS1_COP0 + COP0_Context, "Context", m_cop0[COP0_Context]);

	// cop1 registers
	if (m_fcr0)
	{
		state_add(MIPS1_FCR31, "FCSR", m_fcr31);
		for (unsigned i = 0; i < std::size(m_f); i++)
			state_add(MIPS1_F0 + i, util::string_format("F%d", i * 2).c_str(), m_f[i]);
	}

	save_item(NAME(m_reset_time));
	save_item(NAME(m_tlb));

	save_item(NAME(m_fcr30));
	save_item(NAME(m_fcr31));
	save_item(NAME(m_f));
}

void mips1_device_base::device_reset()
{
	mips1core_device_base::device_reset();

	// tlb is not shut down
	m_cop0[COP0_Status] &= ~SR_TS;

	m_reset_time = total_cycles();

	// initialize tlb mru index with identity mapping
	for (unsigned i = 0; i < std::size(m_tlb); i++)
	{
		m_tlb_mru[TRANSLATE_READ][i] = i;
		m_tlb_mru[TRANSLATE_WRITE][i] = i;
		m_tlb_mru[TRANSLATE_FETCH][i] = i;
	}
}

void mips1_device_base::handle_cop0(u32 const op)
{
	switch (op)
	{
	case 0x42000001: // TLBR - read tlb
		{
			u8 const index = (m_cop0[COP0_Index] >> 8) & 0x3f;

			m_cop0[COP0_EntryHi] = m_tlb[index][0];
			m_cop0[COP0_EntryLo] = m_tlb[index][1];
		}
		break;

	case 0x42000002: // TLBWI - write tlb (indexed)
		{
			u8 const index = (m_cop0[COP0_Index] >> 8) & 0x3f;

			m_tlb[index][0] = m_cop0[COP0_EntryHi];
			m_tlb[index][1] = m_cop0[COP0_EntryLo];

			LOGMASKED(LOG_TLB, "asid %2d tlb write index %2d vpn 0x%08x pfn 0x%08x %c%c%c%c (%s)\n",
				(m_cop0[COP0_EntryHi] & EH_ASID) >> 6, index, m_cop0[COP0_EntryHi] & EH_VPN, m_cop0[COP0_EntryLo] & EL_PFN,
				m_cop0[COP0_EntryLo] & EL_N ? 'N' : '-',
				m_cop0[COP0_EntryLo] & EL_D ? 'D' : '-',
				m_cop0[COP0_EntryLo] & EL_V ? 'V' : '-',
				m_cop0[COP0_EntryLo] & EL_G ? 'G' : '-',
				machine().describe_context());
		}
		break;

	case 0x42000006: // TLBWR - write tlb (random)
		{
			u8 const random = get_cop0_reg(COP0_Random) >> 8;

			m_tlb[random][0] = m_cop0[COP0_EntryHi];
			m_tlb[random][1] = m_cop0[COP0_EntryLo];

			LOGMASKED(LOG_TLB, "asid %2d tlb write random %2d vpn 0x%08x pfn 0x%08x %c%c%c%c (%s)\n",
				(m_cop0[COP0_EntryHi] & EH_ASID) >> 6, random, m_cop0[COP0_EntryHi] & EH_VPN, m_cop0[COP0_EntryLo] & EL_PFN,
				m_cop0[COP0_EntryLo] & EL_N ? 'N' : '-',
				m_cop0[COP0_EntryLo] & EL_D ? 'D' : '-',
				m_cop0[COP0_EntryLo] & EL_V ? 'V' : '-',
				m_cop0[COP0_EntryLo] & EL_G ? 'G' : '-',
				machine().describe_context());
		}
		break;

	case 0x42000008: // TLBP - probe tlb
		m_cop0[COP0_Index] = 0x80000000;
		for (u8 index = 0; index < 64; index++)
		{
			// test vpn and optionally asid
			u32 const mask = (m_tlb[index][1] & EL_G) ? EH_VPN : EH_VPN | EH_ASID;
			if ((m_tlb[index][0] & mask) == (m_cop0[COP0_EntryHi] & mask))
			{
				LOGMASKED(LOG_TLB, "asid %2d tlb probe index %2d vpn 0x%08x (%s)\n",
					(m_cop0[COP0_EntryHi] & EH_ASID) >> 6, index, m_cop0[COP0_EntryHi] & mask, machine().describe_context());

				m_cop0[COP0_Index] = index << 8;
				break;
			}
		}
		if ((VERBOSE & LOG_TLB) && BIT(m_cop0[COP0_Index], 31))
			LOGMASKED(LOG_TLB, "asid %2d tlb probe miss vpn 0x%08x(%s)\n",
				(m_cop0[COP0_EntryHi] & EH_ASID) >> 6, m_cop0[COP0_EntryHi] & EH_VPN, machine().describe_context());
		break;

	default:
		mips1core_device_base::handle_cop0(op);
	}
}

u32 mips1_device_base::get_cop0_reg(unsigned const reg)
{
	// assume 64-entry tlb with 8 wired entries
	if (reg == COP0_Random)
		m_cop0[reg] = (63 - ((total_cycles() - m_reset_time) % 56)) << 8;

	return m_cop0[reg];
}

void mips1_device_base::set_cop0_reg(unsigned const reg, u32 const data)
{
	switch (reg)
	{
	case COP0_EntryHi:
		m_cop0[COP0_EntryHi] = data & EH_WM;
		break;

	case COP0_EntryLo:
		m_cop0[COP0_EntryLo] = data & EL_WM;
		break;

	case COP0_Context:
		m_cop0[COP0_Context] = (m_cop0[COP0_Context] & ~PTE_BASE) | (data & PTE_BASE);
		break;

	default:
		mips1core_device_base::set_cop0_reg(reg, data);
		break;
	}
}

void mips1_device_base::handle_cop1(u32 const op)
{
	if (!(SR & SR_COP1))
	{
		generate_exception(EXCEPTION_BADCOP1);
		return;
	}

	if (!m_fcr0)
		return;

	softfloat_exceptionFlags = 0;

	switch (op >> 26)
	{
	case 0x11: // COP1
		switch ((op >> 21) & 0x1f)
		{
		case 0x00: // MFC1
			if (FSREG & 1)
				// move the high half of the floating point register
				m_r[RTREG] = m_f[FSREG >> 1] >> 32;
			else
				// move the low half of the floating point register
				m_r[RTREG] = m_f[FSREG >> 1] >> 0;
			break;
		case 0x02: // CFC1
			switch (FSREG)
			{
			case 0:  m_r[RTREG] = m_fcr0; break;
			case 30: m_r[RTREG] = m_fcr30; break;
			case 31: m_r[RTREG] = m_fcr31; break;
				break;

			default:
				logerror("cfc1 undefined fpu control register %d (%s)\n", FSREG, machine().describe_context());
				break;
			}
			break;
		case 0x04: // MTC1
			if (FSREG & 1)
				// load the high half of the floating point register
				m_f[FSREG >> 1] = (u64(m_r[RTREG]) << 32) | u32(m_f[FSREG >> 1]);
			else
				// load the low half of the floating point register
				m_f[FSREG >> 1] = (m_f[FSREG >> 1] & ~0xffffffffULL) | m_r[RTREG];
			break;
		case 0x06: // CTC1
			switch (RDREG)
			{
			case 0: // register is read-only
				break;

			case 30:
				m_fcr30 = m_r[RTREG];
				break;

			case 31:
				m_fcr31 = m_r[RTREG];

				// update rounding mode
				switch (m_fcr31 & FCR31_RM)
				{
				case 0: softfloat_roundingMode = softfloat_round_near_even; break;
				case 1: softfloat_roundingMode = softfloat_round_minMag; break;
				case 2: softfloat_roundingMode = softfloat_round_max; break;
				case 3: softfloat_roundingMode = softfloat_round_min; break;
				}

				// exception check
				{
					bool const exception = (m_fcr31 & FCR31_CE) || (((m_fcr31 & FCR31_CM) >> 5) & (m_fcr31 & FCR31_EM));
					execute_set_input(m_fpu_irq, exception ? ASSERT_LINE : CLEAR_LINE);
				}
				break;

			default:
				logerror("ctc1 undefined fpu control register %d (%s)\n", RDREG, machine().describe_context());
				break;
			}
			break;
		case 0x08: // BC
			switch ((op >> 16) & 0x1f)
			{
			case 0x00: // BC1F
				if (!(m_fcr31 & FCR31_C))
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				break;
			case 0x01: // BC1T
				if (m_fcr31 & FCR31_C)
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				break;
			case 0x02: // BC1FL
				if (!(m_fcr31 & FCR31_C))
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				else
				{
					m_pc += 4;
				}
				break;
			case 0x03: // BC1TL
				if (m_fcr31 & FCR31_C)
				{
					m_branch_state = BRANCH;
					m_branch_target = m_pc + 4 + (s32(SIMMVAL) << 2);
				}
				else
				{
					m_pc += 4;
				}
				break;

			default:
				// unimplemented operation
				m_fcr31 |= FCR31_CE;
				execute_set_input(m_fpu_irq, ASSERT_LINE);
				break;
			}
			break;
		case 0x10: // S
			switch (op & 0x3f)
			{
			case 0x00: // ADD.S
				set_cop1_reg(FDREG >> 1, f32_add(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) }).v);
				break;
			case 0x01: // SUB.S
				set_cop1_reg(FDREG >> 1, f32_sub(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) }).v);
				break;
			case 0x02: // MUL.S
				set_cop1_reg(FDREG >> 1, f32_mul(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) }).v);
				break;
			case 0x03: // DIV.S
				set_cop1_reg(FDREG >> 1, f32_div(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) }).v);
				break;
			case 0x05: // ABS.S
				if (f32_lt(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ 0 }))
					set_cop1_reg(FDREG >> 1, f32_mul(float32_t{ u32(m_f[FSREG >> 1]) }, i32_to_f32(-1)).v);
				else
					set_cop1_reg(FDREG >> 1, u32(m_f[FSREG >> 1]));
				break;
			case 0x06: // MOV.S
				if (FDREG & 1)
					if (FSREG & 1)
						// move high half to high half
						m_f[FDREG >> 1] = (m_f[FSREG >> 1] & ~0xffffffffULL) | u32(m_f[FDREG >> 1]);
					else
						// move low half to high half
						m_f[FDREG >> 1] = (m_f[FSREG >> 1] << 32) | u32(m_f[FDREG >> 1]);
				else
					if (FSREG & 1)
						// move high half to low half
						m_f[FDREG >> 1] = (m_f[FDREG >> 1] & ~0xffffffffULL) | (m_f[FSREG >> 1] >> 32);
					else
						// move low half to low half
						m_f[FDREG >> 1] = (m_f[FDREG >> 1] & ~0xffffffffULL) | u32(m_f[FSREG >> 1]);
				break;
			case 0x07: // NEG.S
				set_cop1_reg(FDREG >> 1, f32_mul(float32_t{ u32(m_f[FSREG >> 1]) }, i32_to_f32(-1)).v);
				break;

			case 0x21: // CVT.D.S
				set_cop1_reg(FDREG >> 1, f32_to_f64(float32_t{ u32(m_f[FSREG >> 1]) }).v);
				break;
			case 0x24: // CVT.W.S
				set_cop1_reg(FDREG >> 1, f32_to_i32(float32_t{ u32(m_f[FSREG >> 1]) }, softfloat_roundingMode, true));
				break;

			case 0x30: // C.F.S (false)
				m_fcr31 &= ~FCR31_C;
				break;
			case 0x31: // C.UN.S (unordered)
				f32_eq(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) });
				if (softfloat_exceptionFlags & softfloat_flag_invalid)
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;
				break;
			case 0x32: // C.EQ.S (equal)
				if (f32_eq(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) }))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;
				break;
			case 0x33: // C.UEQ.S (unordered equal)
				if (f32_eq(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) }) || (softfloat_exceptionFlags & softfloat_flag_invalid))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;
				break;
			case 0x34: // C.OLT.S (less than)
				if (f32_lt(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) }))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;
				break;
			case 0x35: // C.ULT.S (unordered less than)
				if (f32_lt(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) }) || (softfloat_exceptionFlags & softfloat_flag_invalid))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;
				break;
			case 0x36: // C.OLE.S (less than or equal)
				if (f32_le(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) }))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;
				break;
			case 0x37: // C.ULE.S (unordered less than or equal)
				if (f32_le(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) }) || (softfloat_exceptionFlags & softfloat_flag_invalid))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;
				break;

			case 0x38: // C.SF.S (signalling false)
				f32_eq(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) });

				m_fcr31 &= ~FCR31_C;

				if (softfloat_exceptionFlags & softfloat_flag_invalid)
				{
					m_fcr31 |= FCR31_CV;
					execute_set_input(m_fpu_irq, ASSERT_LINE);
				}
				break;
			case 0x39: // C.NGLE.S (not greater, less than or equal)
				f32_eq(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) });

				if (softfloat_exceptionFlags & softfloat_flag_invalid)
				{
					m_fcr31 |= FCR31_C | FCR31_CV;
					execute_set_input(m_fpu_irq, ASSERT_LINE);
				}
				else
					m_fcr31 &= ~FCR31_C;
				break;
			case 0x3a: // C.SEQ.S (signalling equal)
				if (f32_eq(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) }))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;

				if (softfloat_exceptionFlags & softfloat_flag_invalid)
				{
					m_fcr31 |= FCR31_CV;
					execute_set_input(m_fpu_irq, ASSERT_LINE);
				}
				break;
			case 0x3b: // C.NGL.S (not greater or less than)
				if (f32_eq(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) }) || (softfloat_exceptionFlags & softfloat_flag_invalid))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;

				if (softfloat_exceptionFlags & softfloat_flag_invalid)
				{
					m_fcr31 |= FCR31_CV;
					execute_set_input(m_fpu_irq, ASSERT_LINE);
				}
				break;
			case 0x3c: // C.LT.S (less than)
				if (f32_lt(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) }))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;

				if (softfloat_exceptionFlags & softfloat_flag_invalid)
				{
					m_fcr31 |= FCR31_CV;
					execute_set_input(m_fpu_irq, ASSERT_LINE);
				}
				break;
			case 0x3d: // C.NGE.S (not greater or equal)
				if (f32_lt(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) }) || (softfloat_exceptionFlags & softfloat_flag_invalid))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;

				if (softfloat_exceptionFlags & softfloat_flag_invalid)
				{
					m_fcr31 |= FCR31_CV;
					execute_set_input(m_fpu_irq, ASSERT_LINE);
				}
				break;
			case 0x3e: // C.LE.S (less than or equal)
				if (f32_le(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) }))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;

				if (softfloat_exceptionFlags & softfloat_flag_invalid)
				{
					m_fcr31 |= FCR31_CV;
					execute_set_input(m_fpu_irq, ASSERT_LINE);
				}
				break;
			case 0x3f: // C.NGT.S (not greater than)
				if (f32_le(float32_t{ u32(m_f[FSREG >> 1]) }, float32_t{ u32(m_f[FTREG >> 1]) }) || (softfloat_exceptionFlags & softfloat_flag_invalid))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;

				if (softfloat_exceptionFlags & softfloat_flag_invalid)
				{
					m_fcr31 |= FCR31_CV;
					execute_set_input(m_fpu_irq, ASSERT_LINE);
				}
				break;

			default: // unimplemented operation
				m_fcr31 |= FCR31_CE;
				execute_set_input(m_fpu_irq, ASSERT_LINE);
				break;
			}
			break;
		case 0x11: // D
			switch (op & 0x3f)
			{
			case 0x00: // ADD.D
				set_cop1_reg(FDREG >> 1, f64_add(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] }).v);
				break;
			case 0x01: // SUB.D
				set_cop1_reg(FDREG >> 1, f64_sub(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] }).v);
				break;
			case 0x02: // MUL.D
				set_cop1_reg(FDREG >> 1, f64_mul(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] }).v);
				break;
			case 0x03: // DIV.D
				set_cop1_reg(FDREG >> 1, f64_div(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] }).v);
				break;

			case 0x05: // ABS.D
				if (f64_lt(float64_t{ m_f[FSREG >> 1] }, float64_t{ 0 }))
					set_cop1_reg(FDREG >> 1, f64_mul(float64_t{ m_f[FSREG >> 1] }, i32_to_f64(-1)).v);
				else
					set_cop1_reg(FDREG >> 1, m_f[FSREG >> 1]);
				break;
			case 0x06: // MOV.D
				m_f[FDREG >> 1] = m_f[FSREG >> 1];
				break;
			case 0x07: // NEG.D
				set_cop1_reg(FDREG >> 1, f64_mul(float64_t{ m_f[FSREG >> 1] }, i32_to_f64(-1)).v);
				break;

			case 0x20: // CVT.S.D
				set_cop1_reg(FDREG >> 1, f64_to_f32(float64_t{ m_f[FSREG >> 1] }).v);
				break;
			case 0x24: // CVT.W.D
				set_cop1_reg(FDREG >> 1, f64_to_i32(float64_t{ m_f[FSREG >> 1] }, softfloat_roundingMode, true));
				break;

			case 0x30: // C.F.D (false)
				m_fcr31 &= ~FCR31_C;
				break;
			case 0x31: // C.UN.D (unordered)
				f64_eq(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] });
				if (softfloat_exceptionFlags & softfloat_flag_invalid)
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;
				break;
			case 0x32: // C.EQ.D (equal)
				if (f64_eq(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] }))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;
				break;
			case 0x33: // C.UEQ.D (unordered equal)
				if (f64_eq(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] }) || (softfloat_exceptionFlags & softfloat_flag_invalid))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;
				break;
			case 0x34: // C.OLT.D (less than)
				if (f64_lt(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] }))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;
				break;
			case 0x35: // C.ULT.D (unordered less than)
				if (f64_lt(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] }) || (softfloat_exceptionFlags & softfloat_flag_invalid))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;
				break;
			case 0x36: // C.OLE.D (less than or equal)
				if (f64_le(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] }))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;
				break;
			case 0x37: // C.ULE.D (unordered less than or equal)
				if (f64_le(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] }) || (softfloat_exceptionFlags & softfloat_flag_invalid))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;
				break;

			case 0x38: // C.SF.D (signalling false)
				f64_eq(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] });

				m_fcr31 &= ~FCR31_C;

				if (softfloat_exceptionFlags & softfloat_flag_invalid)
				{
					m_fcr31 |= FCR31_CV;
					execute_set_input(m_fpu_irq, ASSERT_LINE);
				}
				break;
			case 0x39: // C.NGLE.D (not greater, less than or equal)
				f64_eq(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] });

				if (softfloat_exceptionFlags & softfloat_flag_invalid)
				{
					m_fcr31 |= FCR31_C | FCR31_CV;
					execute_set_input(m_fpu_irq, ASSERT_LINE);
				}
				else
					m_fcr31 &= ~FCR31_C;
				break;
			case 0x3a: // C.SEQ.D (signalling equal)
				if (f64_eq(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] }))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;

				if (softfloat_exceptionFlags & softfloat_flag_invalid)
				{
					m_fcr31 |= FCR31_CV;
					execute_set_input(m_fpu_irq, ASSERT_LINE);
				}
				break;
			case 0x3b: // C.NGL.D (not greater or less than)
				if (f64_eq(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] }) || (softfloat_exceptionFlags & softfloat_flag_invalid))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;

				if (softfloat_exceptionFlags & softfloat_flag_invalid)
				{
					m_fcr31 |= FCR31_CV;
					execute_set_input(m_fpu_irq, ASSERT_LINE);
				}
				break;
			case 0x3c: // C.LT.D (less than)
				if (f64_lt(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] }))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;

				if (softfloat_exceptionFlags & softfloat_flag_invalid)
				{
					m_fcr31 |= FCR31_CV;
					execute_set_input(m_fpu_irq, ASSERT_LINE);
				}
				break;
			case 0x3d: // C.NGE.D (not greater or equal)
				if (f64_lt(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] }) || (softfloat_exceptionFlags & softfloat_flag_invalid))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;

				if (softfloat_exceptionFlags & softfloat_flag_invalid)
				{
					m_fcr31 |= FCR31_CV;
					execute_set_input(m_fpu_irq, ASSERT_LINE);
				}
				break;
			case 0x3e: // C.LE.D (less than or equal)
				if (f64_le(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] }))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;

				if (softfloat_exceptionFlags & softfloat_flag_invalid)
				{
					m_fcr31 |= FCR31_CV;
					execute_set_input(m_fpu_irq, ASSERT_LINE);
				}
				break;
			case 0x3f: // C.NGT.D (not greater than)
				if (f64_le(float64_t{ m_f[FSREG >> 1] }, float64_t{ m_f[FTREG >> 1] }) || (softfloat_exceptionFlags & softfloat_flag_invalid))
					m_fcr31 |= FCR31_C;
				else
					m_fcr31 &= ~FCR31_C;

				if (softfloat_exceptionFlags & softfloat_flag_invalid)
				{
					m_fcr31 |= FCR31_CV;
					execute_set_input(m_fpu_irq, ASSERT_LINE);
				}
				break;

			default: // unimplemented operation
				m_fcr31 |= FCR31_CE;
				execute_set_input(m_fpu_irq, ASSERT_LINE);
				break;
			}
			break;
		case 0x14: // W
			switch (op & 0x3f)
			{
			case 0x20: // CVT.S.W
				set_cop1_reg(FDREG >> 1, i32_to_f32(s32(m_f[FSREG >> 1])).v);
				break;
			case 0x21: // CVT.D.W
				set_cop1_reg(FDREG >> 1, i32_to_f64(s32(m_f[FSREG >> 1])).v);
				break;

			default: // unimplemented operation
				m_fcr31 |= FCR31_CE;
				execute_set_input(m_fpu_irq, ASSERT_LINE);
				break;
			}
			break;

		default: // unimplemented operation
			m_fcr31 |= FCR31_CE;
			execute_set_input(m_fpu_irq, ASSERT_LINE);
			break;
		}
		break;
	case 0x31: // LWC1
		load<u32>(SIMMVAL + m_r[RSREG],
			[this, op](u32 data)
		{
			if (FTREG & 1)
				// load the high half of the floating point register
				m_f[FTREG >> 1] = (u64(data) << 32) | u32(m_f[FTREG >> 1]);
			else
				// load the low half of the floating point register
				m_f[FTREG >> 1] = (m_f[FTREG >> 1] & ~0xffffffffULL) | data;
		});
		break;
	case 0x39: // SWC1
		if (FTREG & 1)
			// store the high half of the floating point register
			store<u32>(SIMMVAL + m_r[RSREG], m_f[FTREG >> 1] >> 32);
		else
			// store the low half of the floating point register
			store<u32>(SIMMVAL + m_r[RSREG], m_f[FTREG >> 1]);
		break;
	}
}

template <typename T> void mips1_device_base::set_cop1_reg(unsigned const reg, T const data)
{
	// translate softfloat exception flags to cause register
	if (softfloat_exceptionFlags)
	{
		if (softfloat_exceptionFlags & softfloat_flag_inexact)
			m_fcr31 |= FCR31_CI;
		if (softfloat_exceptionFlags & softfloat_flag_underflow)
			m_fcr31 |= FCR31_CU;
		if (softfloat_exceptionFlags & softfloat_flag_overflow)
			m_fcr31 |= FCR31_CO;
		if (softfloat_exceptionFlags & softfloat_flag_infinite)
			m_fcr31 |= FCR31_CZ;
		if (softfloat_exceptionFlags & softfloat_flag_invalid)
			m_fcr31 |= FCR31_CV;

		// set flags
		m_fcr31 |= ((m_fcr31 & FCR31_CM) >> 10);

		// update exception state
		bool const exception = (m_fcr31 & FCR31_CE) || ((m_fcr31 & FCR31_CM) >> 5) & (m_fcr31 & FCR31_EM);
		execute_set_input(m_fpu_irq, exception ? ASSERT_LINE : CLEAR_LINE);

		if (exception)
			return;
	}

	if (sizeof(T) == 4)
		m_f[reg] = (m_f[reg] & ~0xffffffffULL) | data;
	else
		m_f[reg] = data;
}

bool mips1_device_base::memory_translate(int spacenum, int intention, offs_t &address)
{
	// check for kernel memory address
	if (BIT(address, 31))
	{
		// check debug or kernel mode
		if ((intention & TRANSLATE_DEBUG_MASK) || !(SR & SR_KUc))
		{
			switch (address & 0xe0000000)
			{
			case 0x80000000: // kseg0: unmapped, cached, privileged
			case 0xa0000000: // kseg1: unmapped, uncached, privileged
				address &= ~0xe0000000;
				return true;

			case 0xc0000000: // kseg2: mapped, cached, privileged
			case 0xe0000000:
				break;
			}
		}
		else if (SR & SR_KUc)
		{
			address_error(intention, address);

			return false;
		}
	}

	if (m_cpurev == 0x3927) { //&& address >= 0xfffe0000 && address <= 0xffff0000) {
		// TX3927 peripherals
		return true;
	}

	// key is a combination of VPN and ASID
	u32 const key = (address & EH_VPN) | (m_cop0[COP0_EntryHi] & EH_ASID);

	unsigned *mru = m_tlb_mru[intention & TRANSLATE_TYPE_MASK];

	bool refill = !BIT(address, 31);
	bool modify = false;

	for (unsigned i = 0; i < std::size(m_tlb); i++)
	{
		unsigned const index = mru[i];
		u32 const *const entry = m_tlb[index];

		// test vpn and optionally asid
		u32 const mask = (entry[1] & EL_G) ? EH_VPN : EH_VPN | EH_ASID;
		if ((entry[0] & mask) != (key & mask))
			continue;

		// test valid
		if (!(entry[1] & EL_V))
		{
			refill = false;
			break;
		}

		// test dirty
		if ((intention & TRANSLATE_WRITE) && !(entry[1] & EL_D))
		{
			refill = false;
			modify = true;
			break;
		}

		// translate the address
		address &= ~EH_VPN;
		address |= (entry[1] & EL_PFN);

		// promote the entry in the mru index
		if (i > 0)
			std::swap(mru[i - 1], mru[i]);

		return true;
	}

	if (!machine().side_effects_disabled() && !(intention & TRANSLATE_DEBUG_MASK))
	{
		if (VERBOSE & LOG_TLB)
		{
			if (modify)
				LOGMASKED(LOG_TLB, "asid %2d tlb modify address 0x%08x (%s)\n",
					(m_cop0[COP0_EntryHi] & EH_ASID) >> 6, address, machine().describe_context());
			else
				LOGMASKED(LOG_TLB, "asid %2d tlb miss %c address 0x%08x (%s)\n",
					(m_cop0[COP0_EntryHi] & EH_ASID) >> 6, (intention & TRANSLATE_WRITE) ? 'w' : 'r', address, machine().describe_context());
		}

		// load tlb exception registers
		m_cop0[COP0_BadVAddr] = address;
		m_cop0[COP0_EntryHi] = key;
		m_cop0[COP0_Context] = (m_cop0[COP0_Context] & PTE_BASE) | ((address >> 10) & BAD_VPN);

		generate_exception(modify ? EXCEPTION_TLBMOD : (intention & TRANSLATE_WRITE) ? EXCEPTION_TLBSTORE : EXCEPTION_TLBLOAD, refill);
	}

	return false;
}

////////////////////////////////

tx3927_sio::tx3927_sio(const machine_config& mconfig, const char* tag, device_t* owner, uint32_t clock) :
	device_t(mconfig, TX3927_SIO, tag, owner, clock),
	device_serial_interface(mconfig, *this),
	m_irq_handler(*this),
	m_txd_handler(*this),
	m_dtr_handler(*this),
	m_rts_handler(*this)
{
}

void tx3927_sio::device_start()
{
	m_timer = timer_alloc(0);
}

void tx3927_sio::device_reset()
{
	m_sifcr = 0;
	m_sidisr = 0x2000;

	m_silcr = 0;
	m_sidicr = 0;
	m_siscisr = 0b000110;
	m_siflcr = 0b0000000110000010;
	m_sibgr = 0x3ff;

	m_data_bits_count = 8;

	std::fill(std::begin(m_sitfifo), std::end(m_sitfifo), 0);
	m_sitfifo_len = 0;

	std::fill(std::begin(m_sirfifo), std::end(m_sirfifo), 0);
	m_sirfifo_len = 0;
}

void tx3927_sio::device_resolve_objects()
{
	// resolve callbacks
	m_irq_handler.resolve_safe();
	m_txd_handler.resolve_safe();
	m_rts_handler.resolve_safe();
	m_dtr_handler.resolve_safe();
}

void tx3927_sio::device_timer(emu_timer& timer, device_timer_id tid, int param)
{
	transmit_clock(false);
	sio_timer_adjust();
}

void tx3927_sio::transmit_clock(bool is_cts)
{
	// TODO: Support transmit enable select where CTS* hardware signal is used

	if (is_transmit_register_empty() && m_sitfifo_len > 0)
	{
		// If TSDE is set then SIO halts transmission until the bit is cleared
		if (BIT(m_siflcr, SIFLCR_TSDE))
			return;

		auto r = m_sitfifo[0];
		std::copy(std::begin(m_sitfifo) + 1, std::end(m_sitfifo), std::begin(m_sitfifo));
		m_sitfifo_len--;
		transmit_register_setup(r);
	}

	// Bits are only transmitted every 16 SIOCLK cycles
	if (m_transmit_bit != 0) {
		if (m_transmit_bit == 15) {
			m_transmit_bit = 0;
		}

		return;
	}
	else {
		m_transmit_bit++;
	}

	if (!is_transmit_register_empty())
	{
		uint8_t data = transmit_register_get_data_bit();
		LOGMASKED(LOG_TX39_SIO, "Tx Present a %d\n", data);
		m_txd_handler(data);
	}
}

WRITE_LINE_MEMBER(tx3927_sio::write_rxd)
{
	if (!BIT(m_siflcr, SIFLCR_RSDE)) {
		return;
	}
	// TODO?: The receiver controller looks for the high-to-low transition of a start bit on the RXD pin. A Low on
	// RXD is not treated as a start bit at the time when the SIFLCR.RSDE bit is cleared.When a valid start
	// bit has been detected, the receive controller begins sampling data received on the RXD pin

	LOGMASKED(LOG_TX39_SIO, "sio: Presented a %02x\n", state);

	m_recv_timeout_counter = machine().time();

	receive_register_update_bit(state);

	if (is_receive_register_full())
	{
		receive_register_extract();

		auto c = get_received_char();
		auto status = 0;

		if (is_receive_parity_error()) {
			status |= 1 << (SIDISR_UPER - SIDISR_UOER);
		}

		if (is_receive_framing_error()) {
			status |= 1 << (SIDISR_UFER - SIDISR_UOER);

			m_siscisr |= 1 << SISCISR_UBRKD;
			m_siscisr |= 1 << SISCISR_RBRKD;

			if (BIT(m_sidicr, SIDICR_STIE_UBRKD) || BIT(m_sidicr, SIDICR_STIE_RBRKD)) {
				// Set STIS when UBRKD or RBRKD is triggered
				m_sidisr |= 1 << SIDISR_STIS;
			}
		}
		else {
			// Automatically cleared when a non-break frame is received
			m_siscisr |= ~(1 << SISCISR_RBRKD);
		}

		if (m_sirfifo_len < 16) {
			m_sirfifo[m_sirfifo_len] = (status << 8) | c;
			m_sirfifo_len++;
		}
		else {
			// Overrun status bit of the 16th byte in the receive FIFO is set when the buffer is 100% full
			m_sirfifo[15] |= 1 << (SIDISR_UOER - SIDISR_UOER);
		}

		// TODO: Set flags as required for received byte(?)
		m_siflcr |= 1 << SIFLCR_RTSSC; // Software control RTS

		if (BIT(m_siflcr, SIFLCR_RCS) && m_sirfifo_len >= BIT(m_siflcr, SIFLCR_RTSTL, 4) && BIT(m_siflcr, SIFLCR_RTSTL, 4) != 0) {
			// Also needs hardware control RTS to be triggered
			m_rts_handler(1);
		}
	}
}

WRITE_LINE_MEMBER(tx3927_sio::write_cts)
{
	if (BIT(m_sidicr, SIDICR_STIE_CTSAC) && BIT(m_sidicr, SIDICR_CTSAC, 2) != 0) {
		bool t;

		if (BIT(m_sidicr, SIDICR_CTSAC, 2) == 1) {
			// Falling edge on the CTS* pin
			t = m_cts && !state;
		}
		else if (BIT(m_sidicr, SIDICR_CTSAC, 2) == 2) {
			// Rising edge on the CTS* pin
			t = !m_cts && state;
		}
		else {
			// Both rising and falling edges on the CTS* pin
			t = true;
		}

		if (t) // Sets STIS to 1 when the change specified by CTSAC occurs in CTSS
			m_sidisr |= 1 << SIDISR_STIS;
	}

	m_cts = state;
	m_siscisr |= 1 << SISCISR_CTSS;
	transmit_clock(true);
}

void tx3927_sio::sio_timer_adjust()
{
	constexpr int clock_divs[4] = { 2, 8, 32, 128 };
	const auto clock_div = clock_divs[BIT(m_sibgr, 8, 2)];
	const auto brd_div = m_sibgr & 0xff;
	const auto imclk = attotime::from_hz(133000000.0 / 4);
	const auto sclk = attotime::from_hz(133000000.0 / 4);
	const auto target_clock = BIT(m_clock_sel, 1) ? sclk : imclk;

	if (BIT(m_clock_sel, 0)) {
		attotime n_time = attotime::never;

		// Baud rate generator
		if (clock_div != 0 && brd_div != 0)
		{
			n_time = attotime::from_hz(target_clock.as_hz() / clock_div / brd_div / 16);
			//LOGMASKED(LOG_TX39_SIO, "sio_timer_adjust( %s ) = %s ( %d x %d )\n", tag(), n_time.as_string(), clock_div, brd_div);
		}
		else {
			//LOGMASKED(LOG_TX39_SIO, "sio_timer_adjust( %s ) invalid baud rate ( %d x %d )\n", tag(), clock_div, brd_div);
		}

		m_timer->adjust(n_time);
	}
	else {
		// Internal/external clock
		m_timer->adjust(target_clock);
	}
}

uint32_t tx3927_sio::read(offs_t offset, uint32_t mem_mask)
{
	auto sio_offset = (offset & 0x3f) * 4;

	//LOGMASKED(LOG_TX39_SIO, "%s: sio_read %08x %08x | %04x\n", machine().describe_context().c_str(), offset * 4, mem_mask, sio_offset);

	switch (sio_offset) {
	case 0x00:
		return m_silcr;

	case 0x04:
		return m_sidicr;

	case 0x08: {
		m_sidisr &= ~0x0f;
		m_sidisr |= m_sirfifo_len;

		if (8 - m_sitfifo_len > 0) {
			// Transmit Data Empty (has at least 1 empty location)
			m_siscisr |= 1 << SISCISR_TRDY;

			if (BIT(m_sidicr, SIDICR_STIE_TRDY)) // Sets STIS to 1 when TRDY is set
				m_sidisr |= 1 << SIDISR_STIS;
		}
		else {
			m_siscisr &= ~(1 << SISCISR_TRDY);
		}

		if (m_sitfifo_len <= 0 && is_transmit_register_empty()) {
			// Transmission Complete
			m_siscisr |= 1 << SISCISR_TXALS;

			if (BIT(m_sidicr, SIDICR_STIE_TXALS)) // Sets STIS to 1 when TXALS is set
				m_sidisr |= 1 << SIDISR_STIS;
		}
		else {
			m_siscisr &= ~(1 << SISCISR_TXALS);
		}

		const int sitfifo_len_limits[4] = { 1, 4, 8, 0 };
		if (BIT(m_sifcr, SIFCR_TDIL, 2) != 3 && 8 - m_sitfifo_len >= sitfifo_len_limits[BIT(m_sifcr, SIFCR_TDIL, 2)]) {
			// Transmit Data Empty
			m_sidisr |= 1 << SIDISR_TDIS;

			if (BIT(m_sidicr, SIDICR_TIE) && !BIT(m_sidicr, SIDICR_TDE)) {
				// Assert SITXIREQ (IRQ)
			}
			else if (!BIT(m_sidicr, SIDICR_TIE) && BIT(m_sidicr, SIDICR_TDE)) {
				// Assert SITXDREQ (DMA)
			}
		}

		const int sirfifo_len_limits[4] = { 1, 4, 8, 12 };
		if (m_sirfifo_len >= sirfifo_len_limits[BIT(m_sifcr, SIFCR_RDIL, 2)]) {
			// Receive Data Full
			m_sidisr |= 1 << SIDISR_RDIS;

			if (BIT(m_sidicr, SIDICR_RIE) && !BIT(m_sidicr, SIDICR_RDE)) {
				// Assert SIRXIREQ (IRQ)
			}
			else if (!BIT(m_sidicr, SIDICR_RIE) && BIT(m_sidicr, SIDICR_RDE)) {
				// Assert SIRXDREQ (DMA)
			}
		}

		//LOGMASKED(LOG_TX39_SIO, "sio data %08x\n", m_sidisr);

		return m_sidisr;
	}

	case 0x0c:
		if (8 - m_sitfifo_len > 0) {
			// Transmit Data Empty (has at least 1 empty location)
			m_siscisr |= 1 << SISCISR_TRDY;

			if (BIT(m_sidicr, SIDICR_STIE_TRDY)) // Sets STIS to 1 when TRDY is set
				m_sidisr |= 1 << SIDISR_STIS;
		}
		else {
			m_siscisr &= ~(1 << SISCISR_TRDY);
		}

		if (m_sitfifo_len <= 0 && is_transmit_register_empty()) {
			// Transmission Complete
			m_siscisr |= 1 << SISCISR_TXALS;

			if (BIT(m_sidicr, SIDICR_STIE_TXALS)) // Sets STIS to 1 when TXALS is set
				m_sidisr |= 1 << SIDISR_STIS;
		}
		else {
			m_siscisr &= ~(1 << SISCISR_TXALS);
		}

		//LOGMASKED(LOG_TX39_SIO, "sio data %08x\n", m_siscisr);

		return m_siscisr;
	case 0x10:
		return m_sifcr;

	case 0x14:
		return m_siflcr;

	case 0x18:
		return m_sibgr;

	case 0x20: {
		uint32_t r = 0;

		if (m_sirfifo_len > 0) {
			r = m_sirfifo[0] & 0xff;

			auto status = m_sirfifo[0] >> 8;
			std::copy(std::begin(m_sirfifo) + 1, std::end(m_sirfifo), std::begin(m_sirfifo));
			m_sirfifo_len--;

			if (BIT(status, SIDISR_UOER - SIDISR_UOER)) {
				m_sidisr |= 1 << SIDISR_UOER;
				m_sidisr |= 1 << SIDISR_ERI;
			}
			else {
				m_sidisr &= ~(1 << SIDISR_UOER);
			}

			if (BIT(status, SIDISR_UPER - SIDISR_UOER)) {
				m_sidisr |= 1 << SIDISR_UPER;
				m_sidisr |= 1 << SIDISR_ERI;
			}
			else {
				m_sidisr &= ~(1 << SIDISR_UPER);
			}

			if (BIT(status, SIDISR_UFER - SIDISR_UOER)) {
				m_sidisr |= 1 << SIDISR_UFER;
				m_sidisr |= 1 << SIDISR_ERI;
			}
			else {
				m_sidisr &= ~(1 << SIDISR_UFER);
			}

			if (BIT(status, SIDISR_UBRK - SIDISR_UOER)) {
				m_sidisr |= 1 << SIDISR_UBRK;
			}
			else {
				m_sidisr &= ~(1 << SIDISR_UBRK);
			}

			if (m_sirfifo_len > 0) {
				m_sidisr |= 1 << SIDISR_UVALID;
			}
			else {
				m_sidisr &= ~(1 << SIDISR_UVALID);
			}
		}
		else {
			// Error Interrupt
			m_sidisr |= 1 << SIDISR_ERI;
		}

		if (!BIT(m_sidicr, SIDICR_RDE) && BIT(m_sidicr, SIDICR_RIE) && (BIT(m_sidisr, SIDISR_ERI) || BIT(m_sidisr, SIDISR_TOUT))) {
			// TODO: Receive data serial interrupt
			//m_status |= SIO_STATUS_IRQ;
			m_irq_handler(1);
		}
		else if (BIT(m_sidicr, SIDICR_RDE) && !BIT(m_sidicr, SIDICR_RIE) && (BIT(m_sidisr, SIDISR_RDIS) || BIT(m_sidisr, SIDISR_TOUT))) {
			// TODO: Receive data DMA interrupt
		}

		if (BIT(m_sidicr, SIDICR_SPIE) && BIT(m_sidisr, SIDISR_ERI)) {
			// TODO: Assert SISPIREQ
		}

		return r;
	}
	}

	return 0;
}

void tx3927_sio::write(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	auto sio_offset = (offset & 0x3f) * 4;

	LOGMASKED(LOG_TX39_SIO, "%s: sio_write %08x %08x %08x\n", machine().describe_context().c_str(), offset * 4, data, mem_mask);

	switch (sio_offset) {
	case 0x00:
	{
		//auto txd_open_drain_enable = BIT(m_silcr, 13); // Multidrop only
		//auto transmit_wakeup_bit = BIT(m_silcr, 14); // Multidrop only
		//auto receive_wakeup_bit = BIT(m_silcr, 15); // Multidrop only

		auto stop_bits = BIT(data, 2) ? STOP_BITS_2 : STOP_BITS_1;
		auto parity_enabled = BIT(data, 3) && !BIT(data, 1);
		auto parity = parity_enabled ? (BIT(data, 4) ? PARITY_EVEN : PARITY_ODD) : PARITY_ODD;
		m_data_bits_count = BIT(data, 0) ? 7 : 8;
		set_data_frame(1, m_data_bits_count, parity, stop_bits);

		if (BIT(data, 5, 2) != m_clock_sel) {
			m_clock_sel = BIT(data, 5, 2);
			sio_timer_adjust();
		}

		m_silcr = data;
		break;
	}

	case 0x04:
		m_sidicr = data;
		break;

	case 0x08:
		m_sidisr = (m_sidisr & 0xf800) | (data & ~0xf800);
		break;

	case 0x0c:
		m_siscisr = (m_siscisr & ~0x21) | (data & 0x21);
		break;

	case 0x10: {
		if (BIT(data, SIFCR_SWRST)) {
			// TODO: SIO reset
			data &= ~(1 << SIFCR_SWRST);
		}

		if (BIT(data, SIFCR_FRSTE) && BIT(data, SIFCR_TFRST)) {
			// Transmit FIFO reset
			data &= ~(1 << SIFCR_TFRST);
			m_sitfifo_len = 0;
		}

		if (BIT(data, SIFCR_FRSTE) && BIT(data, SIFCR_RFRST)) {
			// Receive FIFO reset
			data &= ~(1 << SIFCR_RFRST);
			m_sirfifo_len = 0;
		}

		m_sifcr = data;
		break;
	}

	case 0x14:
		m_siflcr = data;
		break;

	case 0x18:
		if (data != m_sibgr) {
			m_sibgr = data;
			sio_timer_adjust();
		}
		break;

	case 0x1c:
		LOGMASKED(LOG_TX39_SIO, "sio_write %08x %c\n", data, data & 0xff);

		if (m_sitfifo_len < 8) {
			//m_sitfifo[m_sitfifo_len++] = data;
		}

		break;
	}
}

DEFINE_DEVICE_TYPE(TX3927_SIO, tx3927_sio, "tx3927_sio", "Toshiba TX3927 Serial I/O")

////////////////////////////////

tx3927_device::tx3927_device(const machine_config& mconfig, const char* tag, device_t* owner, uint32_t clock, size_t icache_size, size_t dcache_size) :
	mips1_device_base(mconfig, TX3927, tag, owner, clock, 0x3927, icache_size, dcache_size),
	m_program_config("program", ENDIANNESS_BIG, 32, 32, 0, address_map_constructor(FUNC(tx3927_device::amap), this)),
	m_sio(*this, "sio%d", 0L)
{
}

void tx3927_device::device_add_mconfig(machine_config& config)
{
	mips1core_device_base::device_add_mconfig(config);

	TX3927_SIO(config, "sio0", 0);
	TX3927_SIO(config, "sio1", 0);
}

void tx3927_device::device_reset()
{
	mips1core_device_base::device_reset();

	m_irc_irscr = 0;
	m_irc_irssr = 0;
	m_irc_ircsr = (1 << IRCSR_IF) | 0x1f;
	m_irc_ircer = 0;
	m_irc_irimr = 0;
	std::fill(std::begin(m_irc_irilr_full), std::end(m_irc_irilr_full), 0);
	std::fill(std::begin(m_irc_irilr), std::end(m_irc_irilr), 0);
	std::fill(std::begin(m_irc_ircr), std::end(m_irc_ircr), 0);

	std::fill(std::begin(m_pio_flags), std::end(m_pio_flags), 0);

	for (int i = 0; i < 3; i++) {
		m_tmr[i].TMTCR = 0;
		m_tmr[i].TMTISR = 0;
		m_tmr[i].TMCPRA = 0xffffff;
		m_tmr[i].TMCPRB = 0xffffff;
		m_tmr[i].TMITMR = 0;
		m_tmr[i].TMCCDR = 0;
		m_tmr[i].TMPGMR = 0;
		m_tmr[i].TMWTMR = 0;
		m_tmr[i].TMTRR = 0;
	}

	m_ccfg = 0x0d;
	m_crir = 0x39270011;
	m_pcfg = 0;
	m_tear = 0;
	m_pdcr = 0;

	m_pci_istat = 0;
	m_pci_pcistat = 0x210;
	m_pcicmd = 0;
	m_pci_iba = 0;
	m_pci_mba = 0;
	m_pci_svid = 0;
	m_pci_ssvid = 0;
	m_pci_ml = 0xff;
	m_pci_mg = 0xff;
	m_pci_ip = 0x01;
	m_pci_il = 0x00;
	m_ipcidata = 0;
	m_pci_icmd = 0;
	m_pci_ibe = 0;
	m_pci_lbc = 0;
	m_pci_mmas = 0;
	m_pci_iomas = 0;
	m_pci_ipciaddr = 0;
	m_pci_ipcidata = 0;

	std::fill(std::begin(m_rom_rccr), std::end(m_rom_rccr), 0x1fc30000);
	m_rom_rccr[0] = 0x1fc3e280; // Should have BAI, B16, BBC, BME set based on input pins
	update_rom_config(0);
}

void tx3927_device::device_resolve_objects()
{
}

device_memory_interface::space_config_vector tx3927_device::memory_space_config() const
{
	return space_config_vector{
		std::make_pair(AS_PROGRAM, &m_program_config),
		std::make_pair(1, &m_icache_config),
		std::make_pair(2, &m_dcache_config)
	};
}

void tx3927_device::amap(address_map& map)
{
	map(0xfffe8000, 0xfffe8fff).rw(FUNC(tx3927_device::sdram_read), FUNC(tx3927_device::sdram_write));
	map(0xfffe9000, 0xfffe9fff).rw(FUNC(tx3927_device::rom_read), FUNC(tx3927_device::rom_write));
	map(0xfffeb000, 0xfffebfff).rw(FUNC(tx3927_device::dma_read), FUNC(tx3927_device::dma_write));
	map(0xfffec000, 0xfffecfff).rw(FUNC(tx3927_device::irc_read), FUNC(tx3927_device::irc_write));
	map(0xfffed000, 0xfffedfff).rw(FUNC(tx3927_device::pci_read), FUNC(tx3927_device::pci_write));
	map(0xfffee000, 0xfffeefff).rw(FUNC(tx3927_device::ccfg_read), FUNC(tx3927_device::ccfg_write));
	map(0xfffef000, 0xfffef2ff).rw(FUNC(tx3927_device::tmr_read), FUNC(tx3927_device::tmr_write));
	map(0xfffef300, 0xfffef3ff).rw(m_sio[0], FUNC(tx3927_sio::read), FUNC(tx3927_sio::write));
	map(0xfffef400, 0xfffef4ff).rw(m_sio[1], FUNC(tx3927_sio::read), FUNC(tx3927_sio::write));
	map(0xfffef500, 0xfffef5ff).rw(FUNC(tx3927_device::pio_read), FUNC(tx3927_device::pio_write));
}

void tx3927_device::device_start()
{
	mips1core_device_base::device_start();

	m_program = &space(AS_PROGRAM);

	m_timer[0] = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(tx3927_device::update_timer<0>), this));
	m_timer[1] = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(tx3927_device::update_timer<1>), this));
	m_timer[2] = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(tx3927_device::update_timer<2>), this));

	update_timer_speed();
}

constexpr double TX3927_TIMER_DIVISOR = 32;
void tx3927_device::update_timer_speed()
{
	// TODO: Add support for counter clock select

	for (int i = 0; i < 3; i++) {
		auto clock_speed = 133000000.0 / 4; // IMCLK speed is clock speed (133 MHz) / 4

		auto divisor = 0;
		if (BIT(m_tmr[i].TMTCR, TMTCR_CCDE)) {
			// Counter clock divide enable
			divisor = m_tmr[i].TMCCDR;
		}
		clock_speed /= std::pow(2, divisor + 1);

		// Divide clock speed further for performance reasons
		// The timer will tick up an equivalent amount to make up for the speed difference at the current divisor
		clock_speed /= TX3927_TIMER_DIVISOR;

		auto imclk = attotime::from_hz(clock_speed);
		m_timer[i]->adjust(attotime::zero, 0, imclk);
	}
}

void tx3927_device::trigger_irq(int irq, int state)
{
	// IRQ vector priority, highest to lowest
	//  0 INT[0]
	//  1 INT[1]
	//  2 INT[2]
	//  3 INT[3]
	//  4 INT[4]
	//  5 INT[5]
	//  6 SIO[0]
	//  7 SIO[1]
	//  8 DMA
	//  9 PIO
	// 10 PCI
	// 11 (Reserved)
	// 12 (Reserved)
	// 13 TMR[0]
	// 14 TMR[1]
	// 15 TMR[2]

	if (state) {
		m_irc_irssr |= 1 << irq;
	}
	else {
		m_irc_irssr &= ~(1 << irq);
		m_irc_ircsr = (1 << IRCSR_IF) | 0x1f;
	}

	if (!BIT(m_irc_ircer, 0)) // Interrupts disabled
		return;

	CAUSE &= ~CAUSE_IP;
	if (state) {
		// Find highest priority interrupt
		for (int i = 0; i < 16; i++) {
			int curlevel = 0;
			int curmask = BIT(m_irc_irimr, 0, 3);
			int curirq = BIT(m_irc_ircsr, IRCSR_IVL, 5);

			if (!BIT(m_irc_irssr, i))
				continue;

			if (m_irc_irilr[i] == 0) // Disabled IRQ
				continue;

			if (m_irc_irilr[i] < curmask) // Masked IRQ
				continue;

			if (!BIT(m_irc_ircsr, IRCSR_IF))
				curlevel = BIT(m_irc_ircsr, IRCSR_ILV, 3);

			auto accept = BIT(m_irc_ircsr, IRCSR_IF) // No IRQ
				|| curlevel == 0 // Disabled IRQ level
				|| m_irc_irilr[i] < curlevel // Higher priority
				|| (m_irc_irilr[i] == curlevel && i < curirq); // Same priority + lower interrupt vector
			if (accept) {
				// The IP[5] bit in the Cause register is set to 1 to indicate an interrupt
				// The IP[4:0] field captures the interrupt vector associated with its source
				CAUSE |= (i & 0xf) << 10;
				CAUSE |= CAUSE_IPEX5;
				m_irc_ircsr = (m_irc_irilr[irq] << 8) | irq;
			}
		}
	}
}

template <int N> TIMER_CALLBACK_MEMBER(tx3927_device::update_timer)
{
	if (BIT(m_tmr[N].TMTCR, TMTCR_TMODE, 2) == 3) {
		// Timer not enabled
		m_timer[N]->adjust(attotime::never);
		return;
	}

	if (BIT(m_tmr[N].TMTCR, TMTCR_TCE) && m_tmr[N].TMTRR < m_tmr[N].TMCPRA) {
		// Running the timer at the exact speed it needs to be causes huge issues with performance, so just increase the step for each tick
		m_tmr[N].TMTRR += TX3927_TIMER_DIVISOR;

		if (m_tmr[N].TMTRR > 0xffffff)
			m_tmr[N].TMTRR = 0xffffff;

		//LOGMASKED(LOG_TX39_TMR, "tmr[%d].TMTRR: %d %d | %d\n", N, m_tmr[N].TMTRR, m_tmr[N].TMCPRA, BIT(m_tmr[N].TMITMR, TMITMR_TIIE));
	}

	if (m_tmr[N].TMTRR >= m_tmr[N].TMCPRA) {
		if (BIT(m_tmr[N].TMITMR, TMITMR_TZCE)) {
			m_tmr[N].TMTRR = 0;
		}

		if (BIT(m_tmr[N].TMITMR, TMITMR_TIIE)) {
			// Timer Interval Interrupt Enabled
			trigger_irq(13 + N, ASSERT_LINE);
		}

		m_tmr[N].TMTISR |= 1 << TMTISR_TIIS; // Set interrupt on TIIS
	}
}

uint32_t tx3927_device::tmr_read(offs_t offset, uint32_t mem_mask)
{
	auto tmr_idx = (offset >> 6) & 3;
	auto tmr_offset = (offset & 0x3f) << 2;

	switch (tmr_offset) {
	case 0x00:
		return m_tmr[tmr_idx].TMTCR & 0xff;
	case 0x04:
		return m_tmr[tmr_idx].TMTISR & 0xf;
	case 0x08:
		return m_tmr[tmr_idx].TMCPRA & 0xffffff;
	case 0x0c:
		return m_tmr[tmr_idx].TMCPRB & 0xffffff;
	case 0x10:
		return m_tmr[tmr_idx].TMITMR & 0xffff;
	case 0x20:
		return m_tmr[tmr_idx].TMCCDR & 0x7;
	case 0x30:
		return m_tmr[tmr_idx].TMPGMR & 0xffff;
	case 0x40:
		if (tmr_idx == 2) {
			// Only exists for 3rd timer
			return m_tmr[tmr_idx].TMWTMR & 0xffff;
		}
		break;
	case 0xf0: {
		// Timer Read Register
		return m_tmr[tmr_idx].TMTRR & 0xffffff;
	}
	}

	LOGMASKED(LOG_TX39_TMR, "%s: tmr read %08x %08x\n", machine().describe_context().c_str(), offset * 4, mem_mask);
	return 0;
}

void tx3927_device::tmr_write(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	auto tmr_idx = (offset >> 6) & 3;
	auto tmr_offset = (offset & 0x3f) << 2;

	switch (tmr_offset) {
	case 0x00:
		m_tmr[tmr_idx].TMTCR = data & 0xff;

		if (BIT(m_tmr[tmr_idx].TMTCR, TMTCR_TCE) == 0 && BIT(m_tmr[tmr_idx].TMTCR, TMTCR_CRE)) {
			// Disable + reset enabled = zero counter
			LOGMASKED(LOG_TX39_TMR, "Timer %d counter reset\n", tmr_idx);
			m_tmr[tmr_idx].TMTRR = 0;
		}

		update_timer_speed();
		break;
	case 0x04:
		m_tmr[tmr_idx].TMTISR = data & 0xe;

		if (BIT(data, 0) == 0) { // Has no effect when 1 is written
			if (BIT(m_irc_irssr, 13 + tmr_idx)) {
				trigger_irq(13 + tmr_idx, CLEAR_LINE);
			}

			m_tmr[tmr_idx].TMTISR &= ~(1 << 0); // Unset interrupt
		}
		break;
	case 0x08:
		m_tmr[tmr_idx].TMCPRA = data & 0xffffff;
		break;
	case 0x0c:
		m_tmr[tmr_idx].TMCPRB = data & 0xffffff;
		break;
	case 0x10:
		m_tmr[tmr_idx].TMITMR = data & 0xffff;
		break;
	case 0x20:
		m_tmr[tmr_idx].TMCCDR = data & 0x7;
		update_timer_speed();
		break;
	case 0x30:
		m_tmr[tmr_idx].TMPGMR = data & 0xffff;
		break;
	case 0x40:
		if (tmr_idx == 2) {
			m_tmr[tmr_idx].TMWTMR = data & 0xffff;
		}
		break;
	}

	if (offset != 1)
		LOGMASKED(LOG_TX39_TMR, "%s: tmr write %08x %08x %08x\n", machine().describe_context().c_str(), offset * 4, data, mem_mask);
}

uint32_t tx3927_device::ccfg_read(offs_t offset, uint32_t mem_mask)
{
	LOGMASKED(LOG_TX39_CCFG, "%s: ccfg read %08x %08x\n", machine().describe_context().c_str(), offset * 4, mem_mask);
	switch (offset * 4) {
	case 0x00:
		// CCFG
		return m_ccfg;
	case 0x04:
		// CRIR
		return m_crir;
	case 0x08:
		// PCFG
		return m_pcfg;
	case 0x0c:
		// TEAR
		return m_tear;
	case 0x10:
		// PDCR
		return m_pdcr;
	}

	return 0;
}

void tx3927_device::ccfg_write(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	LOGMASKED(LOG_TX39_CCFG, "%s: ccfg write %08x %08x %08x\n", machine().describe_context().c_str(), offset * 4, data, mem_mask);

	switch (offset * 4) {
	case 0x00:
		// CCFG
		m_ccfg = (m_ccfg & ~0x3dc01) | (data & 0x3dc01);
		break;
	case 0x08:
		// PCFG
		m_pcfg = data & 0xfffffff;
		break;
	case 0x10:
		// PDCR
		m_pdcr = data & 0xffffff;
		break;
	}
}

uint32_t tx3927_device::sdram_read(offs_t offset, uint32_t mem_mask)
{
	LOGMASKED(LOG_TX39_SDRAM, "%s: sdram_read %08x %08x\n", machine().describe_context().c_str(), offset * 4, mem_mask);

	if ((offset * 4) == 0x20) {
		// SDCTR1
		return 0x400;
	}
	else if ((offset * 4) == 0x24) {
		// SDCTR2
		return 0xff;
	}

	return 0;
}

void tx3927_device::sdram_write(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	LOGMASKED(LOG_TX39_SDRAM, "%s: sdram_write %08x %08x %08x\n", machine().describe_context().c_str(), offset * 4, data, mem_mask);
}

uint32_t tx3927_device::rom_read(offs_t offset, uint32_t mem_mask)
{
	LOGMASKED(LOG_TX39_ROM, "%s: rom_read %08x %08x\n", machine().describe_context().c_str(), offset * 4, mem_mask);

	if (offset > 7)
		return 0;

	return m_rom_rccr[offset];
}

void tx3927_device::update_rom_config(int idx)
{
	auto base_addr = BIT(m_rom_rccr[idx], 20, 12) << 20;
	auto bus_width = BIT(m_rom_rccr[idx], 7) ? 1 : 2;
	auto channel_size = std::min(
		int(pow(2, BIT(m_rom_rccr[idx], 8, 4))) * 1024 * 1024 * bus_width,
		0x20000000
	);

	LOGMASKED(LOG_TX39_ROM, "ram[%d]: %08x | %06x | %08x -> %08x\n", idx, m_rom_rccr[idx], channel_size, base_addr, base_addr + channel_size - 1);

	/*
	if (m_ram[idx] && m_ram[idx]->pointer()) {
		m_program->install_ram(0x00000000 + base_addr, 0x00000000 + base_addr + channel_size - 1, m_ram[idx]->pointer());
		m_program->install_ram(0x80000000 + base_addr, 0x80000000 + base_addr + channel_size - 1, m_ram[idx]->pointer());
		m_program->install_ram(0xa0000000 + base_addr, 0xa0000000 + base_addr + channel_size - 1, m_ram[idx]->pointer());
	}
	*/
}

void tx3927_device::rom_write(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	LOGMASKED(LOG_TX39_ROM, "%s: rom_write %08x %08x %08x\n", machine().describe_context().c_str(), offset * 4, data, mem_mask);

	if (offset > 7)
		return;

	if (m_rom_rccr[offset] != data) {
		m_rom_rccr[offset] = data;
		update_rom_config(offset);
	}
}

uint32_t tx3927_device::dma_read(offs_t offset, uint32_t mem_mask)
{
	LOGMASKED(LOG_TX39_DMA, "%s: dma_read %08x %08x\n", machine().describe_context().c_str(), offset * 4, mem_mask);
	return 0;
}

void tx3927_device::dma_write(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	LOGMASKED(LOG_TX39_DMA, "%s: dma_write %08x %08x %08x\n", machine().describe_context().c_str(), offset * 4, data, mem_mask);
}

uint32_t tx3927_device::irc_read(offs_t offset, uint32_t mem_mask)
{
	uint32_t ret = 0;

	switch (offset * 4) {
	case 0x00:
		// Interrupt Control Enable Register
		ret = m_irc_ircer;
		break;
	case 0x04:
		// Interrupt Control Mode Register 0
		break;
	case 0x08:
		// Interrupt Control Mode Register 1
		break;
	case 0x10:
		// Interrupt Level 0 000000000111 HDD
		// Interrupt Level 1 011100000000 CD-ROM
		ret = m_irc_irilr_full[0];
		break;
	case 0x14:
		// Interrupt Level 2 000000000111
		// Interrupt Level 3 011100000000
		ret = m_irc_irilr_full[1];
		break;
	case 0x18:
		// Interrupt Level 4 000000000111
		// Interrupt Level 5 011100000000
		ret = m_irc_irilr_full[2];
		break;
	case 0x1c:
		// Interrupt Level 6 000000000111
		// Interrupt Level 7 011100000000
		ret = m_irc_irilr_full[3];
		break;
	case 0x20:
		// Interrupt Level 8 000000000111
		// Interrupt Level 9 011100000000
		ret = m_irc_irilr_full[4];
		break;
	case 0x24:
		// Interrupt Level 10 000000000111
		ret = m_irc_irilr_full[5];
		break;
	case 0x28:
		// Interrupt Level 13 011100000000
		ret = m_irc_irilr_full[6];
		break;
	case 0x2c:
		// Interrupt Level 14 000000000111
		// Interrupt Level 15 011100000000
		ret = m_irc_irilr_full[7];
		break;
	case 0x40:
		// Interrupt Mask Level
		ret = m_irc_irimr;
		break;
	case 0x60:
		// Interrupt Status/Control Register
		break;
	case 0x80:
		// Interrupt Source Status Register
		ret = m_irc_irssr;
		break;
	case 0xa0:
		// Interrupt Current Status Register
		ret = m_irc_ircsr;
		break;
	}

	LOGMASKED(LOG_TX39_IRC, "%s: irc_read %08x | %08x\n", machine().describe_context().c_str(), offset * 4, ret);

	return ret;
}

void tx3927_device::irc_write(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	LOGMASKED(LOG_TX39_IRC, "%s: irc_write %08x %08x %08x\n", machine().describe_context().c_str(), offset * 4, data, mem_mask);

	switch (offset * 4) {
	case 0x00:
		m_irc_ircer = data;
		break;
	case 0x04:
		for (int i = 0; i < 8; i++)
			m_irc_ircr[i] = BIT(data, i * 2, 2);
		break;
	case 0x08:
		for (int i = 0; i < 8; i++)
			m_irc_ircr[8+i] = BIT(data, i * 2, 2);
		break;
	case 0x10:
		// Interrupt Level 0 000000000111 HDD
		// Interrupt Level 1 011100000000 CD-ROM
		m_irc_irilr_full[0] = data;
		m_irc_irilr[0] = BIT(data, 0, 3);
		m_irc_irilr[1] = BIT(data, 8, 3);
		break;
	case 0x14:
		// Interrupt Level 2 000000000111
		// Interrupt Level 3 011100000000
		m_irc_irilr_full[1] = data;
		m_irc_irilr[2] = BIT(data, 0, 3);
		m_irc_irilr[3] = BIT(data, 8, 3);
		break;
	case 0x18:
		// Interrupt Level 4 000000000111
		// Interrupt Level 5 011100000000
		m_irc_irilr_full[2] = data;
		m_irc_irilr[4] = BIT(data, 0, 3);
		m_irc_irilr[5] = BIT(data, 8, 3);
		break;
	case 0x1c:
		// Interrupt Level 6 000000000111
		// Interrupt Level 7 011100000000
		m_irc_irilr_full[3] = data;
		m_irc_irilr[6] = BIT(data, 0, 3);
		m_irc_irilr[7] = BIT(data, 8, 3);
		break;
	case 0x20:
		// Interrupt Level 8 000000000111
		// Interrupt Level 9 011100000000
		m_irc_irilr_full[4] = data;
		m_irc_irilr[8] = BIT(data, 0, 3);
		m_irc_irilr[9] = BIT(data, 8, 3);
		break;
	case 0x24:
		// Interrupt Level 10 000000000111
		m_irc_irilr_full[5] = data;
		m_irc_irilr[10] = BIT(data, 0, 3);
		m_irc_irilr[11] = BIT(data, 8, 3);
		break;
	case 0x28:
		// Interrupt Level 13 011100000000
		m_irc_irilr_full[6] = data;
		m_irc_irilr[12] = BIT(data, 0, 3);
		m_irc_irilr[13] = BIT(data, 8, 3);
		break;
	case 0x2c:
		// Interrupt Level 14 000000000111
		// Interrupt Level 15 011100000000
		m_irc_irilr_full[7] = data;
		m_irc_irilr[14] = BIT(data, 0, 3);
		m_irc_irilr[15] = BIT(data, 8, 3);
		break;
	case 0x40:
		m_irc_irimr = data;
		break;
	case 0x60:
		m_irc_irscr = data & 0xffffefff;
		if (BIT(data, 8)) {
			auto source = BIT(m_irc_irscr, 0, 4);
			trigger_irq(source, CLEAR_LINE);
		}
		break;
	}
}

uint32_t tx3927_device::pci_read(offs_t offset, uint32_t mem_mask)
{
	auto r = 0;

	switch (offset * 4) {
	case 0x00: {
		// +002 Device ID Register (DID)
		// +000 Vendor ID Register (VID)
		constexpr uint16_t device_id = 0x000a; // TX3927
		constexpr uint16_t vendor_id = 0x102f; // Toshiba
		return (device_id << 16) | vendor_id;
	}

	case 0x04: {
		// +006 PCI Status Register (PCISTAT)
		// +004 PCI Command Register (PCICMD)
		return (m_pci_pcistat << 16) | m_pcicmd;
	}

	case 0x08: {
		// +00b Class Code Register (CC)
		// +00a Subclass Code Register (SCC)
		// +009 Register-Level Programming Interface Register (RLPI)
		// +008 Revision ID Register (RID)
		constexpr uint8_t class_code = 0x06;
		constexpr uint8_t subclass_code = 0x00;
		constexpr uint8_t rlpi = 0x00; // Register-Level Programming Interface
		constexpr uint8_t rev_id = 0; // ?
		return (class_code << 24) | (subclass_code << 16) | (rlpi << 8) | rev_id;
	}

	case 0x0c: {
		// +00e Header Type Register (HT)
		// +00d Master Latency Timer Register (MLT)
		// +00c Cache Line Size Register
		constexpr uint8_t mfht = 0; // Multi-Function and Header Type
		constexpr uint8_t mlt = 0x1f; // Master Latency Timer Count Value
		constexpr uint8_t cls = 0;
		return (mfht << 16) | (mlt << 8) | cls;
	}

	case 0x10: {
		// +010 Target I/O Base Address Register (IOBA)
		constexpr uint8_t imai = 1; //  I/O Base Address Indicator
		return (m_pci_iba << 2) | imai;
	}

	case 0x14: {
		// +014 Target Memory Base Address Register (MBA)
		constexpr uint8_t pf = 1; // Prefetchable
		constexpr uint8_t mty = 0; // Memory Type
		constexpr uint8_t mbai = 0; // Memory Base Address Indicator
		return (m_pci_mba << 4) | (pf << 3) | (mty << 1) | mbai;
	}

	case 0x2c: {
		// +02e System Vendor ID Register (SVID)
		// +02c Subsystem Vendor ID Register (SSVID)
		return (m_pci_svid << 16) | m_pci_ssvid;
	}

	case 0x34: {
		// +037 Capabilities Pointer (CAPPTR)
		constexpr uint8_t capptr = 0xe0;
		return capptr;
	}

	case 0x3c: {
		// +03f Maximum Latency Register (ML)
		// +03e Minimum Grant Register (MG)
		// +03d Interrupt Pin Register (IP)
		// +03c Interrupt Line Register (IL)
		return (m_pci_ml << 24) | (m_pci_mg << 16) | (m_pci_ip << 8) | m_pci_il;
	}

	case 0x44:
		r = m_pci_istat;
		break;

	case 0x154: {
		// Initiator Indirect Data Register (IPCIDATA)
		return m_ipcidata;
	}

	case 0x158: {
		// Initiator Indirect Command/Byte Enable Register (IPCICBE)
		return (m_pci_icmd << 4) | m_pci_ibe;
	}
	}

	LOGMASKED(LOG_TX39_PCI, "%s: pci_read %08x %08x\n", machine().describe_context().c_str(), offset * 4, r);

	return r;
}

void tx3927_device::pci_write(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	LOGMASKED(LOG_TX39_PCI, "%s: pci_write %08x %08x\n", machine().describe_context().c_str(), offset * 4, data);

	switch (offset * 4) {
	case 0x04:
		// PCI Status Register (PCISTAT)
		m_pci_pcistat = (m_pci_pcistat & 0x065f) | data;
		break;

	case 0x3c:
		// +03f Maximum Latency Register (ML)
		// +03e Minimum Grant Register (MG)
		// +03d Interrupt Pin Register (IP)
		// +03c Interrupt Line Register (IL)
		m_pci_ml = (data >> 24) & 0xff;
		m_pci_mg = (data >> 16) & 0xff;
		m_pci_ip = (data >> 8) & 0xff;
		m_pci_il = data & 0xff;
		break;

	case 0x44:
		// ISTAT register
		if (BIT(data, 12))
			m_pci_istat &= ~(1 << 12);

		if (BIT(data, 10))
			m_pci_istat &= ~(1 << 10);

		if (BIT(data, 9))
			m_pci_istat &= ~(1 << 9);
		break;

	case 0x128:
		// Local Bus Control Register (LBC)
		m_pci_lbc = data & 0xfffffffc;
		break;

	case 0x148:
		// Initiator Memory Mapping Address Size Register (MMAS)
		m_pci_mmas = data & 0xfffffffc;
		break;

	case 0x14c:
		// Initiator I/O Mapping Address Size Register (IOMAS)
		m_pci_iomas = data & 0xfffffffc;
		break;

	case 0x150:
		// Initiator Indirect Address Register (IPCIADDR)
		m_pci_ipciaddr = data;
		break;

	case 0x154:
		// Initiator Indirect Data Register (IPCIDATA)
		m_pci_ipcidata = data;
		break;

	case 0x158:
		// Initiator Indirect Command/Byte Enable Register (IPCICBE)
		m_pci_icmd = (data >> 4) & 0x0f;
		m_pci_ibe = data & 0x0f;
		m_pci_istat |= 1 << 12;
		break;
	}
}

uint32_t tx3927_device::pio_read(offs_t offset, uint32_t mem_mask)
{
	if (offset != 0)
		LOGMASKED(LOG_TX39_PIO, "%s: pio_read %08x %08x\n", machine().describe_context().c_str(), offset * 4, mem_mask);
	return m_pio_flags[offset];
}

void tx3927_device::pio_write(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	if (offset != 0)
		LOGMASKED(LOG_TX39_PIO, "%s: pio_write %08x %08x %08x\n", machine().describe_context().c_str(), offset * 4, data, mem_mask);
	m_pio_flags[offset] = data;
}
