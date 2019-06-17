// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Pengutronix, Ahmad Fatoum <a.fatoum@pengutronix.de>
 */

#include <common.h>
#include <init.h>
#include <io.h>
#include <watchdog.h>
#include <linux/clk.h>
#include <mach/at91_wdt.h>

#define MIN_WDT_TIMEOUT		1
#define MAX_WDT_TIMEOUT		16
#define WDT_DEFAULT_TIMEOUT	MAX_WDT_TIMEOUT
#define SECS_TO_WDOG_TICKS(s)	((s) ? (((s) << 8) - 1) : 0)

struct at91sam9x_wdt {
	struct watchdog wd;
	void __iomem *base;
	struct device_d *dev;
};

// FIXME required?
static void at91sam9x_wdt_ping(struct watchdog *wd)
{
	struct at91sam9x_wdt *priv = container_of(wd, struct at91sam9x_wdt, wd);
	writel(AT91_WDT_WDRSTT | AT91_WDT_KEY, priv->base + AT91_WDT_CR);
}

static int at91sam9x_wdt_set_timeout(struct watchdog *wd, unsigned timeout)
{
	struct at91sam9x_wdt *priv = container_of(wd, struct at91sam9x_wdt, wd);
	u32 val;

	if (!timeout) {
		at91_wdt_disable(priv->base);
		return 0;
	}

	val = AT91_WDT_WDRSTEN
		| AT91_WDT_WDDBGHLT
		| AT91_WDT_WDD
		| (SECS_TO_WDOG_TICKS(timeout) & AT91_WDT_WDV);

	writel(val, priv->base + AT91_WDT_MR);

	return 0;
}

static int at91sam9x_wdt_probe(struct device_d *dev)
{
	struct at91sam9x_wdt *priv;
	struct resource *iores;
	struct clk *clk;
	int ret;

	priv = xzalloc(sizeof(*priv));
	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores)) {
		dev_err(dev, "could not get watchdog memory region\n");
		return PTR_ERR(iores);
	}
	priv->base = IOMEM(iores->start);
	clk = clk_get(dev, NULL);
	if (WARN_ON(IS_ERR(clk)))
		return PTR_ERR(clk);

	clk_enable(clk);

	priv->wd.set_timeout = at91sam9x_wdt_set_timeout;
	priv->wd.hwdev = dev;
	priv->dev = dev;

	ret = watchdog_register(&priv->wd);
	if (ret)
		free(priv);

	return ret;
}

static const __maybe_unused struct of_device_id at91sam9x_wdt_dt_ids[] = {
	{ .compatible = "atmel,at91sam9260-wdt", },
	{ .compatible = "atmel,sama5d4-wdt", },
	{ /* sentinel */ },
};

static struct driver_d at91sam9x_wdt_driver = {
	.name		= "at91sam9x-wdt",
	.of_compatible	= DRV_OF_COMPAT(at91sam9x_wdt_dt_ids),
	.probe		= at91sam9x_wdt_probe,
};

static int __init at91sam9x_wdt_init(void)
{
	return platform_driver_register(&at91sam9x_wdt_driver);
}
device_initcall(at91sam9x_wdt_init);
