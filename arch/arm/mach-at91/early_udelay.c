/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support (at91bootstrap)
 * ----------------------------------------------------------------------------
 * Copyright (c) 2012, Atmel Corporation
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

#include <mach/hardware.h>
#include <asm/io.h>
#include <mach/at91_pmc.h>
#include <mach/at91_pit.h>
#include <mach/early_udelay.h>

static unsigned int master_clock;
static void __iomem *pmc, *pit;

static int pmc_check_mck_h32mxdiv(void)
{
	if (IS_ENABLED(CONFIG_HAVE_AT91_H32MXDIV))
		return __raw_readl(pmc + AT91_PMC_MCKR) & AT91_PMC_H32MXDIV;

	return 0;
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
		       unsigned int master_clock_rate)
{
	master_clock = master_clock_rate;
	pmc = pmc_base;
	pit = pit_base;

	__raw_writel(AT91_PIT_PIV | AT91_PIT_PITEN, pit + AT91_PIT_MR);

	/* Enable PITC Clock */
#ifdef AT91_ID_PIT
	pmc_enable_periph_clock(pmc_base, AT91_ID_PIT);
#else
	pmc_enable_periph_clock(pmc_base, AT91_ID_SYS);
#endif
}
