// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Ahmad Fatoum, Pengutronix
 */

#include <common.h>
#include <envfs.h>
#include <linux/phy.h>
#include <fs.h>
#include <init.h>
#include <linux/micrel_phy.h>

static int sama5d27_som1_devices_init(void)
{
	if (!of_machine_is_compatible("atmel,sama5d27-som1-ek"))
		return 0;

	return 0;
}
coredevice_initcall(sama5d27_som1_devices_init);
