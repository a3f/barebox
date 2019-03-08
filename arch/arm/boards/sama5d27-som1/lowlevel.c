// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Ahmad Fatoum, Pengutronix
 */

#include <common.h>
#include <init.h>

#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>

#include <debug_ll.h>
#include <linux/kconfig.h>
#include <mach/at91_dbgu.h>
#include <mach/at91_ddrsdrc.h>
#include <mach/at91_lowlevel_clock.h>
#include <mach/at91_pio.h>
#include <mach/at91_pmc.h>
#include <mach/at91_wdt.h>
#include <mach/ddramc.h>
#include <mach/early_udelay.h>
#include <mach/gpio.h>
#include <mach/iomux.h>

#define RGB_LED_GREEN (1 << 0)
#define RGB_LED_RED   (1 << 1)
#define RGB_LED_BLUE  (1 << 2)

#define SOM1_EK_LED_RED	AT91_PIN_PA10
#define SOM1_EK_LED_GREEN AT91_PIN_PB1
#define SOM1_EK_LED_BLUE AT91_PIN_PA31

/* PCK = 492MHz, MCK = 164MHz */
#define MASTER_CLOCK	164000000

#define sama5d2_pmc_enable_periph_clock(clk) \
	at91_pmc_sam9x5_enable_periph_clock(IOMEM(SAMA5D2_BASE_PMC), clk)

#define at91_pmc_write(off, val) writel(val, IOMEM(SAMA5D2_BASE_PMC) + off)
#define at91_pmc_read(off) readl(IOMEM(SAMA5D2_BASE_PMC) + off)

#define BAUDRATE(mck, baud) \
	(((((mck) * 10) / ((baud) * 16)) % 10) >= 5) ? \
	(mck / (baud * 16) + 1) : ((mck) / (baud * 16))


#define	PIO4_MSKR	0x0000	/* PIO Mask Register */
#define	PIO4_CFGR	0x0004	/* PIO Configuration Register */
#define	PIO4_SODR	0x0010	/* PIO Set Output Data Register */
#define	PIO4_CODR	0x0014	/* PIO Clear Output Data Register */
#define	PIO4_IDR		0x0024	/* PIO Interrupt Disable Register */

#define	AT91C_PIO4_CFGR_FUNC_GPIO	0x00
#define	AT91C_PIO4_CFGR_DIR	(0x01 << 8)	/* Direction */

static void pio_set_gpio_output(void __iomem *pio, unsigned pin, int value)
{

	unsigned mask = pin_to_mask(pin);
	writel(mask, pio + PIO4_MSKR);
	writel(AT91C_PIO4_CFGR_FUNC_GPIO | AT91C_PIO4_CFGR_DIR,
		     pio + PIO4_CFGR);
	writel(mask, pio + (value ? PIO4_SODR : PIO4_CODR));
}
static void turn_led(unsigned color)
{
	pio_set_gpio_output(IOMEM(SAMA5D2_BASE_PIOA), 10, color & RGB_LED_RED);
	pio_set_gpio_output(IOMEM(SAMA5D2_BASE_PIOB), 1, color & RGB_LED_GREEN);
	pio_set_gpio_output(IOMEM(SAMA5D2_BASE_PIOA), 31, color & RGB_LED_BLUE);
}

#define		AT91C_PIO_CFGR_FUNC_PERIPH_A	0x01
static void pio4_set_periph(void __iomem *pio, unsigned mask, unsigned func)
{
	unsigned int value = func;
	writel(mask, pio + PIO4_MSKR);
	writel(value, pio + PIO4_CFGR);
}

static void configure_piod_pin(unsigned pin)
{
	void __iomem *pio = IOMEM(SAMA5D2_BASE_PIOD);
	unsigned mask = pin_to_mask(pin);

	pio4_set_periph(pio, mask, AT91C_PIO_CFGR_FUNC_PERIPH_A);
}

static void dbgu_init(void)
{
	unsigned mck = MASTER_CLOCK;

	configure_piod_pin(AT91_PIN_PD2); /* DBGU RXD */
	configure_piod_pin(AT91_PIN_PD3); /* DBGU TXD */

	sama5d2_pmc_enable_periph_clock(SAMA5D2_ID_UART1);

	if (at91_pmc_check_mck_h32mxdiv(IOMEM(SAMA5D2_BASE_PMC))) {
		mck /= 2;
	} else {
	}

	at91_dbgu_setup_ll(SAMA5D2_BASE_UART1, BAUDRATE(mck, 115200));

	putc_ll('>');
}

static void ddramc_reg_config(struct at91_ddramc_register *ddramc_config)
{
	ddramc_config->mdr = AT91C_DDRC2_DBW_16_BITS |
			     AT91C_DDRC2_MD_DDR2_SDRAM;

	ddramc_config->cr = AT91C_DDRC2_NC_DDR10_SDR9 |
			    AT91C_DDRC2_NR_13 |
			    AT91C_DDRC2_CAS_3 |
			    AT91C_DDRC2_DISABLE_RESET_DLL |
			    AT91C_DDRC2_WEAK_STRENGTH_RZQ7 |
			    AT91C_DDRC2_ENABLE_DLL |
			    AT91C_DDRC2_NB_BANKS_8 |
			    AT91C_DDRC2_NDQS_ENABLED |
			    AT91C_DDRC2_DECOD_INTERLEAVED |
			    AT91C_DDRC2_UNAL_SUPPORTED;

	ddramc_config->rtr = 0x511;

	ddramc_config->t0pr = AT91C_DDRC2_TRAS_(7) |
			      AT91C_DDRC2_TRCD_(3) |
			      AT91C_DDRC2_TWR_(3) |
			      AT91C_DDRC2_TRC_(9) |
			      AT91C_DDRC2_TRP_(3) |
			      AT91C_DDRC2_TRRD_(2) |
			      AT91C_DDRC2_TWTR_(2) |
			      AT91C_DDRC2_TMRD_(2);

	ddramc_config->t1pr = AT91C_DDRC2_TRFC_(22) |
			      AT91C_DDRC2_TXSNR_(23) |
			      AT91C_DDRC2_TXSRD_(200) |
			      AT91C_DDRC2_TXP_(2);

	ddramc_config->t2pr = AT91C_DDRC2_TXARD_(2) |
			      AT91C_DDRC2_TXARDS_(8) |
			      AT91C_DDRC2_TRPA_(4) |
			      AT91C_DDRC2_TRTP_(2) |
			      AT91C_DDRC2_TFAW_(8);
}

