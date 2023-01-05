// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2021 Ahmad Fatoum, Pengutronix

#include <common.h>
#include <linux/sizes.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <mach/rockchip/bootrom.h>
#include <mach/rockchip/hardware.h>
#include <mach/rockchip/px30-regs.h>
#include <mach/rockchip/atf.h>
#include <debug_ll.h>
#include <mach/rockchip/rockchip.h>

static void __noreturn continue_px30_iesy_osm(void)
{
	extern char __dtb_px30_iesy_osm_eva_mi_start[];
	ulong membase, memsize;
	void *fdt;

	membase = PX30_DRAM_BOTTOM;
	memsize = SZ_1G - PX30_DRAM_BOTTOM;
	fdt = __dtb_px30_iesy_osm_eva_mi_start;

	if (current_el() == 3) {
		px30_lowlevel_init();
		rockchip_store_bootrom_iram(membase, memsize, PX30_IRAM_BASE);
		px30_atf_load_bl31(fdt);
		/* not reached when CONFIG_ARCH_ROCKCHIP_ATF */
	}

	barebox_arm_entry(membase, memsize, fdt);
}

ENTRY_FUNCTION_WITHSTACK(start_px30_iesy_osm, PX30_BAREBOX_LOAD_ADDRESS, x0, x1, x2)
{
	/*
	 * Image execution starts at 0x0, but this is used for ATF
	 * later, so move away from here.
	 */
	if (current_el() == 3)
		relocate_to_adr_full(PX30_BAREBOX_LOAD_ADDRESS);
	else
		relocate_to_current_adr();

	setup_c();
	continue_px30_iesy_osm();
}
