// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2018, STMicroelectronics - All Rights Reserved
 */

#include <common.h>
#include <init.h>
#include <watchdog.h>
#include <asm/io.h>
#include <of_device.h>
#include <linux/iopoll.h>
#include <mfd/syscon.h>

#include <linux/mfd/stpmic1.h>

/* Watchdog Control Register (WCHDG_CR) */
#define WDT_START		BIT(0)
#define WDT_PING		BIT(1)
#define WDT_START_MASK		BIT(0)
#define WDT_PING_MASK		BIT(1)
#define WDT_STOP		0

#define PMIC_WDT_MIN_TIMEOUT 1
#define PMIC_WDT_MAX_TIMEOUT 256
#define PMIC_WDT_DEFAULT_TIMEOUT 30

struct stpmic1_wdt {
	struct watchdog wdd;
	struct regmap *regmap;
	unsigned int timeout;
};

static inline struct stpmic1_wdt *to_stpmic1_wdt(struct watchdog *wdd)
{
	return container_of(wdd, struct stpmic1_wdt, wdd);
}

static int stpmic1_wdt_ping(struct regmap *regmap)
{
	return regmap_update_bits(regmap, WCHDG_CR, WDT_PING_MASK, WDT_PING);
}

static int stpmic1_wdt_start(struct regmap *regmap, unsigned int timeout)
{
	int ret = regmap_write(regmap, WCHDG_TIMER_CR, timeout - 1);
	if (ret)
		return ret;

	return regmap_update_bits(regmap, WCHDG_CR, WDT_START_MASK, WDT_START);
}

static int stpmic1_wdt_stop(struct regmap *regmap)
{
	return regmap_update_bits(regmap, WCHDG_CR, WDT_START_MASK, WDT_STOP);
}

static int stpmic1_wdt_set_timeout(struct watchdog *wdd, unsigned int timeout)
{
	struct stpmic1_wdt *wdt = to_stpmic1_wdt(wdd);
	int ret;

	if (!timeout)
		return stpmic1_wdt_stop(wdt->regmap);

	if (timeout < PMIC_WDT_MIN_TIMEOUT || timeout > wdd->timeout_max)
		return -EINVAL;

	if (wdt->timeout == timeout)
		return stpmic1_wdt_ping(wdt->regmap);

	ret = stpmic1_wdt_start(wdt->regmap, timeout);
	if (ret)
		return ret;

	wdt->timeout = timeout;
	return 0;
}

static int stpmic1_wdt_probe(struct device_d *dev)
{
	struct stpmic1_wdt *wdt;
	struct watchdog *wdd;

	wdt = xzalloc(sizeof(*wdt));

	wdt->regmap = dev_get_regmap(dev->parent, NULL);
	if (IS_ERR(wdt->regmap))
		return PTR_ERR(wdt->regmap);

	wdd = &wdt->wdd;
	wdd->hwdev = dev;
	wdd->set_timeout = stpmic1_wdt_set_timeout;
	wdd->timeout_max = PMIC_WDT_MAX_TIMEOUT;

	return watchdog_register(wdd);
}

static __maybe_unused const struct of_device_id stpmic1_wdt_of_match[] = {
	{ .compatible = "st,stpmic1-wdt" },
	{ /* sentinel */ }
};

static struct driver_d stpmic1_wdt_driver = {
	.name  = "stpmic1-wdt",
	.probe = stpmic1_wdt_probe,
	.of_compatible = DRV_OF_COMPAT(stpmic1_wdt_of_match),
};
device_platform_driver(stpmic1_wdt_driver);
