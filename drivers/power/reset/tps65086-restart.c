
// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Emil Renner Berthing
 */

#include <common.h>
#include <init.h>
#include <regmap.h>
#include <restart.h>

#include <linux/mfd/tps65086.h>

struct tps65086_restart {
	struct restart_handler handler;
	struct tps65086 *tps;
};

static void __noreturn tps65086_restart_handle(struct restart_handler *this)
{
	struct tps65086_restart *tps65086_restart =
		container_of(this, struct tps65086_restart, handler);
	int ret;

	ret = regmap_write(tps65086_restart->tps->regmap, TPS65086_FORCESHUTDN, 1);
	WARN_ON(ret);

	/* give it a little time */
	mdelay(200);

	panic("Unable to restart system\n");
}

static int tps65086_restart_probe(struct device_d *dev)
{
	struct tps65086_restart *tps65086_restart;

	tps65086_restart = xzalloc(sizeof(*tps65086_restart));
	tps65086_restart->tps = dev->parent->priv;

	tps65086_restart->handler.name = "tps65086-restart";
	tps65086_restart->handler.restart = tps65086_restart_handle;
	tps65086_restart->handler.priority = 192;

	return restart_handler_register(&tps65086_restart->handler);
}

static struct driver_d tps65086_restart_driver = {
	.name = "tps65086-restart",
	.probe = tps65086_restart_probe,
};
device_platform_driver(tps65086_restart_driver);

MODULE_AUTHOR("Emil Renner Berthing <kernel@esmil.dk>");
MODULE_DESCRIPTION("TPS65086 restart driver");
