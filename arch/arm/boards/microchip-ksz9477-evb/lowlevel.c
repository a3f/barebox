/*
 * Copyright (C) 2018 Ahmad Fatoum, Pengutronix
 *
 * Under GPLv2
 */

#include <common.h>
#include <init.h>

#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>

#include <mach/ddramc.h>
#include <mach/at91_ddrsdrc.h>
#include <mach/hardware.h>
#include <mach/at91_dbgu.h>
#include <mach/at91_pmc.h>
#include <mach/at91_pio.h>
#include <mach/gpio.h>
#include <mach/iomux.h>
#include <mach/early_udelay.h>
#include <debug_ll.h>

extern char __dtb_at91_microchip_ksz9477_evb_start[];

static void ddramc_init(void);

#define BAUDRATE(mck, baud) \
	(((((mck) * 10) / ((baud) * 16)) % 10) >= 5) ? \
	(mck / (baud * 16) + 1) : ((mck) / (baud * 16))

#define MASTER_CLOCK		132000000


static void configure_pin(unsigned pin)
{
	void __iomem *pio = IOMEM(SAMA5D3_BASE_PIOB);
	unsigned mask = pin_to_mask(pin);

	at91_mux_disable_interrupt(pio, mask);
	at91_mux_set_pullup(pio, mask, 0);
	at91_mux_pio3_set_pulldown(pio, mask, 0);

	at91_mux_pio3_set_A_periph(pio, mask);

	at91_mux_gpio_disable(pio, mask);
}
static void dbgu_init(void)
{
	pmc_enable_periph_clock(SAMA5D3_ID_PIOB);

	configure_pin(AT91_PIN_PB30);
	configure_pin(AT91_PIN_PB31);

	/* Enable clock */
	pmc_enable_periph_clock(SAMA5D3_ID_DBGU);
	at91_dbgu_setup_ll(AT91_BASE_DBGU1, BAUDRATE(MASTER_CLOCK, 115200));
}
void _hw_init(void);
void noinline board_init(void) {
	void *fdt;

	if (get_pc() < SAMA5_DDRCS) {
		_hw_init();
		early_udelay_init(MASTER_CLOCK);
		dbgu_init();
		puts_ll("Initializing dbgu... done\n");
		puts_ll("Initializing SDRAM... ");
		ddramc_init();
		puts_ll("done\n");
	}

	if (IS_ENABLED(CONFIG_MACH_MICROCHIP_KSZ9477_EVB_DT))
		fdt = __dtb_at91_microchip_ksz9477_evb_start + get_runtime_offset();
	else
		fdt = NULL;

	barebox_arm_entry(SAMA5_DDRCS, SZ_256M, fdt);
}

ENTRY_FUNCTION(start_sama5d3_xplained_ung8071, r0, r1, r2)
{
	void *fdt;

	arm_cpu_lowlevel_init();

	arm_setup_stack(SAMA5D3_SRAM_BASE + SAMA5D3_SRAM_SIZE - 16);

	fdt = __dtb_at91_microchip_ksz9477_evb_start + get_runtime_offset();

	arm_setup_stack(SAMA5D3_SRAM_BASE + SAMA5D3_SRAM_SIZE - 16);
	relocate_to_current_adr();
	setup_c();
	barrier();

	board_init();

	barebox_arm_entry(SAMA5_DDRCS, SZ_256M, fdt);
}

/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2014, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

