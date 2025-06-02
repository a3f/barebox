/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef ROCKCHIP_GPIO_H_
#define ROCKCHIP_GPIO_H_

#include <linux/io.h>

enum {
	RK_GPIOV2_DR_L	= 0x00,
	RK_GPIOV2_DR_H	= 0x04,
	RK_GPIOV2_DDR_L	= 0x08,
	RK_GPIOV2_DDR_H	= 0x0c,
	RK_GPIOV2_EXT_PORT = 0x70,
};

static inline int rockchip_gpiov2_direction_output(void __iomem *bank,
						   unsigned int gpio, int val)
{
	u32 mask, out, vval = 0;

	mask = 1 << (16 + (gpio % 16));
	out = 1 << (gpio % 16);
	if (val)
		vval = 1 << (gpio % 16);

	if (gpio < 16) {
		writel(mask | vval, bank + RK_GPIOV2_DR_L);
		writel(mask | out, bank + RK_GPIOV2_DDR_L);
	} else {
		writel(mask | vval, bank + RK_GPIOV2_DR_H);
		writel(mask | out, bank + RK_GPIOV2_DDR_H);
	}

	return 0;
}

#endif
