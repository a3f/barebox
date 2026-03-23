/* SPDX-Comment: Origin-URL: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/clk/meson/clk-phase.h?id=7b70689b07c1d336a5ad6906927aa413619029bd */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 BayLibre, SAS.
 * Author: Jerome Brunet <jbrunet@baylibre.com>
 */

#ifndef __MESON_CLK_PHASE_H
#define __MESON_CLK_PHASE_H

#include <linux/clk-provider.h>
#include "parm.h"

struct meson_clk_phase_data {
	struct parm ph;
};

struct meson_clk_triphase_data {
	struct parm ph0;
	struct parm ph1;
	struct parm ph2;
};

struct meson_sclk_ws_inv_data {
	struct parm ph;
	struct parm ws;
};

extern const struct clk_ops meson_clk_phase_ops;
extern const struct clk_ops meson_clk_triphase_ops;
extern const struct clk_ops meson_sclk_ws_inv_ops;

#endif /* __MESON_CLK_PHASE_H */
