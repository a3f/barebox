/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-Comment: Origin-URL: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/platform/chrome/cros_ec.h?id=918856986014142271a70a334d300994b9c41720 */
/*
 * ChromeOS Embedded Controller core interface.
 *
 * Copyright (C) 2020 Google LLC
 */

#ifndef __CROS_EC_H
#define __CROS_EC_H

struct cros_ec_device;
struct device;

struct cros_ec_device *cros_ec_device_alloc(struct device *dev);

int cros_ec_register(struct cros_ec_device *ec_dev);
void cros_ec_unregister(struct cros_ec_device *ec_dev);

#endif /* __CROS_EC_H */
