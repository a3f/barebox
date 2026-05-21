
// SPDX-License-Identifier: GPL-2.0-only

#include <command.h>
#include <clock.h>
#include <linux/kstrtox.h>
#include <linux/kstrtox.h>
#include <getopt.h>
#include <stdio.h>

static __maybe_unused int do_clocksource(int argc, char *argv[])
{
	unsigned select_idx = 0;
	int ret, opt;
	struct clocksource *cs;
	int i = 1;

	while ((opt = getopt(argc, argv, "s:")) > 0) {
		switch (opt) {
		case 's':
			ret = kstrtouint(optarg, 10, &select_idx);
			if (!ret && !select_idx)
				ret = -EINVAL;
			if (ret)
				return ret;
			break;
		default:
			return COMMAND_ERROR_USAGE;
		}
	}

	argv += optind;
	argc -= optind;

	if (argc)
		return COMMAND_ERROR_USAGE;

	list_for_each_entry(cs, &clocksource_list, list) {
		if (select_idx) {
			if (select_idx == i) {
				current_clock = cs;
				return 0;
			}
		} else {
			printf("%c %2d %-20ps %6d\n",
			       cs == current_clock ? '*' : ' ',
			       i, cs->read, cs->priority);
		}

		i++;
	}

	return select_idx ? -EINVAL : 0;
}

BAREBOX_CMD_HELP_START(clocksource)
	BAREBOX_CMD_HELP_TEXT("List and select clocksources.")
	BAREBOX_CMD_HELP_TEXT("Without options, displays list of clocksources")
	BAREBOX_CMD_HELP_TEXT("Options:")
	BAREBOX_CMD_HELP_OPT("-s CLOCKSOURCE_INDEX\t", "select clocksource by (unstable!) index")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(clocksource)
	.cmd		= do_clocksource,
	BAREBOX_CMD_DESC("list and select clocksource")
	BAREBOX_CMD_OPTS("[-s]")
	BAREBOX_CMD_GROUP(CMD_GRP_HWMANIP)
	BAREBOX_CMD_HELP(cmd_clocksource_help)
BAREBOX_CMD_END
