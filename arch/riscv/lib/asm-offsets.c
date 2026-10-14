// SPDX-License-Identifier: GPL-2.0-only

/*
 * Generate definitions needed by assembly language modules.
 * This code generates raw asm output which is post-processed to extract
 * and format the required data.
 */

#include <linux/kbuild.h>
#include <linux/kernel.h>
#include <asm/ptrace.h>
#include <asm/system.h>
#include <pbl/entry.h>

#define STACK_ALIGN		16

int main(void)
{
	DEFINE(PT_SIZE, sizeof(struct pt_regs));
	OFFSET(PT_EPC, pt_regs, epc);
	OFFSET(PT_RA, pt_regs, ra);
	OFFSET(PT_FP, pt_regs, s0);
	OFFSET(PT_S0, pt_regs, s0);
	OFFSET(PT_S1, pt_regs, s1);
	OFFSET(PT_S2, pt_regs, s2);
	OFFSET(PT_S3, pt_regs, s3);
	OFFSET(PT_S4, pt_regs, s4);
	OFFSET(PT_S5, pt_regs, s5);
	OFFSET(PT_S6, pt_regs, s6);
	OFFSET(PT_S7, pt_regs, s7);
	OFFSET(PT_S8, pt_regs, s8);
	OFFSET(PT_S9, pt_regs, s9);
	OFFSET(PT_S10, pt_regs, s10);
	OFFSET(PT_S11, pt_regs, s11);
	OFFSET(PT_SP, pt_regs, sp);
	OFFSET(PT_TP, pt_regs, tp);
	OFFSET(PT_A0, pt_regs, a0);
	OFFSET(PT_A1, pt_regs, a1);
	OFFSET(PT_A2, pt_regs, a2);
	OFFSET(PT_A3, pt_regs, a3);
	OFFSET(PT_A4, pt_regs, a4);
	OFFSET(PT_A5, pt_regs, a5);
	OFFSET(PT_A6, pt_regs, a6);
	OFFSET(PT_A7, pt_regs, a7);
	OFFSET(PT_T0, pt_regs, t0);
	OFFSET(PT_T1, pt_regs, t1);
	OFFSET(PT_T2, pt_regs, t2);
	OFFSET(PT_T3, pt_regs, t3);
	OFFSET(PT_T4, pt_regs, t4);
	OFFSET(PT_T5, pt_regs, t5);
	OFFSET(PT_T6, pt_regs, t6);
	OFFSET(PT_GP, pt_regs, gp);
	OFFSET(PT_STATUS, pt_regs, status);
	OFFSET(PT_BADADDR, pt_regs, badaddr);
	OFFSET(PT_CAUSE, pt_regs, cause);

	/*
	 * We allocate a pt_regs on the stack. This ensures the alignment is sane.
	 */
	DEFINE(PT_SIZE_ON_STACK, ALIGN(sizeof(struct pt_regs), STACK_ALIGN));
	BLANK();
	COMMENT("struct pbl_board_info offsets");
	OFFSET(PBI_LOCATE_PC,		pbl_board_info, assembly_locate_pc);
	OFFSET(PBI_ROM_STAGE,		pbl_board_info, rom_stage);
	OFFSET(PBI_FIRST_STAGE,		pbl_board_info, first_stage);
	OFFSET(PBI_SECOND_STAGE,	pbl_board_info, second_stage);
	DEFINE(PBI_SIZEOF,		sizeof(struct pbl_board_info));
	BLANK();
	COMMENT("struct pbl_stage offsets");
	OFFSET(PBS_STACKTOP,		pbl_stage, stacktop);
	OFFSET(PBS_MEM_START,		pbl_stage, mem_start);
	OFFSET(PBS_MEM_MINSZ,		pbl_stage, mem_minsz);
	OFFSET(PBS_MEM_MAXSZ,		pbl_stage, mem_maxsz);
	OFFSET(PBS_CPU_INIT,		pbl_stage, prereloc_cpu_init);
	OFFSET(PBS_SOC_INIT,		pbl_stage, soc_init);
	OFFSET(PBS_BOARD_INIT,		pbl_stage, board_init);
	OFFSET(PBS_SOC_CHAINLOAD,	pbl_stage, soc_chainload);
	BLANK();
	DEFINE(PBL_STAGE_ROM_ASM,	PBL_STAGE_ROM);
	DEFINE(PBL_STAGE_OCRAM_ASM,	PBL_STAGE_OCRAM);
	DEFINE(PBL_STAGE_DRAM_ASM,	PBL_STAGE_DRAM);
	DEFINE(PBS_STACKTOP_BEFORE_BAREBOX,	PBL_STACKTOP_BEFORE_BAREBOX);
	DEFINE(PBS_STACKTOP_AFTER_BAREBOX,	PBL_STACKTOP_AFTER_BAREBOX);
	DEFINE(RISCV_S_MODE_ASM,	RISCV_S_MODE);
	DEFINE(RISCV_M_MODE_ASM,	RISCV_M_MODE);
	return 0;
}
