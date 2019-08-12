// SPDX-License-Identifier: GPL-2.0-only AND BSD-1-Clause
/*
 * Copyright (C) 2014, Atmel Corporation
 * Copyright (C) 2018 Ahmad Fatoum, Pengutronix
 */

#include <common.h>
#include <init.h>

#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <mach/sama5d3_ll.h>

#include <mach/iomux.h>
#include <debug_ll.h>
#include <mach/at91_dbgu.h>
#include <mach/at91_ddrsdrc.h>
#include <mach/at91_wdt.h>
#include <mach/gpio.h>

/* PCK = 528MHz, MCK = 132MHz */
#define MASTER_CLOCK	132000000

static void dbgu_init(void)
{
	void __iomem *pio = IOMEM(SAMA5D3_BASE_PIOB);

	sama5d3_pmc_enable_periph_clock(SAMA5D3_ID_PIOB);

	at91_mux_pio3_pin(pio, pin_to_mask(AT91_PIN_PB31), AT91_MUX_PERIPH_A, 0);

	sama5d3_pmc_enable_periph_clock(SAMA5D3_ID_DBGU);
	at91_dbgu_setup_ll(IOMEM(AT91_BASE_DBGU1), MASTER_CLOCK, 115200);

	putc_ll('>');
}

static struct at91_ddramc_register ddramc_reg_config = {
	.mdr = AT91C_DDRC2_DBW_32_BITS | AT91C_DDRC2_MD_DDR2_SDRAM,

	.cr = AT91C_DDRC2_NC_DDR10_SDR9
		| AT91C_DDRC2_NR_13
		| AT91C_DDRC2_CAS_3
		| AT91C_DDRC2_DISABLE_RESET_DLL
		| AT91C_DDRC2_ENABLE_DLL
		| AT91C_DDRC2_ENRDM_ENABLE
		| AT91C_DDRC2_NB_BANKS_8
		| AT91C_DDRC2_NDQS_DISABLED
		| AT91C_DDRC2_DECOD_INTERLEAVED
		| AT91C_DDRC2_UNAL_SUPPORTED,

	/*
	 * The DDR2-SDRAM device requires a refresh every 15.625 us or 7.81 us.
	 * With a 133 MHz frequency, the refresh timer count register must to be
	 * set with (15.625 x 133 MHz) ~ 2084 i.e. 0x824
	 * or (7.81 x 133 MHz) ~ 1039 i.e. 0x40F.
	 */
	.rtr = 0x40F,     /* Refresh timer: 7.812us */

	/* One clock cycle @ 133 MHz = 7.5 ns */
	.t0pr = AT91C_DDRC2_TRAS_(6)	/* 6 * 7.5 = 45 ns */
		| AT91C_DDRC2_TRCD_(2)	/* 2 * 7.5 = 22.5 ns */
		| AT91C_DDRC2_TWR_(2)	/* 2 * 7.5 = 15   ns */
		| AT91C_DDRC2_TRC_(8)	/* 8 * 7.5 = 75   ns */
		| AT91C_DDRC2_TRP_(2)	/* 2 * 7.5 = 15   ns */
		| AT91C_DDRC2_TRRD_(2)	/* 2 * 7.5 = 15   ns */
		| AT91C_DDRC2_TWTR_(2)	/* 2 clock cycles min */
		| AT91C_DDRC2_TMRD_(2),	/* 2 clock cycles */

	.t1pr = AT91C_DDRC2_TXP_(2)		/* 2 clock cycles */
		| AT91C_DDRC2_TXSRD_(200)	/* 200 clock cycles */
		| AT91C_DDRC2_TXSNR_(19)	/* 19 * 7.5 = 142.5 ns */
		| AT91C_DDRC2_TRFC_(17),	/* 17 * 7.5 = 127.5 ns */

	.t2pr = AT91C_DDRC2_TFAW_(6)		/* 6 * 7.5 = 45 ns */
		| AT91C_DDRC2_TRTP_(2)		/* 2 clock cycles min */
		| AT91C_DDRC2_TRPA_(2)		/* 2 * 7.5 = 15 ns */
		| AT91C_DDRC2_TXARDS_(8)	/* = TXARD */
		| AT91C_DDRC2_TXARD_(8),	/* MR12 = 1 */
};

extern char __dtb_z_at91_microchip_ksz9477_evb_start[];

static noinline __noreturn void board_setup(void *fdt)
{
	if (IS_ENABLED(CONFIG_MACH_MICROCHIP_KSZ9477_EVB_DT))
		fdt = __dtb_z_at91_microchip_ksz9477_evb_start + get_runtime_offset();

	if (IS_ENABLED(CONFIG_DEBUG_LL))
		dbgu_init();

	at91_wdt_disable(IOMEM(SAMA5D3_BASE_WDT));

	sama5d3_pmc_init(MASTER_CLOCK);
	sama5d3_ddr2_init(&ddramc_reg_config);

	barebox_arm_entry(SAMA5_DDRCS, SZ_256M, fdt);
}

ENTRY_FUNCTION(start_sama5d3_xplained_ung8071, r0, r1, r2)
{
	void *fdt = NULL;

	arm_cpu_lowlevel_init();

	arm_setup_stack(SAMA5D3_SRAM_BASE + SAMA5D3_SRAM_SIZE);

	if (IS_ENABLED(CONFIG_MACH_MICROCHIP_KSZ9477_EVB_DT))
		fdt = __dtb_z_at91_microchip_ksz9477_evb_start;

	if (get_pc() < SAMA5_DDRCS) {
		relocate_to_current_adr();
		setup_c();
		barrier();

		board_setup(fdt);
	}

	if (fdt)
		fdt += get_runtime_offset();

	if (IS_ENABLED(CONFIG_DEBUG_LL))
		dbgu_init();

	barebox_arm_entry(SAMA5_DDRCS, SZ_256M, fdt);
}
