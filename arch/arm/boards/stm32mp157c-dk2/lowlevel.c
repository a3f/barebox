// SPDX-License-Identifier: GPL-2.0+
#include <common.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <mach/stm32.h>
#include <debug_ll.h>

#define RCC_MP_APB1ENSETR (STM32_RCC_BASE + 0x0A00)
#define RCC_MP_AHB4ENSETR (STM32_RCC_BASE + 0x0A28)
#define GPIOG_BASE 0x50008000

static inline void setup_uart(void)
{
	/* UART4 clock enable */
	setbits_le32(RCC_MP_APB1ENSETR, BIT(16));

	/* GPIOG clock enable */
	writel(BIT(6), RCC_MP_AHB4ENSETR);
	/* GPIO configuration for EVAL board => UART4 TX = G11 */
	writel(0xffbfffff, GPIOG_BASE + 0x00);
	writel(0x00006000, GPIOG_BASE + 0x24);

	putc_ll('>');
}

extern char __dtb_stm32mp157c_dk2_start[];

ENTRY_FUNCTION(start_stm32mp157c_dk2, r0, r1, r2)
{
	void *fdt;

	arm_cpu_lowlevel_init();

	if (IS_ENABLED(CONFIG_DEBUG_LL))
		setup_uart();

	fdt = __dtb_stm32mp157c_dk2_start + get_runtime_offset();

	barebox_arm_entry(STM32_DDR_BASE, SZ_512M, fdt);
}
