// SPDX-License-Identifier: GPL-2.0-only AND BSD-1-Clause
/*
 * Copyright (C) 2014, Atmel Corporation
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
#include <mach/at91_ddrsdrc.h>
#include <mach/at91_wdt.h>
#include <mach/gpio.h>

#define RGB_LED_GREEN (1 << 0)
#define RGB_LED_RED   (1 << 1)
#define RGB_LED_BLUE  (1 << 2)

/* PCK = 492MHz, MCK = 164MHz */
#define MASTER_CLOCK	164000000

static void ek_turn_led(unsigned color)
{
	struct {
		unsigned long pio;
		unsigned bit;
		unsigned color;
	} *led, leds[] = {
		{ .pio = SAMA5D2_BASE_PIOA, .bit = 10, .color = color & RGB_LED_RED },
		{ .pio = SAMA5D2_BASE_PIOB, .bit =  1, .color = color & RGB_LED_GREEN },
		{ .pio = SAMA5D2_BASE_PIOA, .bit = 31, .color = color & RGB_LED_BLUE },
		{ /* sentinel */ },
	};

	for (led = leds; led->pio; led++) {
		at91_mux_gpio4_enable(IOMEM(led->pio), BIT(led->bit));
		at91_mux_gpio4_input(IOMEM(led->pio), BIT(led->bit), false);
		at91_mux_gpio4_set(IOMEM(led->pio), BIT(led->bit), led->color);
	}
}

static void ek_dbgu_init(void)
{
	unsigned mck = MASTER_CLOCK / 2;

	sama5d2_pmc_enable_periph_clock(SAMA5D2_ID_PIOD);

	at91_mux_pio4_set_A_periph(IOMEM(SAMA5D2_BASE_PIOD),
				   pin_to_mask(AT91_PIN_PD3)); /* DBGU TXD */

	sama5d2_pmc_enable_periph_clock(SAMA5D2_ID_UART1);

	at91_dbgu_setup_ll(IOMEM(SAMA5D2_BASE_UART1), mck, 115200);

	putc_ll('>');
}

static struct at91_ddramc_register ddramc_reg_config = {
	.mdr = AT91C_DDRC2_DBW_16_BITS |
		AT91C_DDRC2_MD_DDR2_SDRAM,

	.cr = AT91C_DDRC2_NC_DDR10_SDR9 |
		AT91C_DDRC2_NR_13 |
		AT91C_DDRC2_CAS_3 |
		AT91C_DDRC2_DISABLE_RESET_DLL |
		AT91C_DDRC2_WEAK_STRENGTH_RZQ7 |
		AT91C_DDRC2_ENABLE_DLL |
		AT91C_DDRC2_NB_BANKS_8 |
		AT91C_DDRC2_NDQS_ENABLED |
		AT91C_DDRC2_DECOD_INTERLEAVED |
		AT91C_DDRC2_UNAL_SUPPORTED,

	.rtr = 0x511,

	.t0pr = AT91C_DDRC2_TRAS_(7) |
		AT91C_DDRC2_TRCD_(3) |
		AT91C_DDRC2_TWR_(3) |
		AT91C_DDRC2_TRC_(9) |
		AT91C_DDRC2_TRP_(3) |
		AT91C_DDRC2_TRRD_(2) |
		AT91C_DDRC2_TWTR_(2) |
		AT91C_DDRC2_TMRD_(2),

	.t1pr = AT91C_DDRC2_TRFC_(22) |
		AT91C_DDRC2_TXSNR_(23) |
		AT91C_DDRC2_TXSRD_(200) |
		AT91C_DDRC2_TXP_(2),

	.t2pr = AT91C_DDRC2_TXARD_(2) |
		AT91C_DDRC2_TXARDS_(8) |
		AT91C_DDRC2_TRPA_(4) |
		AT91C_DDRC2_TRTP_(2) |
		AT91C_DDRC2_TFAW_(8),
};

static void noinline __noreturn sama5d27_first_stage(void *fdt)
{
	at91_wdt_disable(IOMEM(SAMA5D2_BASE_WDT));
	sama5d2_pmc_init(MASTER_CLOCK);

	ek_turn_led(RGB_LED_RED | RGB_LED_GREEN); /* Yellow */

	if (IS_ENABLED(CONFIG_DEBUG_LL))
		ek_dbgu_init();

	sama5d2_ddr2_init(&ddramc_reg_config);

	ek_turn_led(RGB_LED_GREEN);
	barebox_arm_entry(SAMA5_DDRCS, SZ_128M, fdt);
}

extern char __dtb_z_at91_sama5d27_som1_ek_start[];

ENTRY_FUNCTION(start_sama5d27_som1_ek, r0, r1, r2)
{
	void *fdt = NULL;

	arm_cpu_lowlevel_init();

	arm_setup_stack(SAMA5D2_SRAM_BASE + SAMA5D2_SRAM_SIZE - 16);

	if (IS_ENABLED(CONFIG_MACH_SAMA5D27_SOM1_DT))
		fdt = __dtb_z_at91_sama5d27_som1_ek_start;

	if (get_pc() < SAMA5_DDRCS) {
		relocate_to_current_adr();
		setup_c();
		barrier();

		sama5d27_first_stage(fdt);
	}

	if (fdt)
		fdt += get_runtime_offset();

	if (IS_ENABLED(CONFIG_DEBUG_LL))
		ek_dbgu_init();

	ek_turn_led(RGB_LED_GREEN);
	barebox_arm_entry(SAMA5_DDRCS, SZ_128M, fdt);
}
