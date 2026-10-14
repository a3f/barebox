// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <debug_ll.h>
#include <pbl/entry.h>
#include <asm/barebox-riscv.h>
#include <asm/riscv_nmon.h>

#define BEAGLEV_MEM_BASE	0x84000000

extern char __dtb_z_jh7100_beaglev_starlight_start[];

static void __prereloc beaglev_cpu_init(void)
{
	debug_ll_init();
	barebox_nmon_entry();
}

/*
 * OpenSBI starts its payload below the memory handed to barebox, so the
 * PBL relocates there first.
 */
ENTRY_POINT_HEAD(start_beaglev_starlight, __barebox_pbl_head_v1_supervisor)
	.soc_type = SOC_TYPE_JH7100,
	.second_stage.stacktop = BEAGLEV_MEM_BASE + SZ_512M,
	.second_stage.relocaddr = BEAGLEV_MEM_BASE,
	.second_stage.mem_start = BEAGLEV_MEM_BASE,
	.second_stage.mem_minsz = SZ_512M,
	.second_stage.board_data = __dtb_z_jh7100_beaglev_starlight_start,
	.second_stage.prereloc_cpu_init = beaglev_cpu_init,
	.second_stage.soc_chainload = pbl_stage_barebox_entry,
ENTRY_POINT_END
