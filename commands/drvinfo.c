// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: © 2013 Sascha Hauer, Pengutronix
// SPDX-FileCopyrightText: © 2014 Holger Schurig

#include <common.h>
#include <command.h>
#include <driver.h>
#include <complete.h>
#include <fnmatch.h>
#include <getopt.h>

static int do_drvinfo(int argc, char *argv[])
{
	char *pattern;
	struct driver *drv;
	struct device *dev;
	bool missing_devs = false, unused_drvs = false;
	int opt;

	while((opt = getopt(argc, argv, "mu")) > 0) {
		switch(opt) {
		case 'm':
			missing_devs = true;
			break;
		case 'u':
			unused_drvs = true;
			break;
		default:
			return COMMAND_ERROR_USAGE;
		}
	}

	argv += optind;
	argc -= optind;

	pattern = argv[0];

	if (!missing_devs || !unused_drvs) {
		printf("Driver\tDevice(s)\n");
		printf("--------------------\n");
		for_each_driver(drv) {
			if (pattern && fnmatch(pattern, drv->name, 0))
				continue;

			printf("%s\n",drv->name);
			for_each_device(dev) {
				if (dev->driver == drv)
					printf("\t%s\n", dev_name(dev));
			}
		}

		if (IS_ENABLED(CONFIG_CMD_DEVINFO))
			printf("\nUse 'devinfo DEVICE' for more information\n");
	}

	if (missing_devs) {
		printf("Devices missing a driver:\n");
		printf("--------------------\n");
		for_each_device(dev) {
			if (dev->driver)
				continue;

			if (pattern && fnmatch(pattern, dev_name(dev), 0))
				continue;

			printf("%s\n", dev_name(dev));
		}
	}

	if (unused_drvs) {
		printf("Drivers without any devices:\n");
		printf("--------------------\n");
		for_each_driver(drv) {
			for_each_device(dev) {
				if (dev->driver == drv)
					goto next;
			}

			if (pattern && fnmatch(pattern, drv->name, 0))
				continue;

			printf("%s\n", drv->name);
			next: /* driver */;
		}
	}

	return 0;
}

BAREBOX_CMD_HELP_START(drvinfo)
BAREBOX_CMD_HELP_TEXT("List compiled-in device drivers and the devices they support")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("Options:")
BAREBOX_CMD_HELP_OPT ("-m",  "List devices not bound by a driver")
BAREBOX_CMD_HELP_OPT ("-u",  "List drivers not bound to a device")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(drvinfo)
	.cmd		= do_drvinfo,
	BAREBOX_CMD_DESC("list compiled-in device drivers")
	BAREBOX_CMD_OPTS("[-mu] [DRIVER]")
	BAREBOX_CMD_GROUP(CMD_GRP_INFO)
	BAREBOX_CMD_HELP(cmd_drvinfo_help)
	BAREBOX_CMD_COMPLETE(driver_complete)
BAREBOX_CMD_END
