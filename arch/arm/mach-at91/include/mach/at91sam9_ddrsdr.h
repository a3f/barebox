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

	cr = __raw_readl(base + HDDRSDRC2_CR);
	mdr = __raw_readl(base + HDDRSDRC2_MDR);

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

#ifdef CONFIG_SOC_AT91SAM9G45
#include <mach/at91sam9g45.h>
static inline u32 at91sam9g45_get_ddram_size(int bank)
{
	switch (bank) {
	case 0:
		return at91_get_ddram_size(IOMEM(AT91SAM9G45_BASE_DDRSDRC0), false);
	case 1:
		return at91_get_ddram_size(IOMEM(AT91SAM9G45_BASE_DDRSDRC1), false);
	default:
		return 0;
	}
}
#else
static inline u32 at91sam9g45_get_ddram_size(int bank)
{
	return 0;
}
#endif

#ifdef CONFIG_SOC_AT91SAM9X5
#include <mach/at91sam9x5.h>
static inline u32 at91sam9x5_get_ddram_size(void)
{
	return at91_get_ddram_size(IOMEM(AT91SAM9X5_BASE_DDRSDRC0), true);
}
#else
static inline u32 at91sam9x5_get_ddram_size(void)
{
	return 0;
}
#endif

#ifdef CONFIG_SOC_AT91SAM9N12
#include <mach/at91sam9n12.h>
static inline u32 at91sam9n12_get_ddram_size(void)
{
	return at91_get_ddram_size(IOMEM(AT91SAM9N12_BASE_DDRSDRC0), true);
}
#else
static inline u32 at91sam9n12_get_ddram_size(void)
{
	return 0;
}
#endif

#ifdef CONFIG_SOC_SAMA5
#include <mach/sama5d3.h>
static inline u32 at91sama5_get_ddram_size(void)
{
	u32 cr;
	u32 mdr;
	u32 size;
	void * __iomem base = IOMEM(SAMA5D3_BASE_MPDDRC);

	cr = __raw_readl(base + HDDRSDRC2_CR);
	mdr = __raw_readl(base + HDDRSDRC2_MDR);

	/* Formula:
	 * size = bank << (col + row + 1);
	 * if (bandwidth == 32 bits)
	 *	size <<= 1;
	 */
	size = 1;
	/* COL */
	size += (cr & AT91C_DDRC2_NC) + 9;
	/* ROW */
	size += ((cr & AT91C_DDRC2_NR) >> 2) + 11;
	/* BANK */
	size = ((cr & AT91C_DDRC2_NB_BANKS) ? 8 : 4) << size;

	/* bandwidth */
	if (!(mdr & AT91C_DDRC2_DBW))
		size <<= 1;

	return size;
}
#else
static inline u32 at91sama5_get_ddram_size(void)
{
	return 0;
}
#endif

#endif

#endif
