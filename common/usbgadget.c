/*
 * Copyright (c) 2017 Oleksij Rempel <o.rempel@pengutronix.de>, Pengutronix
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt) "usbgadget: " fmt

#include <common.h>
#include <command.h>
#include <errno.h>
#include <environment.h>
#include <malloc.h>
#include <getopt.h>
#include <fs.h>
#include <xfuncs.h>
#include <usb/usbserial.h>
#include <usb/dfu.h>
#include <usb/mass_storage.h>
#include <usb/gadget-multi.h>
#include <globalvar.h>
#include <magicvar.h>

static int autostart;
static int acm;
static char *dfu_function;
static char *ums_function;

static struct file_list *parse(const char *files)
{
	struct file_list *list = file_list_parse(files);
	if (IS_ERR(list)) {
		pr_err("Parsing file list \"%s\" failed: %pe\n", files, list);
		return NULL;
	}
	return list;
}

int usbgadget_register(const struct usbgadget_funcs *funcs)
{
	int ret;
	int flags = funcs->flags;
	struct device_d *dev;
	struct f_multi_opts *opts;
	const char *fastboot_partitions = get_fastboot_partitions();
	const char *dfu_opts = NULL, *fastboot_opts = NULL, *ums_opts = NULL;

	if (flags & USBGADGET_DFU) {
		dfu_opts = funcs->dfu_opts;
		if (!dfu_opts && dfu_function && *dfu_function)
			dfu_opts = dfu_function;
	}

	if (flags & USBGADGET_FASTBOOT) {
		fastboot_opts = funcs->fastboot_opts;
		if (IS_ENABLED(CONFIG_FASTBOOT_BASE) && !fastboot_opts &&
		    fastboot_partitions && *fastboot_partitions)
			fastboot_opts = fastboot_partitions;
	}

	if (flags & USBGADGET_MASS_STORAGE) {
		ums_opts = funcs->mass_storage_opts;
		if (!ums_opts && ums_function && *ums_function)
			ums_opts = ums_function;
	}

	if (!dfu_opts && !fastboot_opts && !ums_opts && !(flags & USBGADGET_ACM))
		return COMMAND_ERROR_USAGE;

	/*
	 * Creating a gadget with both DFU and Fastboot doesn't work.
	 * Both client tools seem to assume that the device only has
	 * a single configuration
	 */
	if (fastboot_opts && dfu_opts && ums_opts) {
		pr_err("Only one of Fastboot, DFU and mass storage allowed\n");
		return -EINVAL;
	}

	opts = xzalloc(sizeof(*opts));
	opts->release = usb_multi_opts_release;

	if (fastboot_opts) {
		opts->fastboot_opts.files = parse(fastboot_opts);
		opts->fastboot_opts.export_bbu = flags & USBGADGET_EXPORT_BBU;
	}

	if (dfu_opts)
		opts->dfu_opts.files = parse(dfu_opts);

	if (ums_opts)
		opts->ums_opts.files = parse(ums_opts);

	if (!opts->dfu_opts.files && !opts->fastboot_opts.files &&
	    !opts->ums_opts.files && !(flags & USBGADGET_ACM)) {
		pr_warn("No functions to register\n");
		free(opts);
		return 0;
	}

	opts->create_acm = flags & USBGADGET_ACM;

	dev = get_device_by_name("otg");
	if (dev)
		dev_set_param(dev, "mode", "peripheral");

	ret = usb_multi_register(opts);
	if (ret)
		usb_multi_opts_release(opts);

	return ret;
}

static int usbgadget_autostart_set(struct param_d *param, void *ctx)
{
	struct usbgadget_funcs funcs = {};

	if (!autostart)
		return 0;

	if (get_fastboot_bbu())
		funcs.flags |= USBGADGET_EXPORT_BBU;

	if (acm)
		funcs.flags |= USBGADGET_ACM;

	funcs.flags |= USBGADGET_DFU | USBGADGET_FASTBOOT | USBGADGET_MASS_STORAGE;

	return usbgadget_register(&funcs);
}

static int usbgadget_globalvars_init(void)
{
	if (IS_ENABLED(CONFIG_USB_GADGET_AUTOSTART)) {
		globalvar_add_bool("usbgadget.autostart", usbgadget_autostart_set,
				   &autostart, NULL);
		globalvar_add_simple_bool("usbgadget.acm", &acm);
	}
	globalvar_add_simple_string("usbgadget.dfu_function", &dfu_function);
	globalvar_add_simple_string("usbgadget.mass_storage_function", &ums_function);

	return 0;
}
device_initcall(usbgadget_globalvars_init);

BAREBOX_MAGICVAR(global.usbgadget.autostart,
		 "usbgadget: Automatically start usbgadget on boot");
BAREBOX_MAGICVAR(global.usbgadget.acm,
		 "usbgadget: Create CDC ACM function");
BAREBOX_MAGICVAR(global.usbgadget.dfu_function,
		 "usbgadget: Create DFU function");
BAREBOX_MAGICVAR(global.usbgadget.mass_storage_function,
		 "usbgadget: Create mass storage function");
