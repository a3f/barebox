// SPDX-License-Identifer: GPL-2.0
/*
 * Copyright (C) 2019 Ahmad Fatoum <a.fatoum@pengutronix.de>
 */

#include <bootstrap.h>
#include <common.h>
#include <command.h>
#include <init.h>

static int nintendo_init(void)
{

	char *ls_argv[] = { "ls", "/dev", NULL };
	int err;

	err = execute_command(ARRAY_SIZE(ls_argv) - 1, ls_argv);
	if (err)
		bootstrap_err("%s: executing ls failed: %s\n", __func__, strerror(-err));

	char *argv[] = { "fbtest", NULL };

	err = execute_command(ARRAY_SIZE(argv) - 1, argv);
	if (err)
		bootstrap_err("%s: executing fbtest failed: %s\n", __func__, strerror(-err));

	hang();
	return 0;
}

static int nintendo_set_init(void)
{
	barebox_main = nintendo_init;

	return 0;
}
late_initcall(nintendo_set_init);
