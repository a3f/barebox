// SPDX-License-Identifier: GPL-2.0+
#include <common.h>
#include <linux/sizes.h>
#include <init.h>
#include <asm/memory.h>
#include <mach/stm32.h>
#include <mfd/syscon.h>

#define SYSCFG_BOOTR		0x00
#define SYSCFG_PMCSETR		0x04
#define SYSCFG_IOCTRLSETR	0x18
#define SYSCFG_ICNR		0x1C
#define SYSCFG_CMPCR		0x20
#define SYSCFG_CMPENSETR	0x24
#define SYSCFG_PMCCLRR		0x44

#define SYSCFG_BOOTR_BOOT_MASK		GENMASK(2, 0)
#define SYSCFG_BOOTR_BOOTPD_SHIFT	4

#define SYSCFG_IOCTRLSETR_HSLVEN_TRACE		BIT(0)
#define SYSCFG_IOCTRLSETR_HSLVEN_QUADSPI	BIT(1)
#define SYSCFG_IOCTRLSETR_HSLVEN_ETH		BIT(2)
#define SYSCFG_IOCTRLSETR_HSLVEN_SDMMC		BIT(3)
#define SYSCFG_IOCTRLSETR_HSLVEN_SPI		BIT(4)

#define SYSCFG_CMPCR_SW_CTRL		BIT(1)
#define SYSCFG_CMPCR_READY		BIT(8)

#define SYSCFG_CMPENSETR_MPU_EN		BIT(0)

#define SYSCFG_PMCSETR_ETH_CLK_SEL	BIT(16)
#define SYSCFG_PMCSETR_ETH_REF_CLK_SEL	BIT(17)

#define SYSCFG_PMCSETR_ETH_SELMII	BIT(20)

#define SYSCFG_PMCSETR_ETH_SEL_MASK	GENMASK(23, 16)
#define SYSCFG_PMCSETR_ETH_SEL_GMII_MII	(0 << 21)
#define SYSCFG_PMCSETR_ETH_SEL_RGMII	(1 << 21)
#define SYSCFG_PMCSETR_ETH_SEL_RMII	(4 << 21)

static int dk2_postcore_init(void)
{
	if (!of_machine_is_compatible("st,stm32mp157c-dk2"))
		return 0;

	arm_add_mem_device("ram0", STM32_DDR_BASE, SZ_512M);

	return 0;
}
mem_initcall(dk2_postcore_init);

static int dk2_sysconf_init(void)
{
	struct regmap *syscfg;
	u32 reg;

	if (!of_machine_is_compatible("st,stm32mp157c-dk2"))
		return 0;

	if (IS_ENABLED(CONFIG_BOOTM_OPTEE))
		return 0;

	syscfg = syscon_regmap_lookup_by_compatible("st,stm32mp157-syscfg");
	pr_debug("SYSCFG: init @0x%p\n", syscfg);

	/* interconnect update : select master using the port 1 */
	/* LTDC = AXI_M9 */
	/* GPU  = AXI_M8 */
	/* today information is hardcoded in U-Boot */
	regmap_write(syscfg, SYSCFG_ICNR, BIT(9));
	regmap_read(syscfg, SYSCFG_ICNR, &reg);
	pr_debug("[0x%x] SYSCFG.icnr = 0x%08x (LTDC and GPU)\n",
	      (u32)syscfg + SYSCFG_ICNR, reg);

	/* disable Pull-Down for boot pin connected to VDD */
	regmap_read(syscfg, SYSCFG_BOOTR, &reg);
	reg &= ~(SYSCFG_BOOTR_BOOT_MASK << SYSCFG_BOOTR_BOOTPD_SHIFT);
	reg |= (reg & SYSCFG_BOOTR_BOOT_MASK) << SYSCFG_BOOTR_BOOTPD_SHIFT;
	regmap_write(syscfg, SYSCFG_BOOTR, reg);
	regmap_read(syscfg, SYSCFG_BOOTR, &reg);
	pr_debug("[0x%x] SYSCFG.bootr = 0x%08x\n",
	      (u32)syscfg + SYSCFG_BOOTR, reg);

	regmap_read(syscfg, SYSCFG_IOCTRLSETR, &reg);
	pr_debug("[0x%x] SYSCFG.IOCTRLSETR = 0x%08x\n",
	      (u32)syscfg + SYSCFG_IOCTRLSETR, reg);

	/* activate automatic I/O compensation
	 * warning: need to ensure CSI enabled and ready in clock driver
	 */
	regmap_write(syscfg, SYSCFG_CMPENSETR, SYSCFG_CMPENSETR_MPU_EN);

	do {
		regmap_read(syscfg, SYSCFG_CMPCR, &reg);
	} while (!(reg & SYSCFG_CMPCR_READY));

	regmap_read(syscfg, SYSCFG_CMPCR, &reg);
	regmap_write(syscfg, SYSCFG_CMPCR, reg & ~SYSCFG_CMPCR_SW_CTRL);

	regmap_read(syscfg, SYSCFG_CMPCR, &reg);
	pr_debug("[0x%x] SYSCFG.cmpcr = 0x%08x\n",
	      (u32)syscfg + SYSCFG_CMPCR, reg);

	return 0;
}
coredevice_initcall(dk2_sysconf_init);
