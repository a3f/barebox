// SPDX-License-Identifier: BSD-1-Clause
/*
 * Copyright (c) 2006, Atmel Corporation
 */

#ifndef AT91_LOWLEVEL_CLOCK_H
#define AT91_LOWLEVEL_CLOCK_H

#include <errno.h>
#include <asm/io.h>
#include <mach/at91_pmc.h>

void at91_lowlevel_clock_init(void __iomem *pmc_base);
void at91_pmc_cfg_mck(void __iomem *pmc_base, unsigned int pmc_mckr);
static inline int at91_pmc_enable_periph_clock(void __iomem *pmc_base,
					  unsigned periph_id)
{
	u32 mask = 0x01 << (periph_id % 32);

	if ((periph_id / 32) == 1)
		__raw_writel(mask, pmc_base + AT91_PMC_PCER1);
	else if ((periph_id / 32) == 0)
		__raw_writel(mask, pmc_base + AT91_PMC_PCER);
	else
		return -EINVAL;

	return 0;
}

#endif
