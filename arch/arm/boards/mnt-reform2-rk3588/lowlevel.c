// SPDX-License-Identifier: GPL-2.0-only
#include <common.h>
#include <linux/sizes.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <mach/rockchip/hardware.h>
#include <mach/rockchip/atf.h>
#include <debug_ll.h>
#include <mach/rockchip/rockchip.h>

extern char __dtb_rk3588_mnt_reform2_start[];

/*
  Adapted from U-Boot:

  - drivers/clk/rockchip/clk_rk3588.c
    - Copyright (c) 2021 Fuzhou Rockchip Electronics Co., Ltd
    - Author: Elaine Zhang <zhangqing@rock-chips.com>
  - drivers/clk/rockchip/clk_pll.c
    - (C) Copyright 2018-2019 Rockchip Electronics Co., Ltd
  - arch/arm/include/asm/arch-rockchip/cru_rk3588.h
    - Copyright (c) 2020 Rockchip Electronics Co. Ltd.
    - Author: Elaine Zhang <zhangqing@rock-chips.com>
  - arch/arm/include/asm/arch-rockchip/clock.h
    - (C) Copyright 2015 Google, Inc

  Copyright 2026 MNT Research GmbH
  Author: Lucie L. Hartmann
*/

#define RK3588_PLL_CON(x)               ((x) * 0x4)
#define RK3588_MODE_CON                 0x280

#define SBUSCRU_BASE			0xfd7d8000
#define RK3588_SBUSCRU_SPLL_CON(x)      ((x) * 0x4 + 0x220)
#define RK3588_SBUSCRU_MODE_CON0        0x280

#define RKCLK_PLL_MODE_SLOW             0
#define RKCLK_PLL_MODE_NORMAL           1
#define RKCLK_PLL_MODE_DEEP             2

#define RK3588_PLLCON(i)                ((i) * 0x4)
#define RK3588_PLLCON0_M_MASK           0x3ff << 0
#define RK3588_PLLCON0_M_SHIFT          0
#define RK3588_PLLCON1_P_MASK           0x3f << 0
#define RK3588_PLLCON1_P_SHIFT          0
#define RK3588_PLLCON1_S_MASK           0x7 << 6
#define RK3588_PLLCON1_S_SHIFT          6
#define RK3588_PLLCON1_PWRDOWN          BIT(13)
#define RK3588_PLLCON6_LOCK_STATUS      BIT(15)

/* TODD: do we need to set K to 0? */

static void rk3588_spll_set_rate(void);

static void rk3588_spll_set_rate(void) {
	void __iomem *base = (void __iomem *)SBUSCRU_BASE;

	// 702000000 Hz
	unsigned int p = 3;
	unsigned int m = 351;
	unsigned int s = 2;

	unsigned int con_offset = RK3588_SBUSCRU_SPLL_CON(0);
	unsigned int mode_offset = RK3588_SBUSCRU_MODE_CON0;
	unsigned int mode_mask = 15;
	unsigned int mode_shift = 0;

	/* force PLL into slow mode to ensure stable clock output */
	rk_clrsetreg(base + mode_offset,
							 mode_mask << mode_shift,
							 RKCLK_PLL_MODE_SLOW << mode_shift);

	/* power down */
	rk_setreg(base + con_offset + RK3588_PLLCON(1),
						RK3588_PLLCON1_PWRDOWN);
	rk_clrsetreg(base + con_offset,
							 RK3588_PLLCON0_M_MASK,
							 (m << RK3588_PLLCON0_M_SHIFT));
	rk_clrsetreg(base + con_offset + RK3588_PLLCON(1),
							 (RK3588_PLLCON1_P_MASK | RK3588_PLLCON1_S_MASK),
							 (p << RK3588_PLLCON1_P_SHIFT | s << RK3588_PLLCON1_S_SHIFT));

	/* power up */
	rk_clrreg(base + con_offset + RK3588_PLLCON(1),
						RK3588_PLLCON1_PWRDOWN);

	/* wait for PLL lock */
	while (!(readl(base + con_offset + RK3588_PLLCON(6)) &
					 RK3588_PLLCON6_LOCK_STATUS)) {
		udelay(1);
	}
};

ENTRY_FUNCTION(start_rk3588_mnt_reform2, r0, r1, r2)
{
	putc_ll('M');
	putc_ll('N');
	putc_ll('T');
	putc_ll('R');
	putc_ll('E');
	putc_ll('>');

	rk3588_spll_set_rate();

	putc_ll('>');
	if (current_el() == 3)
		relocate_to_adr_full(RK3588_BAREBOX_LOAD_ADDRESS);
	else
		relocate_to_current_adr();

	setup_c();

	rk3588_barebox_entry(__dtb_rk3588_mnt_reform2_start);
}
