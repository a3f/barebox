// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Ahmad Fatoum <a.fatoum@pengutronix.de>
 */

#include <debug_ll.h>
#include <common.h>
#include <linux/sizes.h>
#include <io.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <asm/sections.h>
#include <mach/gba.h>

extern char __dtb_nintendo_gameboy_advance_start[];

static inline void setup_uart(void __iomem *base)
{
	putc_ll('>');
}

ENTRY_FUNCTION(start_nintendo_gba, r0, r1, r2)
{
	void *fdt;

	arm_cpu_lowlevel_init();

	if (IS_ENABLED(CONFIG_DEBUG_LL))
		setup_uart(IOMEM(GBA_SERIAL_BASE));

	fdt = __dtb_nintendo_gameboy_advance_start + get_runtime_offset();

	barebox_arm_entry(GBA_EWRAM_BASE, GBA_EWRAM_SIZE, fdt);
}
