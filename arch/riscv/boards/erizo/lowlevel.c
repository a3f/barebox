// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <pbl/entry.h>
#include <asm/barebox-riscv.h>
#include <debug_ll.h>
#include <asm/riscv_nmon.h>

#define ERIZO_SDRAM_BASE	0x80000000

extern char __dtb_z_erizo_generic_start[];

static void __prereloc erizo_cpu_init(void)
{
	debug_ll_init();
	barebox_nmon_entry();
}

/*
 * On POR, we are running from read-only memory, so the PBL relocates to
 * the start of SDRAM. It stays in place if it was loaded there.
 */
ENTRY_POINT_HEAD(start_erizo_generic, __barebox_pbl_head_v1_machine)
	.soc_type = SOC_TYPE_ERIZO,
	.second_stage.stacktop = ERIZO_SDRAM_BASE + SZ_8M,
	.second_stage.relocaddr = ERIZO_SDRAM_BASE,
	.second_stage.mem_start = ERIZO_SDRAM_BASE,
	.second_stage.mem_minsz = SZ_8M,
	.second_stage.board_data = __dtb_z_erizo_generic_start,
	.second_stage.prereloc_cpu_init = erizo_cpu_init,
	.second_stage.soc_chainload = pbl_stage_barebox_entry,
ENTRY_POINT_END
