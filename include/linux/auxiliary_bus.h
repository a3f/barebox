/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020 Intel Corporation
 *
 * Ported from Linux to barebox.
 */

#ifndef _AUXILIARY_BUS_H_
#define _AUXILIARY_BUS_H_

#include <linux/device.h>
#include <linux/mod_devicetable.h>

struct auxiliary_device {
	struct device dev;
	const char *name;
	u32 id;
};

struct auxiliary_driver {
	int (*probe)(struct auxiliary_device *auxdev, const struct auxiliary_device_id *id);
	void (*remove)(struct auxiliary_device *auxdev);
	const char *name;
	struct driver driver;
	const struct auxiliary_device_id *id_table;
};

static inline void *auxiliary_get_drvdata(struct auxiliary_device *auxdev)
{
	return dev_get_drvdata(&auxdev->dev);
}

static inline void auxiliary_set_drvdata(struct auxiliary_device *auxdev, void *data)
{
	dev_set_drvdata(&auxdev->dev, data);
}

static inline struct auxiliary_device *to_auxiliary_dev(struct device *dev)
{
	return container_of(dev, struct auxiliary_device, dev);
}

static inline const struct auxiliary_driver *to_auxiliary_drv(const struct driver *drv)
{
	return container_of(drv, struct auxiliary_driver, driver);
}

int auxiliary_device_init(struct auxiliary_device *auxdev);
int __auxiliary_device_add(struct auxiliary_device *auxdev, const char *modname);
#define auxiliary_device_add(auxdev) __auxiliary_device_add(auxdev, KBUILD_MODNAME)

static inline void auxiliary_device_uninit(struct auxiliary_device *auxdev)
{
	put_device(&auxdev->dev);
}

static inline void auxiliary_device_delete(struct auxiliary_device *auxdev)
{
	unregister_device(&auxdev->dev);
}

int __auxiliary_driver_register(struct auxiliary_driver *auxdrv,
				const char *modname);
#define auxiliary_driver_register(auxdrv) \
	__auxiliary_driver_register(auxdrv, KBUILD_MODNAME)

void auxiliary_driver_unregister(struct auxiliary_driver *auxdrv);

struct auxiliary_device *auxiliary_device_create(struct device *dev,
						 const char *modname,
						 const char *devname,
						 void *platform_data,
						 int id);
void auxiliary_device_destroy(void *auxdev);

#define module_auxiliary_driver(__auxiliary_driver) \
	module_driver(__auxiliary_driver, auxiliary_driver_register, auxiliary_driver_unregister)

#endif /* _AUXILIARY_BUS_H_ */
