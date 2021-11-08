/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __ROCKCHIP_STIMER_H_
#define __ROCKCHIP_STIMER_H_

#define TIMER_END_COUNT_L	0x00
#define TIMER_END_COUNT_H	0x04
#define TIMER_INIT_COUNT_L	0x10
#define TIMER_INIT_COUNT_H	0x14
#define TIMER_CONTROL_REG	0x1c

#define TIMER_EN	0x1
#define TIMER_FMODE	BIT(0)
#define TIMER_RMODE	BIT(1)

static inline void rockchip_stimer_init(void __iomem *base)
{
	if (readl(base + TIMER_CONTROL_REG) & TIMER_EN)
		return;

	writel(0xffffffff, base + TIMER_END_COUNT_L);
	writel(0xffffffff, base + TIMER_END_COUNT_H);
	writel(0, base + TIMER_INIT_COUNT_L);
	writel(0, base + TIMER_INIT_COUNT_H);
	writel(TIMER_EN | TIMER_FMODE, base + TIMER_CONTROL_REG);
}

#endif
