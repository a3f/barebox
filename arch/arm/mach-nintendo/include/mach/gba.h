/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019 Ahmad Fatoum <a.fatoum@pengutronix.de>
 */
#ifndef __MACH_GBA_H_
#define __MACH_GBA_H_

#include <linux/sizes.h>

#define GBA_SYSROM_BASE		0x00000000
#define GBA_SYSROM_SIZE		SZ_16K
#define GBA_EWRAM_BASE		0x02000000
#define GBA_EWRAM_SIZE		SZ_256K
#define GBA_IWRAM_BASE		0x03000000
#define GBA_IWRAM_SIZE		SZ_32K
#define GBA_PAKROM0_BASE	0x08000000

#define GBA_MMIO_BASE		0x04000000
#define GBA_SERIAL_BASE		(GBA_MMIO_BASE + 0x120)

#endif
