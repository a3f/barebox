// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2013 Steffen Trumtrar <s.trumtrar@pengutronix.de>

#include <common.h>
#include <io.h>
#include <asm/barebox-arm.h>
#include <asm/barebox-arm-head.h>
#include <mach/zynq/init.h>
#include <mach/zynq/zynq7000-regs.h>
#include <serial/cadence.h>

extern char __dtb_z_zynq_zturn_v5_start[];

static void init_console(void)
{
	cadence_uart_init((void *)ZYNQ_UART1_BASE_ADDR);
	pbl_set_putc(cadence_uart_putc, (void *)ZYNQ_UART1_BASE_ADDR);

	pr_info("\nHello World\n");
}

// TODO: 0 assumes that previous stage has set up RAM.
// Alternative, if that's not the case would be 0x200000
ENTRY_FUNCTION_WITHSTACK(start_myir_zturn, 0, r0, r1, r2)
{
	zynq_cpu_lowlevel_init();

	relocate_to_current_adr();
	setup_c();
	barrier();

	init_console();

	barebox_arm_entry(0, SZ_512M, __dtb_z_zynq_zturn_v5_start);
}