static void sama5d2_ddr2_init(void)
{
	struct at91_ddramc_register ddramc_reg;
	unsigned int reg;

	ddramc_reg_config(&ddramc_reg);

	/* enable ddr2 clock */
	sama5d2_pmc_enable_periph_clock(SAMA5D2_ID_MPDDRC);
	at91_pmc_write(AT91_PMC_SCER, AT91CAP9_PMC_DDR);

	reg = AT91C_MPDDRC_RD_DATA_PATH_ONE_CYCLES;
	writel(reg, SAMA5D2_BASE_MPDDRC + AT91C_MPDDRC_RD_DATA_PATH);

	reg = readl(SAMA5D2_BASE_MPDDRC + AT91C_MPDDRC_IO_CALIBR);
	reg &= ~AT91C_MPDDRC_RDIV;
	reg &= ~AT91C_MPDDRC_TZQIO;
	reg |= AT91C_MPDDRC_RDIV_DDR2_RZQ_50;
	reg |= AT91C_MPDDRC_TZQIO_(101);
	writel(reg, SAMA5D2_BASE_MPDDRC + AT91C_MPDDRC_IO_CALIBR);

	/* DDRAM2 Controller initialize */
	at91_ddram_initialize(SAMA5D2_BASE_MPDDRC, SAMA5_DDRCS, &ddramc_reg);
}

extern char __dtb_z_at91_sama5d27_som1_ek_start[];

static void noinline lowlevel_board_init(void)
{
	if (get_pc() < SAMA5_DDRCS) {
		at91_wdt_disable(IOMEM(SAMA5D2_BASE_WDT));
		turn_led(RGB_LED_GREEN);
		at91_lowlevel_clock_init(IOMEM(SAMA5D2_BASE_PMC));

		turn_led(RGB_LED_RED | RGB_LED_GREEN); /* Yellow */

		/* Configure PLLA = MOSC * (PLL_MULA + 1) / PLL_DIVA */
		at91_pmc_write(AT91_CKGR_PLLAR, AT91_PMC_PLLA_WR_ERRATA);
		at91_pmc_write(AT91_CKGR_PLLAR, AT91_PMC_PLLA_WR_ERRATA
			       | AT91_PMC3_MUL_(40) | AT91_PMC_OUT_0
			       | AT91_PMC_PLLCOUNT
			       | AT91_PMC_DIV_BYPASS);

		while (!(at91_pmc_read(AT91_PMC_SR) & AT91_PMC_LOCKA))
			;

		/* Initialize PLLA charge pump */
		/* No need: we keep what is set in ROM code */
		//at91_pmc_write(AT91_PMC_PLLICPR, AT91_PMC_IPLLA_3);

		/* Switch PCK/MCK on PLLA output */
		at91_pmc_cfg_mck(IOMEM(SAMA5D2_BASE_PMC),
				AT91_PMC_H32MXDIV
				| AT91_PMC_PLLADIV2_ON
				| AT91SAM9_PMC_MDIV_3
				| AT91_PMC_CSS_PLLA);

		early_udelay_init(IOMEM(SAMA5D2_BASE_PMC), IOMEM(SAMA5D2_BASE_PITC),
				  SAMA5D2_ID_PIT, MASTER_CLOCK);
	}

	if (IS_ENABLED(CONFIG_DEBUG_LL)) {
		dbgu_init();
	}

	if (get_pc() < SAMA5_DDRCS) {
		sama5d2_ddr2_init();
	}

	turn_led(RGB_LED_GREEN);
	barebox_arm_entry(SAMA5_DDRCS, SZ_128M, NULL);
}

ENTRY_FUNCTION(start_sama5d27_som1_ek_boot_bin, r0, r1, r2)
{
	arm_cpu_lowlevel_init();

	arm_setup_stack(SAMA5D2_SRAM_BASE + SAMA5D2_SRAM_SIZE - 16);

	relocate_to_current_adr();
	setup_c();
	barrier();

	lowlevel_board_init();
}
static void noinline board_init(void)
{
	void *fdt;

	if (IS_ENABLED(CONFIG_DEBUG_LL))
		dbgu_init();

	if (IS_ENABLED(CONFIG_MACH_SAMA5D27_SOM1_DT))
		fdt = __dtb_z_at91_sama5d27_som1_ek_start + get_runtime_offset();
	else
		fdt = NULL;

	turn_led(RGB_LED_GREEN);
	barebox_arm_entry(SAMA5_DDRCS, SZ_128M, fdt);
}

ENTRY_FUNCTION(start_sama5d27_som1_ek, r0, r1, r2)
{
	arm_setup_stack(SAMA5D2_SRAM_BASE + SAMA5D2_SRAM_SIZE - 16);

	if (IS_ENABLED(CONFIG_DEBUG_LL))
		dbgu_init();

	board_init();
}
