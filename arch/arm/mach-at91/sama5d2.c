/*
 * Chip-specific setup code for the SAMA5D2 family
 *
 * Copyright (C) 2014 Atmel Corporation,
 *		      Bo Shen <voice.shen@atmel.com>
 *
 * Licensed under GPLv2 or later.
 */

#include <common.h>
#include <gpio.h>
#include <init.h>
#include <restart.h>
#include <mach/hardware.h>
#include <mach/at91_pmc.h>
#include <mach/cpu.h>
#include <mach/board.h>
#include <mach/at91_rstc.h>
#include <linux/clk.h>

#include "generic.h"
#include "clock.h"

/*
 * The peripheral clocks.
 */
static struct clk pit_clk = {
	.name		= "pit_clk",
	.pid		= SAMA5D2_ID_PIT,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk macb0_clk = {
	.name = "macb0_clk ",
	.pid = 5,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk tdes_clk = {
	.name = "tdes_clk",
	.pid = 11,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk matrix1_clk = {
	.name = "matrix1_clk",
	.pid = 14,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk smc_clk = {
	.name = "smc_clk",
	.pid = SAMA5D2_ID_HSMC,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk pioA_clk = {
	.name = "pioA_clk",
	.pid = 18,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk flx0_clk = {
	.name = "flx0_clk",
	.pid = 19,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk flx1_clk = {
	.name = "flx1_clk",
	.pid = 20,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk flx2_clk = {
	.name = "flx2_clk",
	.pid = 21,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk flx3_clk = {
	.name = "flx3_clk",
	.pid = 22,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk flx4_clk = {
	.name = "flx4_clk",
	.pid = 23,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk uart0_clk = {
	.name = "uart0_clk",
	.pid = 24,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk uart1_clk = {
	.name = "uart1_clk",
	.pid = 25,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk uart2_clk = {
	.name = "uart2_clk",
	.pid = 26,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk uart3_clk = {
	.name = "uart3_clk",
	.pid = 27,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk uart4_clk = {
	.name = "uart4_clk",
	.pid = 28,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk twi0_clk = {
	.name = "twi0_clk",
	.pid = 29,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk twi1_clk = {
	.name = "twi1_clk",
	.pid = 30,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk spi0_clk = {
	.name = "spi0_clk",
	.pid = 33,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk spi1_clk = {
	.name = "spi1_clk",
	.pid = 34,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk tcb0_clk = {
	.name = "tcb0_clk",
	.pid = 35,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk tcb1_clk = {
	.name = "tcb1_clk",
	.pid = 36,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk pwm_clk = {
	.name = "pwm_clk",
	.pid = 38,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk adc_clk = {
	.name = "adc_clk",
	.pid = 40,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk uhphs_clk = {
	.name = "uhphs_clk",
	.pid = 41,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk udphs_clk = {
	.name = "udphs_clk",
	.pid = 42,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk ssc0_clk = {
	.name = "ssc0_clk",
	.pid = 43,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk ssc1_clk = {
	.name = "ssc1_clk",
	.pid = 44,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk trng_clk = {
	.name = "trng_clk",
	.pid = 47,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk pdmic_clk = {
	.name = "pdmic_clk",
	.pid = 48,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk securam_clk = {
	.name = "securam_clk",
	.pid = 51,
	.type = CLK_TYPE_PERIPHERAL,

};
static struct clk i2s0_clk = {
	.name = "i2s0_clk",
	.pid = 54,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk i2s1_clk = {
	.name = "i2s1_clk",
	.pid = 55,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk can0_clk = {
	.name = "can0_clk",
	.pid = 56,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk can1_clk = {
	.name = "can1_clk",
	.pid = 57,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk classd_clk = {
	.name = "classd_clk",
	.pid = 59,
	.type = CLK_TYPE_PERIPHERAL,
};
static struct clk dma0_clk = {
	.name = "dma0_clk",
	.pid = 6,
	.type           = CLK_TYPE_PERIPHERAL | CLK_TYPE_PERIPH_H64MX,
};
static struct clk dma1_clk = {
	.name = "dma1_clk",
	.pid = 7,
	.type           = CLK_TYPE_PERIPHERAL | CLK_TYPE_PERIPH_H64MX,
};
static struct clk aes_clk = {
	.name = "aes_clk",
	.pid = 9,
	.type           = CLK_TYPE_PERIPHERAL | CLK_TYPE_PERIPH_H64MX,
};
static struct clk aesb_clk = {
	.name = "aesb_clk",
	.pid = 10,
	.type           = CLK_TYPE_PERIPHERAL | CLK_TYPE_PERIPH_H64MX,
};
static struct clk sha_clk = {
	.name = "sha_clk",
	.pid = 12,
	.type           = CLK_TYPE_PERIPHERAL | CLK_TYPE_PERIPH_H64MX,
};
static struct clk mpddr_clk = {
	.name = "mpddr_clk",
	.pid = 13,
	.type           = CLK_TYPE_PERIPHERAL | CLK_TYPE_PERIPH_H64MX,
};
static struct clk matrix0_clk = {
	.name = "matrix0_clk",
	.pid = 15,
	.type           = CLK_TYPE_PERIPHERAL | CLK_TYPE_PERIPH_H64MX,
};
static struct clk sdmmc0_hclk = {
	.name = "sdmmc0_hclk",
	.pid = 31,
	.type           = CLK_TYPE_PERIPHERAL | CLK_TYPE_PERIPH_H64MX,
};
static struct clk sdmmc1_hclk = {
	.name = "sdmmc1_hclk",
	.pid = 32,
	.type           = CLK_TYPE_PERIPHERAL | CLK_TYPE_PERIPH_H64MX,
};
static struct clk lcdc_clk = {
	.name = "lcdc_clk",
	.pid = 45,
	.type           = CLK_TYPE_PERIPHERAL | CLK_TYPE_PERIPH_H64MX,
};
static struct clk isc_clk = {
	.name = "isc_clk",
	.pid = 46,
	.type           = CLK_TYPE_PERIPHERAL | CLK_TYPE_PERIPH_H64MX,
};
static struct clk qspi0_clk = {
	.name = "qspi0_clk",
	.pid = 52,
	.type           = CLK_TYPE_PERIPHERAL | CLK_TYPE_PERIPH_H64MX,
};
static struct clk qspi1_clk = {
	.name = "qspi1_clk",
	.pid = 53,
	.type           = CLK_TYPE_PERIPHERAL | CLK_TYPE_PERIPH_H64MX,
};
static struct clk *periph_clocks[] __initdata = {
	&pit_clk,
	&macb0_clk,
	&tdes_clk,
	&matrix1_clk,
	&smc_clk,
	&pioA_clk,
	&flx0_clk,
	&flx1_clk,
	&flx2_clk,
	&flx3_clk,
	&flx4_clk,
	&uart0_clk,
	&uart1_clk,
	&uart2_clk,
	&uart3_clk,
	&uart4_clk,
	&twi0_clk,
	&twi1_clk,
	&spi0_clk,
	&spi1_clk,
	&tcb0_clk,
	&tcb1_clk,
	&pwm_clk,
	&adc_clk,
	&uhphs_clk,
	&udphs_clk,
	&ssc0_clk,
	&ssc1_clk,
	&trng_clk,
	&pdmic_clk,
	&securam_clk,
	&i2s0_clk,
	&i2s1_clk,
	&can0_clk,
	&can1_clk,
	&classd_clk,
	&dma0_clk,
	&dma1_clk,
	&aes_clk,
	&aesb_clk,
	&sha_clk,
	&mpddr_clk,
	&matrix0_clk,
	&sdmmc0_hclk,
	&sdmmc1_hclk,
	&lcdc_clk,
	&isc_clk,
	&qspi0_clk,
	&qspi1_clk,
};

static struct clk pck0 = {
	.name		= "pck0",
	.pmc_mask	= AT91_PMC_PCK0,
	.type		= CLK_TYPE_PROGRAMMABLE,
	.id		= 0,
};

static struct clk pck1 = {
	.name		= "pck1",
	.pmc_mask	= AT91_PMC_PCK1,
	.type		= CLK_TYPE_PROGRAMMABLE,
	.id		= 1,
};

static struct clk pck2 = {
	.name		= "pck2",
	.pmc_mask	= AT91_PMC_PCK2,
	.type		= CLK_TYPE_PROGRAMMABLE,
	.id		= 2,
};

static struct clk_lookup periph_clocks_lookups[] = {
	CLKDEV_CON_DEV_ID("macb_clk", "macb0", &macb0_clk),
	CLKDEV_CON_DEV_ID("spi_clk", "atmel_spi0", &spi0_clk),
	CLKDEV_DEV_ID("at91-pit", &pit_clk),
	CLKDEV_CON_DEV_ID("hck1", "atmel_hlcdfb", &lcdc_clk),
};

static struct clk_lookup uart_clocks_lookups[] = {
	CLKDEV_CON_DEV_ID("usart", "atmel_usart0", &uart0_clk),
	CLKDEV_CON_DEV_ID("usart", "atmel_usart1", &uart1_clk),
	CLKDEV_CON_DEV_ID("usart", "atmel_usart2", &uart2_clk),
	CLKDEV_CON_DEV_ID("usart", "atmel_usart3", &uart3_clk),
	CLKDEV_CON_DEV_ID("usart", "atmel_usart4", &uart3_clk),
};

static void __init sama5d2_register_clocks(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(periph_clocks); i++)
		clk_register(periph_clocks[i]);

	clkdev_add_physbase(&pioA_clk, SAMA5D2_BASE_PIOA, 0);

	clkdev_add_table(periph_clocks_lookups,
			 ARRAY_SIZE(periph_clocks_lookups));

	clkdev_add_table(uart_clocks_lookups,
			 ARRAY_SIZE(uart_clocks_lookups));

	clk_register(&pck0);
	clk_register(&pck1);
	clk_register(&pck2);
}

static void sama5d2_restart(struct restart_handler *rst)
{
	at91sam9g45_reset(IOMEM(SAMA5D2_BASE_MPDDRC),
			  IOMEM(SAMA5D2_BASE_RSTC + AT91_RSTC_CR));
}

/* --------------------------------------------------------------------
 *  Processor initialization
 * -------------------------------------------------------------------- */
static void sama5d2_initialize(void)
{
	/* Register the processor-specific clocks */
	sama5d2_register_clocks();
	at91_add_pit(SAMA5D2_BASE_PITC);
	at91_add_sam9_smc(DEVICE_ID_SINGLE, SAMA5D2_BASE_HSMC + 0x600, 0xa0);
	restart_handler_register_fn(sama5d2_restart);
}

static int sama5d2_setup(void)
{
	at91_boot_soc = sama5d2_initialize;
	return 0;
}
pure_initcall(sama5d2_setup);
