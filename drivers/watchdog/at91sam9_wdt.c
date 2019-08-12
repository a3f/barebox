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
	struct watchdog wdd;
	void __iomem *base;
	struct device_d *dev;
	unsigned int timeout;
};

static inline void at91sam9x_wdt_start(struct at91sam9x_wdt *wdt,
				       unsigned int timeout)
{
	u32 val = AT91_WDT_WDRSTEN
		| AT91_WDT_WDDBGHLT
		| AT91_WDT_WDD
		| (SECS_TO_WDOG_TICKS(timeout) & AT91_WDT_WDV);

	writel(val, wdt->base + AT91_WDT_MR);
}

static inline void at91sam9x_wdt_ping(struct at91sam9x_wdt *wdt)
{
	writel(AT91_WDT_WDRSTT | AT91_WDT_KEY, wdt->base + AT91_WDT_CR);
}

static int at91sam9x_wdt_set_timeout(struct watchdog *wdd, unsigned timeout)
{
	struct at91sam9x_wdt *wdt = container_of(wdd, struct at91sam9x_wdt, wdd);

	if (!timeout) {
		/* FIXME: you can only disable it once... Think about this
		 * along with what ladislav wrote on the mailing list
		 */
		at91_wdt_disable(wdt->base);
		return 0;
	}

	if (wdt->timeout != timeout) {
		at91sam9x_wdt_start(wdt, timeout);
		wdt->timeout = timeout;
	}

	at91sam9x_wdt_ping(wdt);
	return 0;
}

static int at91sam9x_wdt_probe(struct device_d *dev)
{
	struct at91sam9x_wdt *wdt;
	struct resource *iores;
	struct clk *clk;
	int ret;

	wdt = xzalloc(sizeof(*wdt));
	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores)) {
		dev_err(dev, "could not get watchdog memory region\n");
		return PTR_ERR(iores);
	}
	wdt->base = IOMEM(iores->start);
	clk = clk_get(dev, NULL);
	if (WARN_ON(IS_ERR(clk)))
		return PTR_ERR(clk);

	clk_enable(clk);

	wdt->wdd.set_timeout = at91sam9x_wdt_set_timeout;
	wdt->wdd.hwdev = dev;
	wdt->dev = dev;

	ret = watchdog_register(&wdt->wdd);
	if (ret)
		free(wdt);

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
