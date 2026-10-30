// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <command.h>
#include <getopt.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <environment.h>
#include <malloc.h>
#include <driver.h>
#include <of.h>
#include <stringlist.h>

static int do_clk_enable(int argc, char *argv[])
{
	struct clk *clk;

	if (argc != 2)
		return COMMAND_ERROR_USAGE;

	clk = clk_lookup(argv[1]);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	return clk_enable(clk);
}

BAREBOX_CMD_START(clk_enable)
	.cmd		= do_clk_enable,
	BAREBOX_CMD_DESC("enable a clock")
	BAREBOX_CMD_OPTS("CLK")
	BAREBOX_CMD_GROUP(CMD_GRP_HWMANIP)
	BAREBOX_CMD_COMPLETE(clk_name_complete)
BAREBOX_CMD_END

static int do_clk_disable(int argc, char *argv[])
{
	struct clk *clk;

	if (argc != 2)
		return COMMAND_ERROR_USAGE;

	clk = clk_lookup(argv[1]);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	clk_disable(clk);

	return COMMAND_SUCCESS;
}

BAREBOX_CMD_START(clk_disable)
	.cmd		= do_clk_disable,
	BAREBOX_CMD_DESC("disable a clock")
	BAREBOX_CMD_OPTS("CLK")
	BAREBOX_CMD_GROUP(CMD_GRP_HWMANIP)
	BAREBOX_CMD_COMPLETE(clk_name_complete)
BAREBOX_CMD_END

static int do_clk_set_rate(int argc, char *argv[])
{
	unsigned long rate;

	if (argc != 3)
		return COMMAND_ERROR_USAGE;

	rate = simple_strtoul(argv[2], NULL, 0);

	return clk_name_set_rate(argv[1], rate);
}

BAREBOX_CMD_HELP_START(clk_set_rate)
BAREBOX_CMD_HELP_TEXT("Set clock CLK rate to HZ")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(clk_set_rate)
	.cmd		= do_clk_set_rate,
	BAREBOX_CMD_DESC("set a clock's rate")
	BAREBOX_CMD_OPTS("CLK HZ")
	BAREBOX_CMD_GROUP(CMD_GRP_HWMANIP)
	BAREBOX_CMD_HELP(cmd_clk_set_rate_help)
	BAREBOX_CMD_COMPLETE(clk_name_complete)
BAREBOX_CMD_END

static int do_clk_round_rate(int argc, char *argv[])
{
	struct clk *clk;
	unsigned long rate;

	if (argc != 3)
		return COMMAND_ERROR_USAGE;

	clk = clk_lookup(argv[1]);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	rate = simple_strtoul(argv[2], NULL, 0);

	printf("%ld\n", clk_round_rate(clk, rate));

	return 0;
}

BAREBOX_CMD_HELP_START(clk_round_rate)
BAREBOX_CMD_HELP_TEXT("Show clock CLK actual rate if set to HZ")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(clk_round_rate)
	.cmd		= do_clk_round_rate,
	BAREBOX_CMD_DESC("show a resulting clocks rate")
	BAREBOX_CMD_OPTS("CLK HZ")
	BAREBOX_CMD_GROUP(CMD_GRP_HWMANIP)
	BAREBOX_CMD_HELP(cmd_clk_round_rate_help)
	BAREBOX_CMD_COMPLETE(clk_name_complete)
BAREBOX_CMD_END

static int do_clk_get_rate(int argc, char *argv[])
{
	int opt;
	struct clk *clk;
	unsigned long rate;
	const char *variable_name = NULL;

	while ((opt = getopt(argc, argv, "s:")) > 0) {
		switch (opt) {
		case 's':
			variable_name = optarg;
			break;
		default:
			return COMMAND_ERROR_USAGE;
		}
	}

	if (optind == argc) {
		eprintf("No clock name given\n");
		return COMMAND_ERROR_USAGE;
	}

	clk = clk_lookup(argv[optind]);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	rate = clk_get_rate(clk);

	if (variable_name)
		pr_setenv(variable_name, "%lu", rate);
	else
		printf("%lu\n", rate);

	return COMMAND_SUCCESS;
}

BAREBOX_CMD_HELP_START(clk_get_rate)
BAREBOX_CMD_HELP_TEXT("Show clock CLK rate")
BAREBOX_CMD_HELP_OPT("-s VARNAME",  "set variable VARNAME instead of showing information")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(clk_get_rate)
	.cmd		= do_clk_get_rate,
	BAREBOX_CMD_DESC("get a clocks rate")
	BAREBOX_CMD_OPTS("[-s VARNAME] CLK")
	BAREBOX_CMD_GROUP(CMD_GRP_HWMANIP)
	BAREBOX_CMD_HELP(cmd_clk_get_rate_help)
	BAREBOX_CMD_COMPLETE(clk_name_complete)
BAREBOX_CMD_END

static int do_clk_dump(int argc, char *argv[])
{
	int opt, flags = 0;
	struct clk *clk;

	while ((opt = getopt(argc, argv, "vj")) > 0) {
		switch(opt) {
		case 'v':
			flags |= CLK_DUMP_VERBOSE;
			break;
		case 'j':
			flags |= CLK_DUMP_JSON;
			break;
		default:
			return -EINVAL;

		}
	}

	if (optind == argc) {
		clk_dump(flags);
		return COMMAND_SUCCESS;
	}

	clk = clk_lookup(argv[optind]);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	clk_dump_one(clk, flags);

	return COMMAND_SUCCESS;
}

