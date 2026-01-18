// SPDX-License-Identifier: GPL-2.0-or-later

#include <globalvar.h>
#include <command.h>
#include <common.h>
#include <getopt.h>
#include <malloc.h>
#include <boot.h>
#include <bootm.h>
#include <complete.h>
#include <bobject.h>
#include <param.h>
#include <unistd.h>

#include <linux/stat.h>

static char *next_argv(void *context)
{
	char ***argv = context;
	char *next = **argv;
	(*argv)++;
	return next;
}

static char *next_word(void *context)
{
	return strsep(context, " ");
}

static int boot_add_override(struct bootm_overrides *overrides, char *var)
{
	const char *val;

	if (!IS_ENABLED(CONFIG_BOOT_OVERRIDE))
		return -ENOSYS;

	var += str_has_prefix(var, "global.");

	val = parse_assignment(var);
	if (!val) {
		val = globalvar_get(var);
		if (isempty(val))
			val = NULL;
	}

	if (!strcmp(var, "bootm.image")) {
		if (isempty(val))
			return -EINVAL;
		return -ENOSYS;
	} else if (!strcmp(var, "bootm.oftree")) {
		overrides->oftree_file = val;
	} else if (!strcmp(var, "bootm.initrd")) {
		overrides->initrd_files = val;
	} else {
		return -EINVAL;
	}

	return 0;
}

struct netboot_state {
	struct bobject *bobj;
	char *image;
	char *initrd;
	char *oftree;
};

static inline struct netboot_state *netboot_init(void)
{
	struct netboot_state *nb;

	if (!IS_ENABLED(CONFIG_CMD_NETBOOT))
		return NULL;

	nb = xzalloc(sizeof(*nb));
	nb->bobj = bobject_alloc("netboot");
	nb->bobj->local = true;

	bobject_add_param_string(nb->bobj, "image", NULL, NULL, &nb->image, NULL);
	bobject_add_param_string(nb->bobj, "initrd", NULL, NULL, &nb->initrd, NULL);
	bobject_add_param_string(nb->bobj, "oftree", NULL, NULL, &nb->oftree, NULL);

	return nb;
}

static inline int netboot_run_script(const char *entry_name)
{
	const char *user = globalvar_get("user");
	const char *hostname = globalvar_get("hostname");
	const char *arch = globalvar_get("arch");
	char *script_path, *cmd;
	struct stat s;
	int ret = -ENOENT;

	/* Try hostname-specific script first */
	script_path = xasprintf("/mnt/tftp/%s-netboot-%s", user, hostname);
	if (stat(script_path, &s) == 0) {
		cmd = xasprintf("source %s %s", script_path, entry_name ?: "");
		ret = run_command(cmd);
		free(cmd);
		free(script_path);
		return ret;
	}
	free(script_path);

	/* Fallback to arch-specific script */
	script_path = xasprintf("/mnt/tftp/%s-netboot-%s", user, arch);
	if (stat(script_path, &s) == 0) {
		cmd = xasprintf("source %s %s", script_path, entry_name ?: "");
		ret = run_command(cmd);
		free(cmd);
		free(script_path);
		return ret;
	}
	free(script_path);

	return ret;
}

static inline void netboot_extract_overrides(struct netboot_state *nb,
					     struct bootm_overrides *overrides,
					     const struct bootm_overrides *cmdline)
{
	/* Per-field precedence: only use netboot value if cmdline didn't set it */
	if (!cmdline->oftree_file && !isempty(nb->oftree))
		overrides->oftree_file = nb->oftree;
	if (!cmdline->initrd_files && !isempty(nb->initrd))
		overrides->initrd_files = nb->initrd;

	/*
	 * Forward netboot.image to boot_add_override (for future implementation)
	 * Note: currently returns -ENOSYS but we pass it through anyway
	 */
	if (!isempty(nb->image)) {
		char *var = xasprintf("bootm.image=%s", nb->image);
		boot_add_override(overrides, var);
		free(var);
	}
}

static inline void netboot_reset_params(struct netboot_state *nb)
{
	free(nb->image);
	free(nb->initrd);
	free(nb->oftree);
	nb->image = NULL;
	nb->initrd = NULL;
	nb->oftree = NULL;
}

