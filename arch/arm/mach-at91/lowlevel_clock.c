// SPDX-License-Identifier: BSD-1-Clause
/*
 * Copyright (c) 2006, Atmel Corporation
 */

#include <common.h>
#include <mach/at91_lowlevel_clock.h>
#include <mach/at91_pmc.h>

#define at91_pmc_write(off, val) writel(val, pmc_base + off)
#define at91_pmc_read(off) readl(pmc_base + off)

void at91_lowlevel_clock_init(void __iomem *pmc_base)
{
	unsigned long tmp;

	/*
	 * Switch the master clock to the slow clock without modifying other
	 * parameters. It is assumed that ROM code set H32MXDIV, PLLADIV2,
	 * PCK_DIV3.
	 */
	tmp = at91_pmc_read(AT91_PMC_MCKR);
	tmp &= ~AT91_PMC_ALT_PCKR_CSS;
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
		tmp &= ~AT91_PMC_OSCOUNT;
		tmp &= ~AT91_PMC_KEY_MASK;
		tmp |= AT91_PMC_MOSCEN;
		tmp |= AT91_PMC_OSCOUNT_(8);
		tmp |= AT91_PMC_KEY;
		at91_pmc_write(AT91_CKGR_MOR, tmp);

		while (!(at91_pmc_read(AT91_PMC_SR) & AT91_PMC_MOSCS))
			;

		/* Switch from internal 12MHz RC to the Main Cristal Oscillator */
		tmp = at91_pmc_read(AT91_CKGR_MOR);
		tmp &= ~AT91_PMC_OSCBYPASS;
		tmp &= ~AT91_PMC_KEY_MASK;
		tmp |= AT91_PMC_KEY;
		at91_pmc_write(AT91_CKGR_MOR, tmp);

		tmp = at91_pmc_read(AT91_CKGR_MOR);
		tmp |= AT91_PMC_MOSCSEL;
		tmp &= ~AT91_PMC_KEY_MASK;
		tmp |= AT91_PMC_KEY;
		at91_pmc_write(AT91_CKGR_MOR, tmp);

		while (!(at91_pmc_read(AT91_PMC_SR) & AT91_PMC_MOSCSELS))
			;

		if (!IS_ENABLED(CONFIG_ARCH_SAMA5D4)) {
			/* Disable the 12MHz RC Oscillator */
			tmp = at91_pmc_read(AT91_CKGR_MOR);
			tmp &= ~AT91_PMC_MOSCRCEN;
			tmp &= ~AT91_PMC_KEY_MASK;
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
		tmp &= ~AT91_PMC_OSCOUNT;
		tmp |= AT91_PMC_MOSCEN;
		tmp |= AT91_PMC_OSCOUNT_(8);
		at91_pmc_write(AT91_CKGR_MOR, tmp);

		while (!(at91_pmc_read(AT91_PMC_SR) & AT91_PMC_MOSCXTS))
			;
	}

	/* After stablization, switch to Main Clock */
	if ((at91_pmc_read(AT91_PMC_MCKR) & AT91_PMC_ALT_PCKR_CSS) == AT91_PMC_CSS_SLOW) {
		tmp = at91_pmc_read(AT91_PMC_MCKR);
		tmp &= ~(0x1 << 13);
		tmp &= ~AT91_PMC_ALT_PCKR_CSS;
		tmp |= AT91_PMC_CSS_MAIN;
		at91_pmc_write(AT91_PMC_MCKR, tmp);

		while (!(at91_pmc_read(AT91_PMC_SR) & AT91_PMC_MCKRDY))
			;

		tmp &= ~AT91_PMC_PRES;
		tmp |= AT91_PMC_PRES_1;
		at91_pmc_write(AT91_PMC_MCKR, tmp);

		while (!(at91_pmc_read(AT91_PMC_SR) & AT91_PMC_MCKRDY))
			;
	}
}

void at91_pmc_cfg_mck(void __iomem *pmc_base, unsigned int pmc_mckr)
{
	u32 tmp;

	/*
	 * Program the PRES field in the AT91_PMC_MCKR register
	 */
	tmp = at91_pmc_read(AT91_PMC_MCKR);
	tmp &= ~(0x1 << 13);
	if (IS_ENABLED(CONFIG_SOC_AT91SAM9X5)
	    || IS_ENABLED(CONFIG_SOC_AT91SAM9N12)
	    || IS_ENABLED(CONFIG_SOC_SAMA5)) {
		tmp &= ~AT91_PMC_ALT_PRES;
		tmp |= pmc_mckr & AT91_PMC_ALT_PRES;
	} else {
		tmp &= ~AT91_PMC_PRES;
		tmp |= pmc_mckr & AT91_PMC_PRES;
	}
	at91_pmc_write(AT91_PMC_MCKR, tmp);

	/*
	 * Program the MDIV field in the AT91_PMC_MCKR register
	 */
	tmp = at91_pmc_read(AT91_PMC_MCKR);
	tmp &= ~AT91_PMC_MDIV;
	tmp |= pmc_mckr & AT91_PMC_MDIV;
	at91_pmc_write(AT91_PMC_MCKR, tmp);

	/*
	 * Program the PLLADIV2 field in the AT91_PMC_MCKR register
	 */
	tmp = at91_pmc_read(AT91_PMC_MCKR);
	tmp &= ~AT91_PMC_PLLADIV2;
	tmp |= pmc_mckr & AT91_PMC_PLLADIV2;
	at91_pmc_write(AT91_PMC_MCKR, tmp);

	/*
	 * Program the H32MXDIV field in the AT91_PMC_MCKR register
	 */
	tmp = at91_pmc_read(AT91_PMC_MCKR);
	tmp &= ~AT91_PMC_H32MXDIV;
	tmp |= pmc_mckr & AT91_PMC_H32MXDIV;
	at91_pmc_write(AT91_PMC_MCKR, tmp);

	/*
	 * Program the CSS field in the AT91_PMC_MCKR register,
	 * wait for MCKRDY bit to be set in the PMC_SR register
	 */
	tmp = at91_pmc_read(AT91_PMC_MCKR);
	tmp &= ~AT91_PMC_ALT_PCKR_CSS;
	tmp |= pmc_mckr & AT91_PMC_ALT_PCKR_CSS;
	at91_pmc_write(AT91_PMC_MCKR, tmp);

	while (!(at91_pmc_read(AT91_PMC_SR) & AT91_PMC_MCKRDY))
		;
}
