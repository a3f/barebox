/*
 * Header file for the Atmel DDR/SDR SDRAM Controller
 *
 * Copyright (C) 2010 Atmel Corporation
 *	Nicolas Ferre <nicolas.ferre@atmel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef AT91SAM9_DDRSDR_H
#define AT91SAM9_DDRSDR_H

#include <mach/at91_ddrsdrc.h>

#ifndef __ASSEMBLY__
#include <common.h>
#include <io.h>

static inline u32 at91_get_ddram_size(void * __iomem base, bool is_nb)
{
	u32 cr;
	u32 mdr;
	u32 size;
	bool is_sdram;

	cr = readl(base + AT91C_HDDRSDRC2_CR);
	mdr = readl(base + AT91C_HDDRSDRC2_MDR);

	/* will always be false for sama5d2, sama5d3 or sama5d4 */
	is_sdram = (mdr & AT91C_DDRC2_MD) <= AT91C_DDRC2_MD_LP_SDR_SDRAM;

	/* Formula:
	 * size = bank << (col + row + 1);
	 * if (bandwidth == 32 bits)
	 *	size <<= 1;
	 */
	size = 1;
	/* COL */
	size += (cr & AT91C_DDRC2_NC) + 8;
	if (!is_sdram)
		size ++;
	/* ROW */
	size += ((cr & AT91C_DDRC2_NR) >> 2) + 11;
	/* BANK */
	if (is_nb)
		size = ((cr & AT91C_DDRC2_NB_BANKS) ? 8 : 4) << size;
	else
		size = 4 << size;

	/* bandwidth */
	if (!(mdr & AT91C_DDRC2_DBW))
		size <<= 1;

	return size;
}

#endif

#endif
