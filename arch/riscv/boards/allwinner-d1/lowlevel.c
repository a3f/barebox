// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <pbl/entry.h>
#include <asm/barebox-riscv.h>

#define DRAM_BASE	0x40000000UL

/* OpenSBI passes on the device tree the SPL loaded */
static void allwinner_d1_bdinit(unsigned long entry_data)
{
	pbl_get_stage()->board_data = (void *)entry_data;
}

ENTRY_POINT_HEAD(start_allwinner_d1, __barebox_pbl_head_v1_supervisor)
	.soc_type = SOC_TYPE_SUN20I_D1,
	.second_stage.stacktop = DRAM_BASE + SZ_1G,
	.second_stage.mem_start = DRAM_BASE,
	.second_stage.mem_minsz = SZ_1G,
	.second_stage.board_init = allwinner_d1_bdinit,
	.second_stage.soc_chainload = pbl_stage_barebox_entry,
ENTRY_POINT_END
