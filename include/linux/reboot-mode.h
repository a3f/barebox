/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __REBOOT_MODE_H__
#define __REBOOT_MODE_H__

#include <linux/list.h>

struct device_d;

struct reboot_mode_driver {
	struct device_d *dev;
	int (*write)(struct reboot_mode_driver *reboot, unsigned int magic);
	int priority;

	/* filled by reboot_mode_register */
	int reboot_mode;
	unsigned nmodes;
	const char **modes;
	uint32_t *magics;
};

int reboot_mode_register(struct reboot_mode_driver *reboot, u32 reboot_mode);
const char *reboot_mode_get(void);

#define REBOOT_MODE_DEFAULT_PRIORITY 100

#endif
