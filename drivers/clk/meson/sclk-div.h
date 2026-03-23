/* SPDX-Comment: Origin-URL: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/clk/meson/sclk-div.h?id=889c2b7ec42b8d14d421541f0a3ae1238e34891e */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 BayLibre, SAS.
 * Author: Jerome Brunet <jbrunet@baylibre.com>
 */

#ifndef __MESON_SCLK_DIV_H
#define __MESON_SCLK_DIV_H

#include <linux/clk-provider.h>
#include "parm.h"

struct meson_sclk_div_data {
	struct parm div;
	struct parm hi;
	unsigned int cached_div;
	struct clk_duty cached_duty;
};

extern const struct clk_ops meson_sclk_div_ops;

#endif /* __MESON_SCLK_DIV_H */
