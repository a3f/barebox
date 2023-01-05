/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __MACH_ROCKCHIP_H
#define __MACH_ROCKCHIP_H

#include <errno.h>

#ifdef CONFIG_ARCH_RK3188
int rk3188_init(void);
#else
static inline int rk3188_init(void)
{
	return -ENOTSUPP;
}
#endif

#ifdef CONFIG_ARCH_RK3288
int rk3288_init(void);
#else
static inline int rk3288_init(void)
{
	return -ENOTSUPP;
}
#endif

#ifdef CONFIG_ARCH_PX30
int px30_init(void);
#else
static inline int px30_init(void)
{
	return -ENOTSUPP;
}
#endif

#ifdef CONFIG_ARCH_RK3399
int rk3399_init(bool pro);
#else
static inline int rk3399_init(bool pro)
{
	return -ENOTSUPP;
}
#endif

#ifdef CONFIG_ARCH_RK3568
int rk3568_init(void);
#define PMU_GRF		0xfdc20000
#define PMU_GRF_IO_VSEL0	(PMU_GRF + 0x140)
#define PMU_GRF_IO_VSEL1	(PMU_GRF + 0x144)
#else
static inline int rk3568_init(void)
{
	return -ENOTSUPP;
}
#endif

#ifdef CONFIG_ARCH_RK3588
int rk3588_init(void);
#else
static inline int rk3588_init(void)
{
	return -ENOTSUPP;
}
#endif

void px30_lowlevel_init(void);
void rk3399_lowlevel_init(void);
void rk3568_lowlevel_init(void);
void rk3588_lowlevel_init(void);

int rockchip_soc(void);

#endif /* __MACH_ROCKCHIP_H */
