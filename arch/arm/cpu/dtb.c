/*
 * Copyright (c) 2013 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <common.h>
#include <init.h>
#include <debug_ll.h>
#include <of.h>
#include <asm/barebox-arm.h>

#define DEBUG 1
extern char __dtb_start[];

static int of_arm_init(void)
{
	struct device_node *root;
	void *fdt;
	pr_info("uaaaasing boarddata provided DTB\n");

	/* See if we already have a dtb */
	root = of_get_root_node();
	if (root)
		return 0;

	/* See if we are provided a dtb in boarddata */
	fdt = barebox_arm_boot_dtb();
	if (fdt)
		pr_info("using boarddata provided DTB\n");

	/* Next see if we have a builtin dtb */
	if (!fdt && IS_ENABLED(CONFIG_BUILTIN_DTB)) {
		fdt = __dtb_start;
		pr_info("using internal DTB\n");
	}

	if (!fdt) {
		pr_info("No DTB found\n");
		return 0;
	}

	puts_ll("0\n");
	root = of_unflatten_dtb(fdt);
	puts_ll("1\n");
	if (!IS_ERR(root)) {
	puts_ll("2\n");
		of_set_root_node(root);
	puts_ll("3\n");
		of_fix_tree(root);
	puts_ll("4\n");
		if (IS_ENABLED(CONFIG_OFDEVICE))
			of_probe();
	puts_ll("5\n");
	}

	puts_ll("all well\n");

	return 0;
}
core_initcall(of_arm_init);
