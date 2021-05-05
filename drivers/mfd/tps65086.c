// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 Texas Instruments Incorporated - https://www.ti.com/
 *	Andrew F. Davis <afd@ti.com>
 */

#include <common.h>
#include <driver.h>
#include <errno.h>
#include <i2c/i2c.h>
#include <linux/mfd/core.h>
#include <init.h>
#include <malloc.h>
#include <of.h>
#include <regmap.h>
#include <xfuncs.h>

#include <linux/mfd/tps65086.h>

static const struct mfd_cell tps65086_cells[] = {
	{ .name = "tps65086-regulator", },
	{ .name = "tps65086-gpio", },
};

static const struct regmap_config tps65086_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xB6,
};

static const struct of_device_id tps65086_of_match_table[] = {
	{ .compatible = "ti,tps65086", },
	{ /* sentinel */ }
};

static int tps65086_probe(struct device_d *dev)
{
	struct tps65086 *tps;
	unsigned int version;
	int ret;

	tps = xzalloc(sizeof(*tps));
	tps->dev = dev;

	tps->regmap = regmap_init_i2c(to_i2c_client(dev), &tps65086_regmap_config);
	if (IS_ERR(tps->regmap)) {
		dev_err(tps->dev, "Failed to initialize register map\n");
		return PTR_ERR(tps->regmap);
	}

	ret = regmap_read(tps->regmap, TPS65086_DEVICEID, &version);
	if (ret) {
		dev_err(tps->dev, "Failed to read revision register\n");
		return ret;
	}

	dev_info(tps->dev, "Device: TPS65086%01lX, OTP: %c, Rev: %ld\n",
		 (version & TPS65086_DEVICEID_PART_MASK),
		 (char)((version & TPS65086_DEVICEID_OTP_MASK) >> 4) + 'A',
		 (version & TPS65086_DEVICEID_REV_MASK) >> 6);

	dev->priv = tps;

	return mfd_add_devices(tps->dev, tps65086_cells, ARRAY_SIZE(tps65086_cells));
}

static struct driver_d tps65086_driver = {
	.name	= "tps65086",
	.of_compatible = tps65086_of_match_table,
	.probe		= tps65086_probe,
};
device_i2c_driver(tps65086_driver);

MODULE_AUTHOR("Andrew F. Davis <afd@ti.com>");
MODULE_DESCRIPTION("TPS65086 PMIC Driver");
MODULE_LICENSE("GPL v2");
