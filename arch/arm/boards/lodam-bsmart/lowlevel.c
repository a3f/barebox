#include <common.h>
#include <linux/sizes.h>
#include <mach/generic.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <mach/imx6-regs.h>
#include <io.h>
#include <debug_ll.h>
#include <mach/esdctl.h>
#include <asm/cache.h>
#include <asm/sections.h>
#include <image-metadata.h>

#define MX6_UART6_BASE_ADDR             (MX6_AIPS2_OFF_BASE_ADDR + 0x7C000)

static inline void setup_uart(void)
{
	void __iomem *uart = IOMEM(MX6_UART6_BASE_ADDR);

	imx6_ungate_all_peripherals();

	imx6_uart_setup(uart);
	pbl_set_putc(imx_uart_putc, uart);

	putc_ll('>');
}

BAREBOX_IMD_TAG_STRING(lodam_bsmart_memsize_SZ_512M, IMD_TYPE_PARAMETER, "memsize=512", 0);

extern char __dtb_imx6ul_lodam_bsmart_start[];

ENTRY_FUNCTION(start_imx6ul_lodam_bsmart, r0, r1, r2)
{
	void *fdt;

	IMD_USED(lodam_bsmart_memsize_SZ_512M);

	imx6ul_cpu_lowlevel_init();

	arm_setup_stack(0x00910000 - 8);

	arm_early_mmu_cache_invalidate();

	relocate_to_current_adr();
	setup_c();
	barrier();

	if (IS_ENABLED(CONFIG_DEBUG_LL))
		setup_uart();

	/* disable all watchdog powerdown counters */
	writew(0x0, 0x020bc008);
	writew(0x0, 0x020c0008);
	writew(0x0, 0x021e4008);

	fdt = __dtb_imx6ul_lodam_bsmart_start - get_runtime_offset();

	imx6ul_barebox_entry(fdt);
}
