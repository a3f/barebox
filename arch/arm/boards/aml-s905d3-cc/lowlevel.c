// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <asm/barebox-arm.h>
//#include <mach/rockchip/hardware.h>
//#include <mach/rockchip/atf.h>
#include <debug_ll.h>

ENTRY_FUNCTION(start_aml_s905d3_cc, r0, r1, r2)
{
	putc_ll('>');
	setup_c();
}
