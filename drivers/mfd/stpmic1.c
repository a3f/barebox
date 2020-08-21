// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Ahmad Fatoum, Pengutronix
 */

#include <common.h>
#include <driver.h>
#include <errno.h>
#include <i2c/i2c.h>
#include <init.h>
#include <malloc.h>
#include <of.h>
#include <regmap.h>
#include <xfuncs.h>
#include <restart.h>
#include <reset_source.h>
#include <linux/mfd/stpmic1.h>
#include <poweroff.h>

/* Restart Status Register (RREQ_STATE_SR) */
#define R_RST			BIT(0)
#define R_SWOFF			BIT(1)
#define R_WDG			BIT(2)
#define R_PKEYLKP		BIT(3)
#define R_VINOK_FA		BIT(4)

struct stpmic1_reset_reason {
	uint32_t mask;
	enum reset_src_type type;
	int instance;
};

struct stpmic1 {
	struct device_d		*dev;
	struct i2c_client	*client;
	struct restart_handler	restart;
	struct poweroff_handler	poweroff;
	struct regmap		*regmap;
};

static int stpmic1_i2c_reg_read(void *ctx, unsigned int reg, unsigned int *val)
{
	struct stpmic1 *stpmic1 = ctx;
	u8 buf[1];
	int ret;

	ret = i2c_read_reg(stpmic1->client, reg, buf, 1);
	*val = buf[0];

	return ret == 1 ? 0 : ret;
}

static int stpmic1_i2c_reg_write(void *ctx, unsigned int reg, unsigned int val)
{
	struct stpmic1 *stpmic1 = ctx;
	u8 buf[] = {
		val & 0xff,
	};
	int ret;

	ret = i2c_write_reg(stpmic1->client, reg, buf, 1);

	return ret == 1 ? 0 : ret;
}

static struct regmap_bus regmap_stpmic1_i2c_bus = {
	.reg_write = stpmic1_i2c_reg_write,
	.reg_read = stpmic1_i2c_reg_read,
};

static const struct regmap_config stpmic1_regmap_i2c_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xB3,
};

static void __noreturn stpmic1_restart_handler(struct restart_handler *rst)
{
	struct stpmic1 *stpmic1 = container_of(rst, struct stpmic1, restart);

	regmap_write_bits(stpmic1->regmap, SWOFF_PWRCTRL_CR,
			  SOFTWARE_SWITCH_OFF_ENABLED | RESTART_REQUEST_ENABLED,
			  SOFTWARE_SWITCH_OFF_ENABLED | RESTART_REQUEST_ENABLED);

	mdelay(1000);
	hang();
}

static void __noreturn stpmic1_poweroff(struct poweroff_handler *handler)
{
	struct stpmic1 *stpmic1 = container_of(handler, struct stpmic1, poweroff);

	shutdown_barebox();

	regmap_write_bits(stpmic1->regmap, SWOFF_PWRCTRL_CR,
			  SOFTWARE_SWITCH_OFF_ENABLED | RESTART_REQUEST_ENABLED,
			  SOFTWARE_SWITCH_OFF_ENABLED);

	mdelay(1000);
	hang();
}

static const struct stpmic1_reset_reason stpmic1_reset_reasons[] = {
	{ R_VINOK_FA,	RESET_BROWNOUT, 0 },
	{ R_PKEYLKP,	RESET_EXT, 0 },
	{ R_WDG,	RESET_WDG, 2 },
	{ R_SWOFF,	RESET_RST, 0 },
	{ R_RST,	RESET_EXT, 0 },
	{ /* sentinel */ }
};

static int stpmic1_set_reset_reason(struct regmap *map)
{
	enum reset_src_type type = RESET_POR;
	u32 reg;
	int ret;
	int i, instance = 0;

	ret = regmap_read(map, RREQ_STATE_SR, &reg);
	if (ret)
		return ret;

	for (i = 0; stpmic1_reset_reasons[i].mask; i++) {
		if (reg & stpmic1_reset_reasons[i].mask) {
			type     = stpmic1_reset_reasons[i].type;
			instance = stpmic1_reset_reasons[i].instance;
			break;
		}
	}

	reset_source_set_prinst(type, 400, instance);

	pr_info("STPMIC1 reset reason %s (RREQ_STATE_SR: 0x%08x)\n",
		reset_source_to_string(type), reg);

	return 0;
}

static int __init stpmic1_probe(struct device_d *dev)
{
	struct stpmic1 *stpmic1;
	u32 reg;
	int ret;

	stpmic1 = xzalloc(sizeof(*stpmic1));
	stpmic1->dev = dev;

	stpmic1->client = to_i2c_client(dev);
	stpmic1->regmap = regmap_init(dev, &regmap_stpmic1_i2c_bus,
				      stpmic1, &stpmic1_regmap_i2c_config);

	ret = regmap_register_cdev(stpmic1->regmap, NULL);
	if (ret)
		return ret;

	/* have the watchdog reset, not power-off the system */
	regmap_write_bits(stpmic1->regmap, SWOFF_PWRCTRL_CR,
			  RESTART_REQUEST_ENABLED, RESTART_REQUEST_ENABLED);

	stpmic1->restart.name = "stpmic1-reset";
	stpmic1->restart.restart = stpmic1_restart_handler;
	stpmic1->restart.priority = 300;

	ret = restart_handler_register(&stpmic1->restart);
	if (ret)
		dev_warn(dev, "Cannot register restart handler\n");

	stpmic1->poweroff.name = "stpmic1-poweroff";
	stpmic1->poweroff.poweroff = stpmic1_poweroff;
	stpmic1->poweroff.priority = 200;

	ret = poweroff_handler_register(&stpmic1->poweroff);
	if (ret)
		dev_warn(dev, "Cannot register poweroff handler\n");

	ret = stpmic1_set_reset_reason(stpmic1->regmap);
	if (ret)
		dev_warn(dev, "Cannot query reset reason\n");

	ret = regmap_read(stpmic1->regmap, VERSION_SR, &reg);
	if (ret) {
		dev_err(dev, "Unable to read PMIC version\n");
		return ret;
	}
	dev_info(dev, "PMIC Chip Version: 0x%x\n", reg);

	return of_platform_populate(dev->device_node, NULL, dev);
}

static __maybe_unused struct of_device_id stpmic1_dt_ids[] = {
	{ .compatible = "st,stpmic1" },
	{ /* sentinel */ }
};

static struct driver_d stpmic1_i2c_driver = {
	.name		= "stpmic1-i2c",
	.probe		= stpmic1_probe,
	.of_compatible	= DRV_OF_COMPAT(stpmic1_dt_ids),
};

coredevice_i2c_driver(stpmic1_i2c_driver);
