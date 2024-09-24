// SPDX-License-Identifier: GPL-2.0-or-later

#include <command.h>
#include <common.h>
#include <complete.h>
#include <driver.h>
#include <environment.h>
#include <fnmatch.h>

static int do_varinfo(int argc, char *argv[])
{
	struct device *dev;
	struct param_d *param;
	const char *prefix = NULL, *val;
	char *arg, *dot;
	bool found = false;

	if (argc != 2)
		return COMMAND_ERROR_USAGE;

	arg = argv[1];

	dot = strchr(arg, '.');
	if (dot) {
		*dot = '\0';
		if (dot[1])
			prefix = &dot[1];
	} else {
		val = getenv(arg);
		if (!val)
			goto not_found;

		printf("%s: %s (environment variable)\n", arg, val);
		return 0;
	}

	dev = get_device_by_name(arg);
	if (!dev)
		return -ENODEV;

	list_for_each_entry(param, &dev->parameters, list) {
		if (prefix && !strstarts(param->name, prefix))
			continue;

		printf("%s: %s (type: %s)", param->name,
		       dev_get_param(dev, param->name), get_param_type(param));
		if (param->info)
			param->info(param);
		printf("\n");
		found = true;
	}

	if (!found)
		goto not_found;

	return 0;
not_found:
	printf("%s: no matching variable found\n", arg);
	return 1;
}

BAREBOX_CMD_HELP_START(varinfo)
BAREBOX_CMD_HELP_TEXT("shows information about the variable in its argument")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(varinfo)
	.cmd		= do_varinfo,
	BAREBOX_CMD_DESC("show information about variables")
	BAREBOX_CMD_OPTS("VAR")
	BAREBOX_CMD_GROUP(CMD_GRP_INFO)
	BAREBOX_CMD_HELP(cmd_varinfo_help)
	BAREBOX_CMD_COMPLETE(env_param_noeval_complete)
BAREBOX_CMD_END
