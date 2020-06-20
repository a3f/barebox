// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Ahmad Fatoum, Pengutronix
 */

#include <common.h>
#include <init.h>
#include <mach/barebox-arm.h>
#include <mach/sama5d2_ll.h>
#include <mach/xload.h>
#include <mach/sama5d2-sip-ddramc.h>
#include <mach/iomux.h>
#include <debug_ll.h>

/* PCK = 492MHz, MCK = 164MHz */
#define MASTER_CLOCK	164000000

static void dbgu_init(void)
{
	sama5d2_dbgu_setup_ll(SAMA5D2_ID_UART1, AT91_PIN_PD3, AT91_MUX_PERIPH_A,
			      MASTER_CLOCK, 115200);
}

SAMA5_ENTRY_FUNCTION(start_sama5d27_giantboard_xload_mmc, r4)
{
	sama5d2_lowlevel_init();

	dbgu_init();
	if (IS_ENABLED(CONFIG_DEBUG_LL))
		putc_ll('>');

	relocate_to_current_adr();
	setup_c();

	pbl_set_putc(at91_dbgu_putc, SAMA5D2_BASE_UART1);

	sama5d2_udelay_init(MASTER_CLOCK);
	sama5d2_d1g_ddrconf();
	sama5d2_sdhci_start_image(r4);
}

extern char __dtb_z_at91_sama5d27_giantboard_start[];

SAMA5_ENTRY_FUNCTION(start_sama5d27_giantboard, r4)
{
	void *fdt;

	if (IS_ENABLED(CONFIG_DEBUG_LL))
		putc_ll('>');

	fdt = __dtb_z_at91_sama5d27_giantboard_start + get_runtime_offset();

	sama5d2_barebox_entry(r4, fdt);
}
