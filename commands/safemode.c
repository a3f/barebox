// SPDX-License-Identifier: GPL-2.0-only

#include <command.h>
#include <getopt.h>
#include <device.h>
#include <xfuncs.h>
#include <stdio.h>
#include <environment.h>

#define	SAFEMODE_MMC		BIT(0)
#define	SAFEMODE_CONSOLE	BIT(1)
#define	SAFEMODE_BOOT		BIT(2)

#define run_command_fmt(args...) ({	\
	char *__buf = xasprintf(args);	\
	if (verbose)			\
		printf("%s\n", __buf);	\
	int __ret = run_command(__buf);	\
	free(__buf);			\
	__ret;				\
})

static void safemode_mmc(int verbose)
{
	struct class *mmc_class;
	struct device *dev;

	mmc_class = get_class_by_name("mmc");
	if (!mmc_class)
		return;

	class_for_each_device(mmc_class, dev) {
		const char *devname = dev_name(dev);

		run_command_fmt("%s.broken_cd=1", devname);
		run_command_fmt("of_property -fs %s max-frequency '<52000000>'", devname);
		run_command_fmt("of_property -fs %s pinctrl-names default", devname);
	}
}

static void safemode_console(int verbose)
{
	run_command_fmt("global.bootm.earlycon=1");
}

static void safemode_boot(int verbose)
{
	if (IS_ENABLED(CONFIG_WATCHDOG))
		run_command_fmt("global.boot.watchdog_timeout=0");

	if (IS_ENABLED(CONFIG_BOOTCHOOSER) &&
	    getenv_nonempty("global.bootchooser.targets"))
		run_command_fmt("bootchooser -a default -p default");

	if (IS_ENABLED(CONFIG_EFI_HANDOVER_PROTOCOL))
		run_command_fmt("global.linux.efi.handover=1");
}

static int do_safemode(int argc, char *argv[])
{
	unsigned safemode = 0;
	int opt, verbose = 0;

	while((opt = getopt(argc, argv, "mcbv")) > 0) {
		switch(opt) {
		case 'm':
			safemode |= SAFEMODE_MMC;
			break;
		case 'c':
			safemode |= SAFEMODE_CONSOLE;
			break;
		case 'b':
			safemode |= SAFEMODE_BOOT;
			break;
		case 'v':
			verbose++;
			break;
		default:
			return COMMAND_ERROR_USAGE;
		}
	}

	if (argc != optind)
		return COMMAND_ERROR_USAGE;

	if (!safemode)
		safemode = ~0;

	if (safemode & SAFEMODE_MMC)
		safemode_mmc(verbose);
	if (safemode & SAFEMODE_CONSOLE)
		safemode_console(verbose);
	if (safemode & SAFEMODE_BOOT)
		safemode_boot(verbose);

	return 0;
}

BAREBOX_CMD_HELP_START(safemode)
BAREBOX_CMD_HELP_TEXT("Apply safe-mode defaults for next kernel boot.")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("Options:")
BAREBOX_CMD_HELP_OPT ("-m",  "safe mmc settings")
BAREBOX_CMD_HELP_OPT ("-c",  "safe console settings")
BAREBOX_CMD_HELP_OPT ("-b",  "safe boot defaults")
BAREBOX_CMD_HELP_OPT ("-v",  "verbose output")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(safemode)
	.cmd		= do_safemode,
	BAREBOX_CMD_DESC("enable safe mode")
	BAREBOX_CMD_OPTS("[-mcbv]")
	BAREBOX_CMD_GROUP(CMD_GRP_HWMANIP)
	BAREBOX_CMD_HELP(cmd_safemode_help)
BAREBOX_CMD_END
