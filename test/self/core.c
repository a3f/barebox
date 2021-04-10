/* SPDX-License-Identifier: GPL-2.0 */

#define pr_fmt(fmt) "bselftest: " fmt

#include <common.h>
#include <bselftest.h>

LIST_HEAD(selftests);

static int (*old_main)(void);

static int run_selftests(void)
{
	struct selftest *test;
	int err = 0;

	pr_notice("Configured tests will run now\n");

	list_for_each_entry(test, &selftests, list)
		err |= test->func();

	if (err)
		pr_err("Some selftests failed\n");

	barebox_main = old_main;
	return barebox_main();
}

static int init_selftests(void)
{
	if (!IS_ENABLED(CONFIG_SELFTEST_AUTORUN))
		return 0;

	old_main = barebox_main;
	barebox_main = run_selftests;

	return 0;
}
postenvironment_initcall(init_selftests);
