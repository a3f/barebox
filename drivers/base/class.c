// SPDX-License-Identifier: GPL-2.0-only
#include <device.h>
#include <driver.h>
#include <linux/list.h>

LIST_HEAD(class_list);

void class_register(struct class *class)
{
	list_add_tail(&class->list, &class_list);
}

int class_add_device(struct class *class, struct device *dev)
{
	list_add_tail(&dev->class_list, &class->devices);

	return 0;
}

void class_remove_device(struct class *class, struct device *dev)
{
	list_del(&dev->class_list);
}
