// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <pbl/entry.h>
#include <asm/barebox-riscv.h>
#include <asm/riscv_nmon.h>

#define LITEX_RAM_BASE	0x40000000

extern char __dtb_z_litex_linux_start[];

static void __prereloc litex_cpu_init(void)
{
	barebox_nmon_entry();
}

/* BIOS will have loaded us into RAM */
ENTRY_POINT_HEAD(start_litex_linux, __barebox_pbl_head_v1_machine)
	.soc_type = SOC_TYPE_LITEX,
	.second_stage.stacktop = LITEX_RAM_BASE + SZ_256M,
	.second_stage.mem_start = LITEX_RAM_BASE,
	.second_stage.mem_minsz = SZ_256M,
	.second_stage.board_data = __dtb_z_litex_linux_start,
	.second_stage.prereloc_cpu_init = litex_cpu_init,
	.second_stage.soc_chainload = pbl_stage_barebox_entry,
ENTRY_POINT_END
