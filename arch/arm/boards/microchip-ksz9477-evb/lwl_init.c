#include <mach/early_udelay.h>
#include <debug_ll.h>
#include <linux/kconfig.h>
#include <mach/at91_pmc.h>
#include <mach/at91_dbgu.h>
#include <mach/at91_wdt.h>
#include <mach/at91_pmc.h>

#define CONFIG_BUS_SPEED_133MHZ
#define CONFIG_CPU_CLK_528MHZ
/*
 * PMC Setting
 *
 * The main oscillator is enabled as soon as possible in the lowlevel_clock_init
 * and MCK is switched on the main oscillator.
 */

/* PCK = 528MHz, MCK = 132MHz */
#define PLLA_MULA		43

#define BOARD_CKGR_PLLA		(AT91C_CKGR_SRCA | AT91_PMC_OUT_0)
#define BOARD_PLLACOUNT		(0x3F << 8)
#define BOARD_MULA		((AT91C_CKGR_MULA << 2) & (PLLA_MULA << 18))
#define BOARD_DIVA		(AT91C_CKGR_DIVA & 1)

#define 	AT91_PMC_MDIV_4		(0x2UL <<  8)
#define BOARD_PRESCALER_MAIN_CLOCK	(AT91_PMC_MDIV_4 \
					 | AT91_PMC_CSS_MAIN)

#define BOARD_PRESCALER_PLLA		(AT91_PMC_MDIV_4 \
					 | AT91_PMC_CSS_PLLA)

/* END CLOCK SPECIFIC */


int _pmc_cfg_mck(unsigned int pmc_mckr)
{
	unsigned int tmp;

	/*
	 * Program the PRES field in the AT91_PMC_MCKR register
	 */
#define	AT91C_PMC_PRES		(0xFUL << 2)
#define	AT91C_PMC_ALT_PRES	(0xFUL << 4)
	tmp = at91_pmc_read(AT91_PMC_MCKR);
	tmp &= (~(0x1 << 13));
	if (IS_ENABLED(CONFIG_SOC_AT91SAM9X5) || IS_ENABLED(CONFIG_SOC_AT91SAM9N12)
	    || IS_ENABLED(CONFIG_SOC_SAMA5)) {
		tmp &= (~AT91C_PMC_ALT_PRES);
		tmp |= (pmc_mckr & AT91C_PMC_ALT_PRES);
	} else {
		tmp &= (~AT91C_PMC_PRES);
		tmp |= (pmc_mckr & AT91C_PMC_PRES);
	}
	at91_pmc_write(AT91_PMC_MCKR, tmp);

	/*
	 * Program the MDIV field in the AT91_PMC_MCKR register
	 */
	tmp = at91_pmc_read(AT91_PMC_MCKR);
	tmp &= (~AT91_PMC_MDIV);
	tmp |= (pmc_mckr & AT91_PMC_MDIV);
	at91_pmc_write(AT91_PMC_MCKR, tmp);

	/*
	 * Program the PLLADIV2 field in the AT91_PMC_MCKR register
	 */
	tmp = at91_pmc_read(AT91_PMC_MCKR);
	tmp &= (~AT91_PMC_PLLADIV2);
	tmp |= (pmc_mckr & AT91_PMC_PLLADIV2);
	at91_pmc_write(AT91_PMC_MCKR, tmp);

	/*
	 * Program the H32MXDIV field in the AT91_PMC_MCKR register
	 */
	tmp = at91_pmc_read(AT91_PMC_MCKR);
	tmp &= (~AT91_PMC_H32MXDIV);
	tmp |= (pmc_mckr & AT91_PMC_H32MXDIV);
	at91_pmc_write(AT91_PMC_MCKR, tmp);

	/*
	 * Program the CSS field in the AT91_PMC_MCKR register,
	 * wait for MCKRDY bit to be set in the PMC_SR register
	 */
	tmp = at91_pmc_read(AT91_PMC_MCKR);
	tmp &= (~AT91_PMC_ALT_PCKR_CSS);
	tmp |= (pmc_mckr & AT91_PMC_ALT_PCKR_CSS);
	at91_pmc_write(AT91_PMC_MCKR, tmp);

	while (!(at91_pmc_read(AT91_PMC_SR) & AT91_PMC_MCKRDY))
		;

	return 0;
}


#define AT91C_CKGR_SRCA		(0x1UL << 29)
#define AT91C_PMC_LOCKA		(0x1UL << 1)
#define PLLA_SETTINGS		(BOARD_CKGR_PLLA \
				 | BOARD_PLLACOUNT \
				 | BOARD_MULA \
				 | BOARD_DIVA)
