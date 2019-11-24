// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2016, Fuzhou Rockchip Electronics Co., Ltd
 * Copyright (c) 2019, Ahmad Fatoum, Pengutronix
 */

#include <common.h>
#include <driver.h>
#include <init.h>
#include <of.h>
#include <linux/reboot-mode.h>
#include <globalvar.h>

#define PREFIX "mode-"

static int __priority;
static struct reboot_mode_driver *__boot_mode;

static int reboot_mode_param_set(struct param_d *p, void *priv)
{
	struct reboot_mode_driver *reboot = priv;
	u32 magic;

	magic = reboot->magics[reboot->reboot_mode];

	return reboot->write(reboot, magic);
}

static int reboot_mode_add_globalvar(void)
{
	struct reboot_mode_driver *reboot = __boot_mode;

	if (!reboot)
		return 0;

	return globalvar_add_enum("reboot_mode",
				  reboot_mode_param_set, NULL,
				  &reboot->reboot_mode, reboot->modes,
				  reboot->nmodes, reboot);
}
late_initcall(reboot_mode_add_globalvar);

static void reboot_mode_print(struct reboot_mode_driver *reboot,
			      const char *prefix, const u32 magic)
{
	dev_dbg(reboot->dev, "%s: %08x\n", prefix, magic);
}
/**
 * reboot_mode_register - register a reboot mode driver
 * @reboot: reboot mode driver
 * @reboot_mode: reboot mode read from hardware
 *
 * Returns: 0 on success or a negative error code on failure.
 */
int reboot_mode_register(struct reboot_mode_driver *reboot, u32 reboot_mode)
{
	struct property *prop;
	struct device_node *np = reboot->dev->device_node;
	size_t len = strlen(PREFIX);
	size_t nmodes = 0;
	int i = 0;
	int ret;

	for_each_property_of_node(np, prop) {
		u32 magic;

		if (strncmp(prop->name, PREFIX, len))
			continue;
		if (of_property_read_u32(np, prop->name, &magic))
			continue;

		nmodes++;
	}

	reboot->nmodes = nmodes;
	reboot->magics = xzalloc(nmodes * sizeof(u32));
	reboot->modes = xzalloc(nmodes * sizeof(const char *));

	reboot_mode_print(reboot, "registering magic", reboot_mode);

	for_each_property_of_node(np, prop) {
		const char **mode;
		u32 *magic;

		magic = &reboot->magics[i];
		mode = &reboot->modes[i];

		if (strncmp(prop->name, PREFIX, len))
			continue;

		if (of_property_read_u32(np, prop->name, magic)) {
			dev_err(reboot->dev, "reboot mode %s without magic number\n",
				*mode);
			continue;
		}

		*mode = prop->name + len;
		if (*mode[0] == '\0') {
			ret = -EINVAL;
			dev_err(reboot->dev, "invalid mode name(%s): too short!\n",
				prop->name);
			goto error;
		}

		reboot_mode_print(reboot, *mode, *magic);

		i++;
	}

	for (i = 0; i < reboot->nmodes; i++) {
		if (reboot->magics[i] == reboot_mode) {
			reboot->reboot_mode = i;
			break;
		}
	}

	dev_add_param_enum(reboot->dev, "reboot_mode",
			   reboot_mode_param_set, NULL,
			   &reboot->reboot_mode, reboot->modes,
			   reboot->nmodes, reboot);

	/* clear mode for next reboot */
	reboot->write(reboot, 0);

	if (!reboot->priority)
		reboot->priority = REBOOT_MODE_DEFAULT_PRIORITY;

	if (reboot->priority >= __priority) {
		__priority = reboot->priority;
		__boot_mode = reboot;
	}

	return 0;

error:
	free(reboot->magics);
	free(reboot->modes);

	return ret;
}
EXPORT_SYMBOL_GPL(reboot_mode_register);

const char *reboot_mode_get(void)
{
	if (!__boot_mode)
		return NULL;

	return __boot_mode->modes[__boot_mode->reboot_mode];
}
EXPORT_SYMBOL_GPL(reboot_mode_get);
