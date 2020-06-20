// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Ahmad Fatoum, Pengutronix
 */

#include <common.h>
#include <init.h>

#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <mach/sama5d2_ll.h>
#include <mach/iomux.h>
#include <debug_ll.h>
#include <mach/at91_dbgu.h>

/* PCK = 492MHz, MCK = 164MHz */
#define MASTER_CLOCK	164000000

static void dbgu_init(void)
{
	sama5d2_dbgu_setup_ll(SAMA5D2_ID_UART1, AT91_PIN_PD3, AT91_MUX_PERIPH_A,
			      MASTER_CLOCK, 115200);

	putc_ll('>');
}

extern char __dtb_z_at91_sama5d27_giantboard_start[];

ENTRY_FUNCTION(start_sama5d27_giantboard, r0, r1, r2)
{
	void *fdt;

	arm_cpu_lowlevel_init();

	if (IS_ENABLED(CONFIG_DEBUG_LL))
		dbgu_init();

	fdt = __dtb_z_at91_sama5d27_giantboard_start + get_runtime_offset();

	sama5d2_barebox_entry(fdt);
}
