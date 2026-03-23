// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020 Intel Corporation
 *
 * Ported from Linux to barebox.
 */

#define pr_fmt(fmt) "auxiliary: " fmt

#include <common.h>
#include <driver.h>
#include <init.h>
#include <linux/slab.h>
#include <linux/sprintf.h>
#include <linux/string.h>
#include <linux/auxiliary_bus.h>
#include <pm_domain.h>

static const struct auxiliary_device_id *auxiliary_match_id(const struct auxiliary_device_id *id,
							    const struct auxiliary_device *auxdev)
{
	const char *auxdev_name = dev_name(&auxdev->dev);
	const char *p = strrchr(auxdev_name, '.');
	int match_size;

	if (!p)
		return NULL;
	match_size = p - auxdev_name;

	for (; id->name[0]; id++) {
		/* use dev_name(&auxdev->dev) prefix before last '.' char to match to */
		if (strlen(id->name) == match_size &&
		    !strncmp(auxdev_name, id->name, match_size))
			return id;
	}
	return NULL;
}

static int auxiliary_match(struct device *dev, const struct driver *drv)
{
	struct auxiliary_device *auxdev = to_auxiliary_dev(dev);
	const struct auxiliary_driver *auxdrv = to_auxiliary_drv(drv);

	return !!auxiliary_match_id(auxdrv->id_table, auxdev);
}

static int auxiliary_bus_probe(struct device *dev)
{
	const struct auxiliary_driver *auxdrv = to_auxiliary_drv(dev->driver);
	struct auxiliary_device *auxdev = to_auxiliary_dev(dev);
	int ret;

	ret = genpd_dev_pm_attach(dev);
	if (ret < 0)
		return dev_err_probe(dev, ret, "power domain attach failed\n");

	return auxdrv->probe(auxdev, auxiliary_match_id(auxdrv->id_table, auxdev));
}

static void auxiliary_bus_remove(struct device *dev)
{
	const struct auxiliary_driver *auxdrv = to_auxiliary_drv(dev->driver);
	struct auxiliary_device *auxdev = to_auxiliary_dev(dev);

	if (auxdrv->remove)
		auxdrv->remove(auxdev);
}

static struct bus_type auxiliary_bus_type = {
	.name = "auxiliary",
	.probe = auxiliary_bus_probe,
	.remove = auxiliary_bus_remove,
	.match = auxiliary_match,
};

/**
 * auxiliary_device_init - check auxiliary_device and initialize
 * @auxdev: auxiliary device struct
 */
int auxiliary_device_init(struct auxiliary_device *auxdev)
{
	struct device *dev = &auxdev->dev;

	if (!dev->parent) {
		pr_err("auxiliary_device has a NULL dev->parent\n");
		return -EINVAL;
	}

	if (!auxdev->name) {
		pr_err("auxiliary_device has a NULL name\n");
		return -EINVAL;
	}

	dev->bus = &auxiliary_bus_type;
	return 0;
}
EXPORT_SYMBOL_GPL(auxiliary_device_init);

/**
 * __auxiliary_device_add - add an auxiliary bus device
 * @auxdev: auxiliary bus device to add to the bus
 * @modname: name of the parent device's driver module
 */
int __auxiliary_device_add(struct auxiliary_device *auxdev, const char *modname)
{
	struct device *dev = &auxdev->dev;
	int ret;

	if (!modname) {
		dev_err(dev, "auxiliary device modname is NULL\n");
		return -EINVAL;
	}

	dev_set_name(dev, "%s.%s.%d", modname, auxdev->name, auxdev->id);
	dev->id = DEVICE_ID_SINGLE;

	ret = register_device(dev);
	if (ret)
		dev_err(dev, "adding auxiliary device failed!: %d\n", ret);

	return ret;
}
EXPORT_SYMBOL_GPL(__auxiliary_device_add);

/**
 * __auxiliary_driver_register - register a driver for auxiliary bus devices
 * @auxdrv: auxiliary_driver structure
 * @modname: KBUILD_MODNAME for parent driver
 */
int __auxiliary_driver_register(struct auxiliary_driver *auxdrv,
				const char *modname)
{
	if (WARN_ON(!auxdrv->probe) || WARN_ON(!auxdrv->id_table))
		return -EINVAL;

	if (auxdrv->name)
		auxdrv->driver.name = basprintf("%s.%s", modname, auxdrv->name);
	else
		auxdrv->driver.name = basprintf("%s", modname);

	auxdrv->driver.bus = &auxiliary_bus_type;

	return register_driver(&auxdrv->driver);
}
EXPORT_SYMBOL_GPL(__auxiliary_driver_register);

/**
 * auxiliary_driver_unregister - unregister a driver
 * @auxdrv: auxiliary_driver structure
 */
void auxiliary_driver_unregister(struct auxiliary_driver *auxdrv)
{
	unregister_driver(&auxdrv->driver);
}
EXPORT_SYMBOL_GPL(auxiliary_driver_unregister);

/**
 * auxiliary_device_create - create a device on the auxiliary bus
 * @dev: parent device
 * @modname: module name used to create the auxiliary driver name.
 * @devname: auxiliary bus device name
 * @platform_data: auxiliary bus device platform data
 * @id: auxiliary bus device id
 */
struct auxiliary_device *auxiliary_device_create(struct device *dev,
						 const char *modname,
						 const char *devname,
						 void *platform_data,
						 int id)
{
	struct auxiliary_device *auxdev;
	int ret;

	auxdev = kzalloc(sizeof(*auxdev), GFP_KERNEL);
	if (!auxdev)
		return NULL;

	auxdev->id = id;
	auxdev->name = devname;
	auxdev->dev.parent = dev;
	auxdev->dev.platform_data = platform_data;
	auxdev->dev.of_node = dev->of_node;

	ret = auxiliary_device_init(auxdev);
	if (ret) {
		kfree(auxdev);
		return NULL;
	}

	ret = __auxiliary_device_add(auxdev, modname);
	if (ret) {
		kfree(auxdev);
		return NULL;
	}

	return auxdev;
}
EXPORT_SYMBOL_GPL(auxiliary_device_create);

/**
 * auxiliary_device_destroy - remove an auxiliary device
 * @auxdev: pointer to the auxdev to be removed
 */
void auxiliary_device_destroy(void *auxdev)
{
	struct auxiliary_device *_auxdev = auxdev;

	auxiliary_device_delete(_auxdev);
}
EXPORT_SYMBOL_GPL(auxiliary_device_destroy);

static int auxiliary_bus_init(void)
{
	return bus_register(&auxiliary_bus_type);
}
pure_initcall(auxiliary_bus_init);
