// SPDX-License-Identifier: GPL-2.0-only

#include <mach/sifive/entry.h>

extern char __dtb_z_hifive_unmatched_a00_start[];
extern char __dtb_z_hifive_unleashed_a00_start[];

/* Both boards boot with their built-in device tree, so that's all they add */

ENTRY_POINT(FU740, start_hifive_unmatched)
	.second_stage.board_data = __dtb_z_hifive_unmatched_a00_start,
ENTRY_POINT_END

ENTRY_POINT(FU540, start_hifive_unleashed)
	.second_stage.board_data = __dtb_z_hifive_unleashed_a00_start,
ENTRY_POINT_END
