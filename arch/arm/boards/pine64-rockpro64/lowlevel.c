// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2021 Ahmad Fatoum, Pengutronix

#include <common.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <asm/reloc.h>
#include <mach/rockchip/atf.h>
#include <debug_ll.h>

ENTRY_FUNCTION_WITHSTACK(start_pine64_rockpro64, ROCKCHIP_BAREBOX_LOAD_ADDRESS, x0, x1, x2)
{
	extern char __dtb_rk3399_rockpro64_v2_start[];

	putc_ll('>');

	rk3399_barebox_entry(runtime_address(__dtb_rk3399_rockpro64_v2_start));
}
