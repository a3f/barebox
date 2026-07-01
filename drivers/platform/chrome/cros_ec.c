// SPDX-License-Identifier: GPL-2.0-only
// SPDX-Comment: Origin-URL: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/platform/chrome/cros_ec.c?id=56cb557279d70397cefb497e0f06bdd6fd685f8e
/*
 * ChromeOS EC multi-function device
 *
 * Copyright (C) 2012 Google, Inc
 *
 * The ChromeOS EC multi function device is used to mux all the requests
 * to the EC device for its multiple features: keyboard controller,
 * battery charging and regulator control, firmware update.
 */

#include <linux/cleanup.h>
#include <linux/module.h>
#include <of.h>
#include <linux/device.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/slab.h>

#include "cros_ec.h"

struct cros_ec_device *cros_ec_device_alloc(struct device *dev)
{
	struct cros_ec_device *ec_dev;

	ec_dev = devm_kzalloc(dev, sizeof(*ec_dev), GFP_KERNEL);
	if (!ec_dev)
		return NULL;

	ec_dev->din_size = sizeof(struct ec_host_response) +
			   sizeof(struct ec_response_get_protocol_info) +
			   EC_MAX_RESPONSE_OVERHEAD;
	ec_dev->dout_size = sizeof(struct ec_host_request) +
			    sizeof(struct ec_params_rwsig_action) +
			    EC_MAX_REQUEST_OVERHEAD;

	ec_dev->din = devm_kzalloc(dev, ec_dev->din_size, GFP_KERNEL);
	if (!ec_dev->din)
		return NULL;

	ec_dev->dout = devm_kzalloc(dev, ec_dev->dout_size, GFP_KERNEL);
	if (!ec_dev->dout)
		return NULL;

	ec_dev->dev = dev;
	ec_dev->max_response = sizeof(struct ec_response_get_protocol_info);
	ec_dev->max_request = sizeof(struct ec_params_rwsig_action);

	return ec_dev;
}
EXPORT_SYMBOL(cros_ec_device_alloc);

static int cros_ec_register_dev(struct cros_ec_device *ec_dev)
{
	struct device *ec;
	int err;

	ec = device_alloc("cros-ec-dev", DEVICE_ID_DYNAMIC);
	ec->parent = ec_dev->dev;
	ec->driver_data = ec_dev;

	err = platform_device_register(ec);
	if (err) {
		free_device(ec);
		return err;
	}

	ec_dev->ec = ec;

	dev_add_param_uint32_fixed(ec, "proto_version",
				   ec_dev->proto_version, "%u");

	return 0;
}

/**
 * cros_ec_register() - Register a new ChromeOS EC, using the provided info.
 * @ec_dev: Device to register.
 *
 * Before calling this, allocate a pointer to a new device and then fill
 * in all the fields up to the --private-- marker.
 *
 * Return: 0 on success or negative error code.
 */
int cros_ec_register(struct cros_ec_device *ec_dev)
{
	struct device *dev = ec_dev->dev;
	int err;

	/* Send RWSIG continue to jump to RW for devices using RWSIG. */
	err = cros_ec_rwsig_continue(ec_dev);
	if (err)
		dev_info(dev, "Failed to continue RWSIG: %d\n", err);

	err = cros_ec_query_all(ec_dev);
	if (err) {
		dev_err(dev, "Cannot identify the EC: error %d\n", err);
		goto exit;
	}

	err = cros_ec_register_dev(ec_dev);
	if (err) {
		dev_err_probe(dev, err, "Failed to create Chrome EC device\n");
		goto exit;
	}

	dev->driver_data = ec_dev;

	scoped_guard(mutex, &ec_dev->lock)
		ec_dev->registered = true;

	dev_info(dev, "Chrome EC device registered\n");

	/* In a deep-probe system, we are likely to directly probe the devices
	 * when populating them below, so we move that after signalling
	 * registration completion, unlike Linux.
	 */
	if (dev_of_node(dev)) {
		err = of_platform_populate(dev->of_node, NULL, dev);
		if (err) {
			dev_err(dev, "Failed to register sub-devices\n");
			unregister_device(ec_dev->ec);
			ec_dev->ec = NULL;
			ec_dev->registered = false;
			goto exit;
		}
	}

	return 0;
exit:
	return err;
}
EXPORT_SYMBOL(cros_ec_register);

/**
 * cros_ec_unregister() - Remove a ChromeOS EC.
 * @ec_dev: Device to unregister.
 *
 * Call this to deregister a ChromeOS EC, then clean up any private data.
 *
 * Return: 0 on success or negative error code.
 */
void cros_ec_unregister(struct cros_ec_device *ec_dev)
{
	scoped_guard(mutex, &ec_dev->lock)
		ec_dev->registered = false;

	if (ec_dev->ec) {
		unregister_device(ec_dev->ec);
		ec_dev->ec = NULL;
	}
}
EXPORT_SYMBOL(cros_ec_unregister);

struct cros_ec_device *dev_to_cros_ec(struct device *dev)
{
	if (!dev)
		return NULL;
	if (dev->driver_data)
		return dev->driver_data;
	if (dev->parent)
		return dev->parent->driver_data;

	return NULL;
}
EXPORT_SYMBOL_GPL(dev_to_cros_ec);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ChromeOS EC core driver");
