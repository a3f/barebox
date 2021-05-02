// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2021 Ahmad Fatoum, Pengutronix

#include <common.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <asm/reloc.h>
#include <mach/rockchip/atf.h>
#include <debug_ll.h>

ENTRY_FUNCTION_WITHSTACK(start_radxa_rock_pi_n10, ROCKCHIP_BAREBOX_LOAD_ADDRESS, x0, x1, x2)
{
	extern char __dtb_rk3399pro_rock_pi_n10_start[];

	putc_ll('>');

	if (current_el() == 3)
		relocate_to_adr_full(ROCKCHIP_BAREBOX_LOAD_ADDRESS);
	else
		relocate_to_current_adr();

	setup_c();

	rk3399_barebox_entry(runtime_address(__dtb_rk3399pro_rock_pi_n10_start));
}
