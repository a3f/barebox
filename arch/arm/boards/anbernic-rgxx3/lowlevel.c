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
	/* UART2 M0 */
#if 0
	rk_clrsetreg(&grf->iofunc_sel3, UART2_IO_SEL_MASK,
		     UART2_IO_SEL_M0 << UART2_IO_SEL_SHIFT);

	/* Switch iomux */
	rk_clrsetreg(&grf->gpio2a_iomux_l,
		     GPIO2A6_MASK | GPIO2A5_MASK,
		     (GPIO2A6_UART7_TXM0 << GPIO2A5_SHIFT) |
		     (GPIO2A5_UART7_RXM0 << GPIO2A6_SHIFT));

	/* UART2_M0 Switch iomux */
	rk_clrsetreg(&bus_ioc->gpio0b_iomux_sel_h,
		     GPIO0B6_MASK | GPIO0B5_MASK,
		     GPIO0B6_UART2_RX_M0 << GPIO0B6_SHIFT |
		     GPIO0B5_UART2_TX_M0 << GPIO0B5_SHIFT);
#endif

#if 1
	// rockchip-pinctrl pinctrl.of: setting mux of GPIO2-5 to 3
	writel(0x0, 0xfdc60000 + 0x24);
	writel(0xf00030, 0xfdc60000 + 0x24);
	// pinctrl pinctrl.of: setting pull of GPIO2-5 to 1
	writel(0x5555, 0xfdc60000 + 0x90);
	writel(0xc005555, 0xfdc60000 + 0x90);
	// pinctrl pinctrl.of: setting mux of GPIO2-6 to 3
	writel(0x300000, 0xfdc60000 + 0x310);
	writel(0x30, 0xfdc60000 + 0x24);
	writel(0xf000330, 0xfdc60000 + 0x24);
	// pinctrl pinctrl.of: setting pull of GPIO2-6 to 1
	writel(0x5555, 0xfdc60000 + 0x90);
	writel(0x30005555, 0xfdc60000 + 0x90);
#endif
}

ENTRY_FUNCTION(start_anbernic_rgxx3, r0, r1, r2)
{
	led_init();

	rockchip_debug_ll_init();
	board_debug_uart_init();

	putc_ll('.');

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
