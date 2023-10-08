// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2009 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix

/* cpuinfo.c - Show information about cp15 registers */

#include <common.h>
#include <getopt.h>
#include <command.h>
#include <complete.h>

static int do_cpuinfo(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "s")) > 0) {
		switch (opt) {
		case 's':
			if (!IS_ENABLED(CONFIG_ARCH_HAS_STACK_DUMP))
				return -ENOSYS;

			dump_stack();
			return 0;
		default:
			return COMMAND_ERROR_USAGE;
		}
	}

	return 0;
}

BAREBOX_CMD_HELP_START(cpuinfo)
BAREBOX_CMD_HELP_TEXT("Shows misc info about CPU")
BAREBOX_CMD_HELP_OPT ("-s", "print call stack info (if supported)")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(cpuinfo)
	.cmd            = do_cpuinfo,
	BAREBOX_CMD_DESC("show info about CPU")
	BAREBOX_CMD_OPTS("[-s]")
	BAREBOX_CMD_GROUP(CMD_GRP_INFO)
	BAREBOX_CMD_COMPLETE(empty_complete)
	BAREBOX_CMD_HELP(cmd_cpuinfo_help)
BAREBOX_CMD_END
