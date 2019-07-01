// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2017, STMicroelectronics - All Rights Reserved
 * Author(s): Patrice Chotard, <patrice.chotard@st.com> for STMicroelectronics.
 */

#include <common.h>
#include <init.h>
#include <linux/err.h>
#include <linux/reset-controller.h>
#include <asm/io.h>

struct stm32f_reset {
	void __iomem *base;
	struct reset_controller_dev rcdev;
};

static struct stm32f_reset *to_stm32f_reset(struct reset_controller_dev *rcdev)
{
	return container_of(rcdev, struct stm32f_reset, rcdev);
}

static int stm32f_reset_assert(struct reset_controller_dev *rcdev,
				unsigned long id)
{
	struct stm32f_reset *priv = to_stm32f_reset(rcdev);
	int bank = (id / BITS_PER_LONG) * 4;
	int offset = id % BITS_PER_LONG;

	setbits_le32(priv->base + bank, BIT(offset));

	return 0;
}

static int stm32f_reset_deassert(struct reset_controller_dev *rcdev,
				 unsigned long id)
{
	struct stm32f_reset *priv = to_stm32f_reset(rcdev);
	int bank = (id / BITS_PER_LONG) * 4;
	int offset = id % BITS_PER_LONG;

	clrbits_le32(priv->base + bank, BIT(offset));

	return 0;
}

static const struct reset_control_ops stm32f_reset_ops = {
	.assert		= stm32f_reset_assert,
	.deassert	= stm32f_reset_deassert,
};

static int stm32f_reset_probe(struct device_d *dev)
{
	struct stm32f_reset *priv;
	struct resource *iores;

	priv = xzalloc(sizeof(*priv));
	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return PTR_ERR(iores);

	priv->base = IOMEM(iores->start);
	priv->rcdev.nr_resets = (iores->end - iores->start) * BITS_PER_BYTE;
	priv->rcdev.ops = &stm32f_reset_ops;
	priv->rcdev.of_node = dev->device_node;

	return reset_controller_register(&priv->rcdev);
}

static const struct of_device_id stm32f_rcc_reset_dt_ids[] = {
	{ .compatible = "st,stm32-rcc" },
	{ /* sentinel */ },
};

static struct driver_d stm32f_rcc_reset_driver = {
	.name = "stm32f_rcc_reset",
	.probe = stm32f_reset_probe,
	.of_compatible = DRV_OF_COMPAT(stm32f_rcc_reset_dt_ids),
};
device_platform_driver(stm32f_rcc_reset_driver);
