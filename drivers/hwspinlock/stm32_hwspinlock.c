// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) STMicroelectronics SA 2018
 * Author: Benjamin Gaignard <benjamin.gaignard@st.com> for STMicroelectronics.
 */

#include <common.h>
#include <init.h>
#include <driver.h>
#include <linux/clk.h>
#include <clock.h>
#include <hwspinlock.h>
#include <io.h>
#include <of.h>
#include "hwspinlock_internal.h"

#define STM32_MUTEX_COREID	BIT(8)
#define STM32_MUTEX_LOCK_BIT	BIT(31)
#define STM32_MUTEX_NUM_LOCKS	32

static bool stm32_hwspinlock_trylock(struct hwspinlock *lock)
{
	void __iomem *lock_addr = lock->priv;
	u32 status;

	writel(STM32_MUTEX_LOCK_BIT | STM32_MUTEX_COREID, lock_addr);
	status = readl(lock_addr);

	return status == (STM32_MUTEX_LOCK_BIT | STM32_MUTEX_COREID);
}

static void stm32_hwspinlock_unlock(struct hwspinlock *lock)
{
	void __iomem *lock_addr = lock->priv;

	writel(STM32_MUTEX_COREID, lock_addr);
}

static void stm32_hwspinlock_relax(struct hwspinlock *lock)
{
	ndelay(50);
}

static const struct hwspinlock_ops stm32_hwspinlock_ops = {
	.trylock	= stm32_hwspinlock_trylock,
	.unlock		= stm32_hwspinlock_unlock,
	.relax		= stm32_hwspinlock_relax,
};

static int stm32_hwspinlock_probe(struct device_d *dev)
{
	struct hwspinlock_device *bank;
	struct resource *iores;
	void __iomem *io_base;
	size_t array_size;
	struct clk *clk;
	int i, ret;

	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return PTR_ERR(iores);
	io_base = IOMEM(iores->start);

	array_size = STM32_MUTEX_NUM_LOCKS * sizeof(struct hwspinlock);
	bank = xzalloc(sizeof(*bank) + array_size);

	clk = clk_get(dev, "hsem");
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	for (i = 0; i < STM32_MUTEX_NUM_LOCKS; i++)
		bank->lock[i].priv = io_base + i * sizeof(u32);

	ret = hwspin_lock_register(bank, dev, &stm32_hwspinlock_ops,
				   0, STM32_MUTEX_NUM_LOCKS);
	if (ret)
		return ret;

	clk_enable(clk);

	return 0;
}

static const struct of_device_id __maybe_unused stm32_hwpinlock_ids[] = {
	{ .compatible = "st,stm32-hwspinlock", },
	{ /* sentinel */ },
};

static struct driver_d stm32_hwspinlock_driver = {
	.name		= "stm32_hwspinlock",
	.probe		= stm32_hwspinlock_probe,
	.of_compatible	= DRV_OF_COMPAT(stm32_hwpinlock_ids),
};

static int __init stm32_hwspinlock_init(void)
{
	return platform_driver_register(&stm32_hwspinlock_driver);
}
/* board init code might need to reserve hwspinlocks for predefined purposes */
core_initcall(stm32_hwspinlock_init);
