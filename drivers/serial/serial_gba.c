// SPDX-License-Identifer: GPL-2.0
/*
 * Copyright (C) 2019 Ahmad Fatoum <a.fatoum@pengutronix.de>
 */

#include <common.h>
#include <init.h>
#include <malloc.h>
#include <io.h>

#define VISUALBOY_HACK_OFFSET 0xC

static int gba_serial_setbaudrate(struct console_device *cdev, int baudrate)
{
	dev_warn(cdev->dev, "setbaudrate not implemented\n");

	return 0;
}

static void gba_serial_putc(struct console_device *cdev, char c)
{
	void __iomem  *sio = IOMEM(cdev->dev->priv);
	writew(c, sio + VISUALBOY_HACK_OFFSET);
}

static int gba_serial_getc(struct console_device *cdev)
{
	dev_err(cdev->dev, "getc not implemented\n");

	return -EIO;
}

static int gba_serial_tstc(struct console_device *cdev)
{
	return 0;
}

static int gba_serial_probe(struct device_d *dev)
{
	struct resource *iores;
	struct console_device *cdev;

	cdev = xzalloc(sizeof(struct console_device));
	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return PTR_ERR(iores);
	dev->priv = IOMEM(iores->start);
	cdev->dev = dev;
	cdev->tstc = gba_serial_tstc;
	cdev->putc = gba_serial_putc;
	cdev->getc = gba_serial_getc;
	cdev->setbrg = gba_serial_setbaudrate;

	console_register(cdev);

	return 0;
}

static struct of_device_id gba_serial_dt_ids[] = {
	{ .compatible = "nintendo,gba-serial" },
};

static struct driver_d gba_serial_driver = {
	.name   = "gba-serial",
	.probe  = gba_serial_probe,
	.of_compatible = DRV_OF_COMPAT(gba_serial_dt_ids),
};
console_platform_driver(gba_serial_driver);