BAREBOX_CMD_HELP_START(clk_dump)
BAREBOX_CMD_HELP_TEXT("Options:")
BAREBOX_CMD_HELP_OPT ("-v",  "verbose")
BAREBOX_CMD_HELP_OPT ("-j",  "json output")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(clk_dump)
	.cmd		= do_clk_dump,
	BAREBOX_CMD_DESC("show information about registered clocks")
	BAREBOX_CMD_OPTS("[-vj] [clkname]")
	BAREBOX_CMD_GROUP(CMD_GRP_INFO)
	BAREBOX_CMD_HELP(cmd_clk_dump_help)
	BAREBOX_CMD_COMPLETE(clk_name_complete)
BAREBOX_CMD_END

static int do_clk_set_parent(int argc, char *argv[])
{
	if (argc != 3)
		return COMMAND_ERROR_USAGE;

	return clk_name_set_parent(argv[1], argv[2]);
}

BAREBOX_CMD_START(clk_set_parent)
	.cmd		= do_clk_set_parent,
	BAREBOX_CMD_DESC("set parent of a clock")
	BAREBOX_CMD_OPTS("CLK PARENT")
	BAREBOX_CMD_GROUP(CMD_GRP_HWMANIP)
	BAREBOX_CMD_COMPLETE(clk_name_complete)
BAREBOX_CMD_END

#ifdef CONFIG_COMMON_CLK_OF_PROVIDER
static struct clk *clk_assigned_get(struct device_node *np,
				    struct of_phandle_args *clkspec)
{
	struct clk *clk;
	char args[32] = "";
	int i, n = 0;

	clk = of_clk_get_from_provider(clkspec);
	/* onecell providers return NULL for clocks barebox doesn't implement */
	if (!IS_ERR_OR_NULL(clk))
		return clk;

	for (i = 0; i < clkspec->args_count; i++)
		n += scnprintf(args + n, sizeof(args) - n, " %u", clkspec->args[i]);

	printf("%pOF: clock <&%pOF%s> not found\n", np, clkspec->np, args);
	return NULL;
}

static bool clk_assigned_check_one(struct device_node *np, int index,
				   bool clk_supplier,
				   struct string_list *ignore)
{
	struct of_phandle_args clkspec;
	struct clk *clk, *parent;
	unsigned long rate;
	u32 assigned;
	bool ok = true;

	/* skip empty (null) phandles */
	if (of_parse_phandle_with_args(np, "assigned-clocks", "#clock-cells",
				       index, &clkspec))
		return true;
	if (clkspec.np == np && !clk_supplier)
		return true;

	clk = clk_assigned_get(np, &clkspec);
	if (!clk)
		return false;
	if (string_list_contains(ignore, clk->name))
		return true;

	if (!of_parse_phandle_with_args(np, "assigned-clock-parents",
					"#clock-cells", index, &clkspec) &&
	    (clkspec.np != np || clk_supplier)) {
		parent = clk_assigned_get(np, &clkspec);
		if (!parent) {
			ok = false;
		} else if (clk_get_parent(clk) != parent) {
			printf("%pOF: %pC has parent %pC instead of %pC\n",
			       np, clk, clk_get_parent(clk), parent);
			ok = false;
		}
	}

	if (!of_property_read_u32_index(np, "assigned-clock-rates", index,
					&assigned) && assigned) {
		rate = clk_get_rate(clk);
		if (rate != assigned && rate != clk_round_rate(clk, assigned)) {
			printf("%pOF: %pC runs at %lu Hz instead of %u Hz\n",
			       np, clk, rate, assigned);
			ok = false;
		}
	}

	return ok;
}

static int do_clk_assigned_check(int argc, char *argv[])
{
	struct device_node *np;
	struct string_list ignore;
	int opt, failed = 0;

	string_list_init(&ignore);

	while ((opt = getopt(argc, argv, "i:")) > 0) {
		switch (opt) {
		case 'i':
			string_list_add(&ignore, optarg);
			break;
		default:
			string_list_free(&ignore);
			return COMMAND_ERROR_USAGE;
		}
	}

	if (optind != argc) {
		string_list_free(&ignore);
		return COMMAND_ERROR_USAGE;
	}

	for_each_node_with_property(np, "assigned-clocks") {
		bool clk_supplier = of_clk_provider_registered(np);
		int i, nclks;

		/* only providers and probed devices had them applied */
		if (!clk_supplier && !(np->dev && dev_is_probed(np->dev)))
			continue;

		nclks = of_count_phandle_with_args(np, "assigned-clocks",
						   "#clock-cells");
		for (i = 0; i < nclks; i++)
			failed += !clk_assigned_check_one(np, i, clk_supplier,
							  &ignore);
	}

	string_list_free(&ignore);

	if (!failed)
		return COMMAND_SUCCESS;

	printf("assigned clocks not in effect: %d\n", failed);
	return COMMAND_ERROR;
}

BAREBOX_CMD_HELP_START(clk_assigned_check)
BAREBOX_CMD_HELP_TEXT("Check that the assigned-clocks of all clock providers and probed")
BAREBOX_CMD_HELP_TEXT("devices still have their assigned parents and rates.")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("Options:")
BAREBOX_CMD_HELP_OPT ("-i CLK",  "ignore assignments to CLK, may be given multiple times")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(clk_assigned_check)
	.cmd		= do_clk_assigned_check,
	BAREBOX_CMD_DESC("check assigned clocks against the device tree")
	BAREBOX_CMD_OPTS("[-i CLK]")
	BAREBOX_CMD_GROUP(CMD_GRP_INFO)
	BAREBOX_CMD_HELP(cmd_clk_assigned_check_help)
BAREBOX_CMD_END
#endif
