// SPDX-License-Identifier: GPL-2.0+
#include <common.h>
#include <linux/sizes.h>
#include <init.h>
#include <asm/memory.h>
#include <mach/stm32.h>
#include <mfd/syscon.h>
#include <envfs.h>

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

static int dk2_devices_init(void)
{
	if (!of_machine_is_compatible("st,stm32mp157c-dk2"))
		return 0;

	barebox_set_hostname("stm32mp157c-dk2");

	defaultenv_append_directory(defaultenv_dk2);

	return 0;
}
device_initcall(dk2_devices_init);

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

	#if 0
	if (IS_ENABLED(CONFIG_DM_REGULATOR)) {
		u32 otp = 0;
		u32 val;
		int ret;
		struct device_d *pwr_dev, *pwr_reg, *dev;
		/* High Speed Low Voltage Pad mode Enable for SPI, SDMMC, ETH, QSPI
		 * and TRACE. Needed above ~50MHz and conditioned by AFMUX selection.
		 * The customer will have to disable this for low frequencies
		 * or if AFMUX is selected but the function not used, typically for
		 * TRACE. Otherwise, impact on power consumption.
		 *
		 * WARNING:
		 *   enabling High Speed mode while VDD>2.7V
		 *   with the OTP product_below_2v5 (OTP 18, BIT 13)
		 *   erroneously set to 1 can damage the IC!
		 *   => U-Boot set the register only if VDD < 2.7V (in DT)
		 *      but this value need to be consistent with board design
		 */
		ret = syscon_get_by_driver_data(STM32MP_SYSCON_PWR, &pwr_dev);
		if (!ret) {

			ret = uclass_get_device_by_driver(UCLASS_MISC,
					DM_GET_DRIVER(stm32mp_bsec),
					&dev);
			if (ret) {
				pr_err("Can't find stm32mp_bsec driver\n");
				return -ENODEV;
			}

			ret = misc_read(dev, STM32_BSEC_SHADOW(18), &otp, 4);
			if (!ret)
				otp = otp & BIT(13);

			/* get VDD = pwr-supply */
			ret = device_get_supply_regulator(pwr_dev, "pwr-supply",
					&pwr_reg);

			/* check if VDD is Low Voltage */
			if (!ret) {
				if (regulator_get_value(pwr_reg) < 2700000) {
					regmap_write(SYSCFG_IOCTRLSETR_HSLVEN_TRACE |
							SYSCFG_IOCTRLSETR_HSLVEN_QUADSPI |
							SYSCFG_IOCTRLSETR_HSLVEN_ETH |
							SYSCFG_IOCTRLSETR_HSLVEN_SDMMC |
							SYSCFG_IOCTRLSETR_HSLVEN_SPI,
							syscfg + SYSCFG_IOCTRLSETR);

					if (!otp)
						pr_err("product_below_2v5=0: HSLVEN protected by HW\n");
				} else {
					if (otp)
						pr_err("product_below_2v5=1: HSLVEN update is destructive, no update as VDD>2.7V\n");
				}
			} else {
				pr_debug("VDD unknown");
			}
		}
	}
	#endif
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