static inline void netboot_free(struct netboot_state *nb)
{
	if (!nb)
		return;
	netboot_reset_params(nb);
	bobject_free(nb->bobj);
	free(nb);
}

static int do_boot(int argc, char *argv[])
{
	char *freep = NULL;
	int opt, ret = 0, do_list = 0, do_menu = 0;
	int dryrun = 0, verbose = 0, timeout = -1;
	unsigned default_menu_entry = 0;
	struct bootentries *entries;
	struct bootentry *entry;
	struct bootm_overrides overrides = {};
	struct bootm_overrides cmdline_overrides = {};
	bool is_netboot = false;
	struct netboot_state *netboot = NULL;
	void *handle;
	const char *name;
	char *(*next)(void *);

	if (IS_ENABLED(CONFIG_CMD_NETBOOT)) {
		is_netboot = !strcmp(argv[0], "netboot");
		if (is_netboot && bootm_signed_images_are_forced()) {
			printf("netboot not allowed when signed images are forced\n");
			return -EPERM;
		}
	}

	while ((opt = getopt(argc, argv, "vldmM:t:w:o:")) > 0) {
		switch (opt) {
		case 'v':
			verbose++;
			break;
		case 'l':
			do_list = 1;
			break;
		case 'd':
			dryrun++;
			break;
		case 'M':
			/* To simplify scripting, an empty string is treated as 1 */
			if (*optarg == '\0') {
				default_menu_entry = 1;
			} else {
				ret = kstrtouint(optarg, 0, &default_menu_entry);
				if (ret)
					return ret;
			}
			fallthrough;
		case 'm':
			do_menu = 1;
			break;
		case 't':
			timeout = simple_strtoul(optarg, NULL, 0);
			break;
		case 'w':
			boot_set_watchdog_timeout(simple_strtoul(optarg, NULL, 0));
			break;
		case 'o':
			ret = boot_add_override(&cmdline_overrides, optarg);
			if (ret)
				return ret;
			break;
		default:
			return COMMAND_ERROR_USAGE;
		}
	}

	if (optind < argc) {
		handle = &argv[optind];
		next = next_argv;
	} else {
		const char *def;

		def = getenv("global.boot.default");
		if (!def)
			return 0;

		handle = freep = xstrdup(def);
		next = next_word;
	}

	if (is_netboot)
		netboot = netboot_init();

	entries = bootentries_alloc();

	while ((name = next(&handle)) != NULL) {
		if (!*name)
			continue;
		ret = bootentry_create_from_name(entries, name);
		if (ret <= 0)
			printf("Nothing bootable found on '%s'\n", name);

		if (do_list || do_menu)
			continue;

		bootentries_for_each_entry(entries, entry) {
			/* Start with cmdline overrides */
			overrides = cmdline_overrides;

			/* netboot script execution before each boot_entry */
			if (netboot) {
				netboot_reset_params(netboot);
				netboot_run_script(entry->title);
				netboot_extract_overrides(netboot, &overrides,
							  &cmdline_overrides);
			}

			bootm_merge_overrides(&entry->overrides, &overrides);

			ret = boot_entry(entry, verbose, dryrun);
			if (!ret)
				goto out;
		}

		bootentries_free(entries);
		entries = bootentries_alloc();
	}

	if (list_empty(&entries->entries)) {
		printf("Nothing bootable found\n");
		ret = COMMAND_ERROR;
		goto out;
	}

	if (do_list)
		bootsources_list(entries);
	else if (do_menu)
		bootsources_menu(entries, default_menu_entry, timeout);

	ret = 0;
out:
	netboot_free(netboot);
	bootentries_free(entries);
	free(freep);

	return ret;
}