static void ddramc_reg_config(struct ddramc_register *ddramc_config)
{
	ddramc_config->mdr = (AT91C_DDRC2_DBW_32_BITS
				| AT91C_DDRC2_MD_DDR2_SDRAM);

	ddramc_config->cr = (AT91C_DDRC2_NC_DDR10_SDR9
				| AT91C_DDRC2_NR_13
				| AT91C_DDRC2_CAS_3
				| AT91C_DDRC2_DISABLE_RESET_DLL
				| AT91C_DDRC2_ENABLE_DLL
				| AT91C_DDRC2_ENRDM_ENABLE
				| AT91C_DDRC2_NB_BANKS_8
				| AT91C_DDRC2_NDQS_DISABLED
				| AT91C_DDRC2_DECOD_INTERLEAVED
				| AT91C_DDRC2_UNAL_SUPPORTED);

	/*
	 * The DDR2-SDRAM device requires a refresh every 15.625 us or 7.81 us.
	 * With a 133 MHz frequency, the refresh timer count register must to be
	 * set with (15.625 x 133 MHz) ~ 2084 i.e. 0x824
	 * or (7.81 x 133 MHz) ~ 1039 i.e. 0x40F.
	 */
	ddramc_config->rtr = 0x40F;     /* Refresh timer: 7.812us */

	/* One clock cycle @ 133 MHz = 7.5 ns */
	ddramc_config->t0pr = (AT91C_DDRC2_TRAS_(6)	/* 6 * 7.5 = 45 ns */
			| AT91C_DDRC2_TRCD_(2)		/* 2 * 7.5 = 22.5 ns */
			| AT91C_DDRC2_TWR_(2)		/* 2 * 7.5 = 15   ns */
			| AT91C_DDRC2_TRC_(8)		/* 8 * 7.5 = 75   ns */
			| AT91C_DDRC2_TRP_(2)		/* 2 * 7.5 = 15   ns */
			| AT91C_DDRC2_TRRD_(2)		/* 2 * 7.5 = 15   ns */
			| AT91C_DDRC2_TWTR_(2)		/* 2 clock cycles min */
			| AT91C_DDRC2_TMRD_(2));	/* 2 clock cycles */

	ddramc_config->t1pr = (AT91C_DDRC2_TXP_(2)	/* 2 clock cycles */
			| AT91C_DDRC2_TXSRD_(200)	/* 200 clock cycles */
			| AT91C_DDRC2_TXSNR_(19)	/* 19 * 7.5 = 142.5 ns */
			| AT91C_DDRC2_TRFC_(17));	/* 17 * 7.5 = 127.5 ns */

	ddramc_config->t2pr = (AT91C_DDRC2_TFAW_(6)	/* 6 * 7.5 = 45 ns */
			| AT91C_DDRC2_TRTP_(2)		/* 2 clock cycles min */
			| AT91C_DDRC2_TRPA_(2)		/* 2 * 7.5 = 15 ns */
			| AT91C_DDRC2_TXARDS_(8)	/* = TXARD */
			| AT91C_DDRC2_TXARD_(8));	/* MR12 = 1 */
}

static void ddramc_init(void)
{
	struct ddramc_register ddramc_reg;
	unsigned int reg;

	ddramc_reg_config(&ddramc_reg);

	/* enable ddr2 clock */
	pmc_enable_periph_clock(SAMA5D3_ID_MPDDRC);
	at91_pmc_write(AT91_PMC_SCER, AT91CAP9_PMC_DDR);


	/* Init the special register for sama5d3x */
	/* MPDDRC DLL Slave Offset Register: DDR2 configuration */
	reg = AT91C_MPDDRC_S0OFF_1
		| AT91C_MPDDRC_S2OFF_1
		| AT91C_MPDDRC_S3OFF_1;
	writel(reg, (SAMA5D3_BASE_MPDDRC + MPDDRC_DLL_SOR));

	/* MPDDRC DLL Master Offset Register */
	/* write master + clk90 offset */
	reg = AT91C_MPDDRC_MOFF_7
		| AT91C_MPDDRC_CLK90OFF_31
		| AT91C_MPDDRC_SELOFF_ENABLED | AT91C_MPDDRC_KEY;
	writel(reg, (SAMA5D3_BASE_MPDDRC + MPDDRC_DLL_MOR));

	/* MPDDRC I/O Calibration Register */
	/* DDR2 RZQ = 50 Ohm */
	/* TZQIO = 4 */
	reg = AT91C_MPDDRC_RDIV_DDR2_RZQ_50
		| AT91C_MPDDRC_TZQIO_4;
	writel(reg, (SAMA5D3_BASE_MPDDRC + MPDDRC_IO_CALIBR));

	/* DDRAM2 Controller initialize */
	ddram_initialize(SAMA5D3_BASE_MPDDRC, SAMA5_DDRCS, &ddramc_reg);
}