#define AT91C_CKGR_MULA		(0xff << 16)
#define AT91C_CKGR_DIVA		(0xff << 0)
void lowlevel_clock_init(void);
void _hw_init(void)
{
	lowlevel_clock_init();
	/* Disable watchdog */ {
		unsigned int reg;
		reg = __raw_readl(SAMA5D3_BASE_WDT + AT91_WDT_MR);
		reg |= AT91_WDT_WDDIS;
		__raw_writel(reg, SAMA5D3_BASE_WDT + AT91_WDT_MR);
	}

	/*
	 * At this stage the main oscillator
	 * is supposed to be enabled PCK = MCK = MOSC
	 */

	/* Configure PLLA = MOSC * (PLL_MULA + 1) / PLL_DIVA */ {
		at91_pmc_write((unsigned int)AT91_CKGR_PLLAR, 0 | AT91C_CKGR_SRCA);
		at91_pmc_write((unsigned int)AT91_CKGR_PLLAR, PLLA_SETTINGS);

		while (!(at91_pmc_read(AT91_PMC_SR) & AT91C_PMC_LOCKA))
			;
	}

#define PMC_PLLICPR	0x80	/* PLL Charge Pump Current Register */
#define 	AT91C_PMC_IPLLA_3		(0x3UL <<  8)
	/* Initialize PLLA charge pump */
	at91_pmc_write(PMC_PLLICPR, AT91C_PMC_IPLLA_3);

	/* Switch PCK/MCK on Main clock output */
	_pmc_cfg_mck(BOARD_PRESCALER_MAIN_CLOCK);


	/* Switch PCK/MCK on PLLA output */
	_pmc_cfg_mck(BOARD_PRESCALER_PLLA);

	/* Init timer */

}



/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2006, Atmel Corporation
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

void lowlevel_clock_init(void)
{
	unsigned long tmp;

	/*
	 * Switch the master clock to the slow clock without modifying other
	 * parameters. It is assumed that ROM code set H32MXDIV, PLLADIV2,
	 * PCK_DIV3.
	 */
	tmp = at91_pmc_read(AT91_PMC_MCKR);
	tmp &= (~AT91_PMC_ALT_PCKR_CSS);
	tmp |= AT91_PMC_CSS_SLOW;
	at91_pmc_write(AT91_PMC_MCKR, tmp);

	while (!(at91_pmc_read(AT91_PMC_SR) & AT91_PMC_MCKRDY))
		;

	if (IS_ENABLED(CONFIG_SOC_AT91SAM9X5) || IS_ENABLED(CONFIG_SOC_AT91SAM9N12)
	   || IS_ENABLED(CONFIG_SOC_SAMA5)) {
		/*
		 * Enable the Main Crystal Oscillator
		 * tST_max = 2ms
		 * Startup Time: 32768 * 2ms / 8 = 8
		 */
		tmp = at91_pmc_read(AT91_CKGR_MOR);
		tmp &= (~AT91_PMC_OSCOUNT);
		tmp &= (~AT91_PMC_KEY_MASK);
		tmp |= AT91_PMC_MOSCEN;
		tmp |= AT91_PMC_OSCOUNT_(8);
		tmp |= AT91_PMC_KEY;
		at91_pmc_write(AT91_CKGR_MOR, tmp);

		while (!(at91_pmc_read(AT91_PMC_SR) & AT91_PMC_MOSCS))
			;

		/* Switch from internal 12MHz RC to the Main Cristal Oscillator */
		tmp = at91_pmc_read(AT91_CKGR_MOR);
		tmp &= (~AT91_PMC_OSCBYPASS);
		tmp &= (~AT91_PMC_KEY_MASK);
		tmp |= AT91_PMC_KEY;
		at91_pmc_write(AT91_CKGR_MOR, tmp);

		tmp = at91_pmc_read(AT91_CKGR_MOR);
		tmp |= AT91_PMC_MOSCSEL;
		tmp &= (~AT91_PMC_KEY_MASK);
		tmp |= AT91_PMC_KEY;
		at91_pmc_write(AT91_CKGR_MOR, tmp);

		while (!(at91_pmc_read(AT91_PMC_SR) & AT91_PMC_MOSCSELS))
			;

		if (!IS_ENABLED(CONFIG_ARCH_SAMA5D4)) {
			/* Disable the 12MHz RC Oscillator */
			tmp = at91_pmc_read(AT91_CKGR_MOR);
			tmp &= (~AT91_PMC_MOSCRCEN);
			tmp &= (~AT91_PMC_KEY_MASK);
			tmp |= AT91_PMC_KEY;
			at91_pmc_write(AT91_CKGR_MOR, tmp);
		}

	} else {
		/*
		 * Enable the Main Crystal Oscillator
		 * tST_max = 2ms
		 * Startup Time: 32768 * 2ms / 8 = 8
		 */
		tmp = at91_pmc_read(AT91_CKGR_MOR);
		tmp &= (~AT91_PMC_OSCOUNT);
		tmp |= AT91_PMC_MOSCEN;
		tmp |= AT91_PMC_OSCOUNT_(8);
		at91_pmc_write(AT91_CKGR_MOR, tmp);

		while (!(at91_pmc_read(AT91_PMC_SR) & AT91_PMC_MOSCXTS))
			;
	}

	/* After stablization, switch to Main Clock */
	if ((at91_pmc_read(AT91_PMC_MCKR) & AT91_PMC_ALT_PCKR_CSS) == AT91_PMC_CSS_SLOW) {
		tmp = at91_pmc_read(AT91_PMC_MCKR);
		tmp &= (~(0x1 << 13));
		tmp &= ~AT91_PMC_ALT_PCKR_CSS;
		tmp |= AT91_PMC_CSS_MAIN;
		at91_pmc_write(AT91_PMC_MCKR, tmp);

		while (!(at91_pmc_read(AT91_PMC_SR) & AT91_PMC_MCKRDY))
			;

		tmp &= ~AT91C_PMC_PRES;
		tmp |= AT91_PMC_PRES_1;
		at91_pmc_write(AT91_PMC_MCKR, tmp);

		while (!(at91_pmc_read(AT91_PMC_SR) & AT91_PMC_MCKRDY))
			;
	}
}