BAREBOX_CMD_HELP_START(boot)
BAREBOX_CMD_HELP_TEXT("This is for booting based on scripts. Unlike the bootm command which")
BAREBOX_CMD_HELP_TEXT("can boot a single image this command offers the possibility to boot with")
BAREBOX_CMD_HELP_TEXT("scripts (by default placed under /env/boot/).")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("BOOTSRC can be:")
BAREBOX_CMD_HELP_TEXT("- a filename under /env/boot/")
BAREBOX_CMD_HELP_TEXT("- a full path to a boot script")
BAREBOX_CMD_HELP_TEXT("- a full path to a bootspec entry")
BAREBOX_CMD_HELP_TEXT("- a device name")
BAREBOX_CMD_HELP_TEXT("- a partition name under /dev/")
BAREBOX_CMD_HELP_TEXT("- a full path to a directory which")
BAREBOX_CMD_HELP_TEXT("   - contains boot scripts, or")
BAREBOX_CMD_HELP_TEXT("   - contains a loader/entries/ directory containing bootspec entries")
#ifdef CONFIG_BOOTCHOOSER
BAREBOX_CMD_HELP_TEXT("- \"bootchooser\": boot with barebox bootchooser")
#endif
#ifdef CONFIG_BOOT_DEFAULTS
BAREBOX_CMD_HELP_TEXT("- \"bootsource\": boot from the device barebox has been started from")
BAREBOX_CMD_HELP_TEXT("- \"diskuuid.*\": boot from disk with specified diskuuid")
BAREBOX_CMD_HELP_TEXT("- \"storage.removable\": boot from removable media")
BAREBOX_CMD_HELP_TEXT("- \"storage.builtin\": boot from non-removable media")
BAREBOX_CMD_HELP_TEXT("- \"storage\": boot from any available media")
#endif
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("Multiple bootsources may be given which are probed in order until")
BAREBOX_CMD_HELP_TEXT("one succeeds.")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("Options:")
BAREBOX_CMD_HELP_OPT ("-v","Increase verbosity")
BAREBOX_CMD_HELP_OPT ("-d","Dryrun. See what happens but do no actually boot (pass twice to run scripts)")
BAREBOX_CMD_HELP_OPT ("-l","List available boot sources")
BAREBOX_CMD_HELP_OPT ("-m","Show a menu with boot options")
BAREBOX_CMD_HELP_OPT ("-M INDEX","Show a menu with boot options with entry INDEX preselected")
BAREBOX_CMD_HELP_OPT ("-w SECS","Start watchdog with timeout SECS before booting")
#ifdef CONFIG_BOOT_OVERRIDE
BAREBOX_CMD_HELP_OPT ("-o VAR[=VAL]","override VAR (bootm.{oftree,initrd}) with VAL")
BAREBOX_CMD_HELP_OPT ("            ","if VAL is not specified, the value of VAR is taken")
#endif
BAREBOX_CMD_HELP_OPT ("-t SECS","specify timeout in SECS")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(boot)
	.cmd	= do_boot,
	BAREBOX_CMD_DESC("boot from script, device, ...")
	BAREBOX_CMD_OPTS("[-vdlmMwto] [BOOTSRC...]")
	BAREBOX_CMD_GROUP(CMD_GRP_BOOT)
	BAREBOX_CMD_HELP(cmd_boot_help)
BAREBOX_CMD_END

#if IS_ENABLED(CONFIG_CMD_NETBOOT)

BAREBOX_CMD_HELP_START(netboot)
BAREBOX_CMD_HELP_TEXT("Network-assisted boot command. Same as 'boot' but executes")
BAREBOX_CMD_HELP_TEXT("a configuration script from TFTP before each boot attempt.")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("Script lookup order:")
BAREBOX_CMD_HELP_TEXT("  1. /mnt/tftp/${global.user}-netboot-${global.hostname}")
BAREBOX_CMD_HELP_TEXT("  2. /mnt/tftp/${global.user}-netboot-${global.arch}")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("The boot entry name is passed as argument to the script.")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("The script can set these parameters:")
BAREBOX_CMD_HELP_TEXT("  netboot.image   - kernel image path (forwarded to bootm.image)")
BAREBOX_CMD_HELP_TEXT("  netboot.initrd  - initrd path")
BAREBOX_CMD_HELP_TEXT("  netboot.oftree  - device tree path")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("Command-line -o overrides take precedence (per-field).")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(netboot)
	.cmd	= do_boot,
	BAREBOX_CMD_DESC("boot with TFTP-based configuration script")
	BAREBOX_CMD_OPTS("[-vdlmMwto] [BOOTSRC...]")
	BAREBOX_CMD_GROUP(CMD_GRP_BOOT)
	BAREBOX_CMD_HELP(cmd_netboot_help)
BAREBOX_CMD_END

#endif
