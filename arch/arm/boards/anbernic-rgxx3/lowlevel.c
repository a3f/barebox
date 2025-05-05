// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <asm/barebox-arm.h>
#include <mach/rockchip/atf.h>
#include <debug_ll.h>
#include <mach/rockchip/debug_ll.h>

#include <mach/rockchip/gpio.h>
#include <dt-bindings/pinctrl/rockchip.h>

extern char __dtb_rk3566_powkiddy_rgb30_start[];

static void led_init(void)
{
	writel(0xff00000, 0xfdc20000 + 0x14);
}

static void board_debug_uart_init(void)
{
	void __iomem *base = IOMEM(RK_UART_BASE(RK_DEBUG_SOC,
		CONFIG_DEBUG_ROCKCHIP_UART_PORT));
	unsigned int divisor;

#if 0
	putc_ll('[');
	divisor = debug_ll_ns16550_calc_divisor(RK_DEBUG_UART_CLOCK * 2);
	puthex_ll(divisor);
	putc_ll('\n');
	putc_ll('(');
	puthex_ll(debug_ll_read_reg(base, NS16550_LCR)); //, 0x0); /* select ier reg */
	putc_ll('\n');
	putc_ll('<');
	puthex_ll(debug_ll_read_reg(base, NS16550_IER)); //, 0x0); /* disable interrupts */
	putc_ll('\n');

	putc_ll('@');
	puthex_ll(debug_ll_read_reg(base, NS16550_LCR));//, NS16550_LCR_BKSE);
	putc_ll('\n');
	putc_ll('~');
	puthex_ll(debug_ll_read_reg(base, NS16550_DLL));//, divisor & 0xff);
	putc_ll('\n');
	putc_ll('/');
	puthex_ll(debug_ll_read_reg(base, NS16550_DLM));//, (divisor >> 8) & 0xff);
	putc_ll('\n');
	puthex_ll(debug_ll_read_reg(base, NS16550_FCR));//, NS16550_FCR_VAL);
	putc_ll('\n');
#endif
}

ENTRY_FUNCTION(start_anbernic_rgxx3, r0, r1, r2)
{
	led_init();

#if 1
	board_debug_uart_init();
	//rockchip_debug_ll_init();
#endif

	putc_ll('>');

#if 1

	if (current_el() == 3) {
		relocate_to_adr_full(RK3568_BAREBOX_LOAD_ADDRESS);
	} else {
		relocate_to_current_adr();
	}

	setup_c();

	rk3568_barebox_entry(__dtb_rk3566_powkiddy_rgb30_start);
#endif
}
