// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2017, STMicroelectronics - All Rights Reserved
 * Author(s): Patrice Chotard, <patrice.chotard@st.com> for STMicroelectronics.
 */

#include <common.h>
#include <init.h>
#include <linux/err.h>
#include <linux/reset-controller.h>
#include <mach/stm32mp1_smc.h>
#include <asm/io.h>

#define RCC_APB5RST_BANK 0x62
#define RCC_AHB5RST_BANK 0x64

/* reset clear offset for STM32MP RCC */
#define RCC_CL 0x4

struct stm32mp_reset {
	void __iomem *base;
	struct reset_controller_dev rcdev;
};

static struct stm32mp_reset *to_stm32mp_reset(struct reset_controller_dev *rcdev)
{
	return container_of(rcdev, struct stm32mp_reset, rcdev);
}

static inline bool is_trusted(void)
{
	return false;
}

static bool has_reset_smc(unsigned int bank)
{
	return is_trusted() && (bank == RCC_APB5RST_BANK << 2 ||
				bank == RCC_AHB5RST_BANK << 2);
}

static int stm32mp_reset_assert(struct reset_controller_dev *rcdev,
				unsigned long id)
{
	struct stm32mp_reset *priv = to_stm32mp_reset(rcdev);
	int bank = (id / BITS_PER_LONG) * 4;
	int offset = id % BITS_PER_LONG;

	/* reset assert is done in rcc set register */
	if (has_reset_smc(bank))
		stm32_smc_exec(STM32_SMC_RCC, STM32_SMC_REG_WRITE,
			       bank, BIT(offset));
	else
		writel(BIT(offset), priv->base + bank);

	return 0;
}

static int stm32mp_reset_deassert(struct reset_controller_dev *rcdev,
				 unsigned long id)
{
	struct stm32mp_reset *priv = to_stm32mp_reset(rcdev);
	int bank = (id / BITS_PER_LONG) * 4;
	int offset = id % BITS_PER_LONG;

	/* reset deassert is done in rcc clr register */
	if (has_reset_smc(bank))
		stm32_smc_exec(STM32_SMC_RCC, STM32_SMC_REG_WRITE,
			       bank + RCC_CL, BIT(offset));
	else
		writel(BIT(offset), priv->base + bank + RCC_CL);

	return 0;
}

static const struct reset_control_ops stm32mp_reset_ops = {
	.assert		= stm32mp_reset_assert,
	.deassert	= stm32mp_reset_deassert,
};

static int stm32mp_reset_probe(struct device_d *dev)
{
	struct stm32mp_reset *priv;
	struct resource *iores;

	priv = xzalloc(sizeof(*priv));
	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return PTR_ERR(iores);

	priv->base = IOMEM(iores->start);
	priv->rcdev.nr_resets = (iores->end - iores->start) * BITS_PER_BYTE;
	priv->rcdev.ops = &stm32mp_reset_ops;
	priv->rcdev.of_node = dev->device_node;

	return reset_controller_register(&priv->rcdev);
}

static const struct of_device_id stm32mp_rcc_reset_dt_ids[] = {
	{ .compatible = "st,stm32mp1-rcc" },
	{ /* sentinel */ },
};

static struct driver_d stm32mp_rcc_reset_driver = {
	.name = "stm32mp_rcc_reset",
	.probe = stm32mp_reset_probe,
	.of_compatible = DRV_OF_COMPAT(stm32mp_rcc_reset_dt_ids),
};
device_platform_driver(stm32mp_rcc_reset_driver);
