// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <common.h>
#include <bselftest.h>
#include <blspec.h>

BSELFTEST_GLOBALS();

static void test_parser(void)
{
}
bselftest(core, test_parser);
