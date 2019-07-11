// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017 Masahiro Yamada <yamada.masahiro@socionext.com>
 * Copyright (C) 2019 Ahmad Fatoum, Pengutronix
 */

#include <common.h>
#include <init.h>
#include <driver.h>
#include <asm/psci.h>
#include <poweroff.h>
#include <restart.h>

static struct restart_handler restart;

static void __noreturn psci_invoke_noreturn(int function)
{
	psci_invoke(function, 0, 0, 0);

	mdelay(1000);
	hang();
}

static void __noreturn psci_poweroff(struct poweroff_handler *handler)
{
	psci_invoke_noreturn(ARM_PSCI_0_2_FN_SYSTEM_OFF);
}

static void __noreturn psci_restart(struct restart_handler *rst)
{
	psci_invoke_noreturn(ARM_PSCI_0_2_FN_SYSTEM_RESET);
}

static int __init psci_sysreset_init(void)
{
	struct device_d *dev;
	const char *version;
	int ret;

	dev = get_device_by_name("psci.of");
	if (IS_ERR(dev))
		return PTR_ERR(dev);

	version = dev_get_param(dev, "version");
	if (IS_ERR(version))
		return PTR_ERR(version);

	if (strcmp(version, "0.1"))
		return 0;

	ret = poweroff_handler_register_fn(psci_poweroff);
	if (ret)
		return ret;

	restart.name = "psci";
	restart.restart = psci_restart;
	restart.priority = 400;

	return restart_handler_register(&restart);
}
device_initcall(psci_sysreset_init);
