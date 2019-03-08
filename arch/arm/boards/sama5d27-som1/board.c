// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Ahmad Fatoum, Pengutronix
 */

#include <init.h>
#include <mach/board.h>
#include <mach/iomux.h>

static int sama5d27_som1_xplained_mem_init(void)
{
	at91_add_device_sdram(0);

	return 0;
}
mem_initcall(sama5d27_som1_xplained_mem_init);

static int sama5d27_som1_xplained_devices_init(void)
{
	return 0;
}
device_initcall(sama5d27_som1_xplained_devices_init);
