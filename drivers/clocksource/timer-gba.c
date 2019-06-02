/*
 * Copyright (C) 2019 Ahmad Fatoum <a.fatoum@pengutronix.de>
 *
 */

#include <common.h>
#include <init.h>
#include <clock.h>
#include <linux/clk.h>
#include <io.h>
#include <asm/system.h>

#define GBA_CLOCKFREQ (1 << 24) /* 16.78Mhz */

#define CLOCKSOURCE_TIMER 3
#define TIMER_REGS_SIZE 4

#define TMCNT_START		(1 << 23)
#define TMCNT_PRESCALER_1024	(3 << 16)

static void __iomem *timer_base;

static uint64_t gba_clocksource_read(void)
{
	return readw(timer_base);
}

static struct clocksource cs = {
	.read	= gba_clocksource_read,
	.mask	= CLOCKSOURCE_MASK(16),
	.shift	= 0,
	.mult	= 61035, /* when prescaler = 1024 */
};

static int gba_timer_probe(struct device_d *dev)
{
	struct resource *iores;

	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return PTR_ERR(iores);

	timer_base = IOMEM(iores->start) + CLOCKSOURCE_TIMER * TIMER_REGS_SIZE;
	writel(TMCNT_PRESCALER_1024 | TMCNT_START, timer_base);

	return init_clock(&cs);
}

static struct of_device_id gba_timer_dt_ids[] = {
	{ .compatible = "nintendo,gba-timer", },
	{ /* sentinel */ }
};

static struct driver_d gba_timer_driver = {
	.name = "gba_timer",
	.probe = gba_timer_probe,
	.of_compatible = DRV_OF_COMPAT(gba_timer_dt_ids),
};
postcore_platform_driver(gba_timer_driver);
