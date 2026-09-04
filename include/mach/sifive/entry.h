/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __MACH_SIFIVE_ENTRY_H
#define __MACH_SIFIVE_ENTRY_H

#include <linux/sizes.h>
#include <pbl/entry.h>
#include <asm/barebox-riscv.h>

#define SIFIVE_DRAM_BASE	0x80000000

/*
 * OpenSBI starts the image like a Linux kernel at its load offset into
 * the DRAM at 0x80000000, so the images use the supervisor mode head.
 * The second stage describes the first 128M of that DRAM, which every
 * board has, and enters barebox proper there, with the stack at the top
 * of the region where barebox_riscv_entry() places its own. A board that
 * wants more of its DRAM than that raises mem_minsz.
 *
 * OpenSBI leaves the console UART set up, so no console is described and
 * the PBL prints through its DEBUG_LL fallback.
 */
#define PBL_SOC_SIFIVE_DEFAULTS(soc)				\
	.soc_type = SOC_TYPE_##soc,				\
	.second_stage = {					\
		.stacktop = SIFIVE_DRAM_BASE + SZ_128M,		\
		.mem_start = SIFIVE_DRAM_BASE,			\
		.mem_minsz = SZ_128M,				\
		.soc_chainload = pbl_stage_barebox_entry,	\
	}

#define PBL_SOC_FU540_HEAD	__barebox_pbl_head_v1_supervisor
#define PBL_SOC_FU540_DEFAULTS	PBL_SOC_SIFIVE_DEFAULTS(FU540)

#define PBL_SOC_FU740_HEAD	__barebox_pbl_head_v1_supervisor
#define PBL_SOC_FU740_DEFAULTS	PBL_SOC_SIFIVE_DEFAULTS(FU740)

#endif
