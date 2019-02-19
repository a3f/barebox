// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2014 Bo Shen <voice.shen@gmail.com>
 * Copyright (C) 2019 Ahmad Fatoum <a.fatoum@pengutronix.de>
 */

#include <init.h>
#include <mach/board.h>
#include <mach/iomux.h>

static int evb_mem_init(void)
{
	at91_add_device_sdram(0);

	return 0;
}
mem_initcall(evb_mem_init);

static struct atmel_mci_platform_data mci0_data = {
	.bus_width	= 8,
	.detect_pin	= AT91_PIN_PE0,
	.wp_pin		= -EINVAL,
};

static int evb_devices_init(void)
{
	if (IS_ENABLED(CONFIG_MCI_ATMEL))
		at91_add_device_mci(0, &mci0_data);

	return 0;
}
device_initcall(evb_devices_init);
