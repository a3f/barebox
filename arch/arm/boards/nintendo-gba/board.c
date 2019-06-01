// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Ahmad Fatoum <a.fatoum@pengutronix.de>
 */

#include <bootsource.h>
#include <common.h>
#include <init.h>

static int gba_device_init(void)
{
	if (!of_machine_is_compatible("nintendo,gameboy-advance"))
		return 0;

	barebox_set_hostname("gba");

	return 0;
}
device_initcall(gba_device_init);
