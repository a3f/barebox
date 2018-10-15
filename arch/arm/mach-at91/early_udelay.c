// SPDX-License-Identifier: BSD-1-Clause
/*
 * Copyright (c) 2012, Atmel Corporation
 */

#include <mach/hardware.h>
#include <asm/io.h>
#include <mach/at91_lowlevel_clock.h>
#include <mach/at91_pit.h>
#include <mach/early_udelay.h>

static unsigned int master_clock;
static void __iomem *pmc, *pit;

static bool pmc_check_mck_h32mxdiv(void)
{
	if (IS_ENABLED(CONFIG_HAVE_AT91_H32MXDIV))
		return __raw_readl(pmc + AT91_PMC_MCKR) & AT91_PMC_H32MXDIV;

	return false;
}

/* Because the below statement is used in the function:
 *	((MASTER_CLOCK >> 10) * usec) is used,
 * to our 32-bit system. the argu "usec" maximum value is:
 * supposed "MASTER_CLOCK" is 132M.
 *	132000000 / 1024 = 128906
 *	(0xffffffff) / 128906 = 33318.
 * So the maximum delay time is 33318 us.
 */
/* requires PIT to be initialized, but not the clocksource framework */
void early_udelay(unsigned int usec)
{
	unsigned int delay;
	unsigned int current;
	unsigned int base = __raw_readl(pit + AT91_PIT_PIIR);

	if (pmc_check_mck_h32mxdiv())
		master_clock /= 2;

	delay = ((master_clock >> 10) * usec) >> 14;

	do {
		current = __raw_readl(pit + AT91_PIT_PIIR);
		current -= base;
	} while (current < delay);
}

void early_udelay_init(void __iomem *pmc_base,
		       void __iomem *pit_base,
		       unsigned clock,
		       unsigned int master_clock_rate)
{
	master_clock = master_clock_rate;
	pmc = pmc_base;
	pit = pit_base;

	__raw_writel(AT91_PIT_PIV | AT91_PIT_PITEN, pit + AT91_PIT_MR);

	at91_pmc_enable_periph_clock(pmc_base, clock);
}
