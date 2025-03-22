// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2025 Ahmad Fatoum
 */
#define pr_fmt(fmt)	"bootdef: " fmt

#include <boot.h>
#include <xfuncs.h>
#include <string.h>
#include <bootsource.h>
#include <driver.h>
#include <init.h>

struct bootdef {
	const char *alias;
	char *(*resolve)(struct bootdef *, const char *partition);
};

static char *format_resolution(struct cdev *cdev,
			       const char *partition)
{
	return xasprintf("/dev/%s%s%s", cdev->name,
			 partition ? "." : "", partition ?: "");
}

static char *bootdef_resolve_bootsource(struct bootdef *bootdef,
					const char *partition)
{
	struct cdev *cdev;

	cdev = bootsource_of_cdev_find();
	if (!cdev) {
		pr_info("Could not autodetect bootsource device\n");
		return NULL;
	}

	return format_resolution(cdev, partition);
}

static struct bootdef bootdef_aliases[] = {
	{ "bootsource", bootdef_resolve_bootsource },
	{ /* sentinel */}
};

static int bootdef_add_entry(struct bootentries *entries, const char *name)
{
	const char *end, *partition = NULL;
	struct bootdef *bootdef;
	size_t prefixlen = 0;
	char *resolved_name;
	int ret;

	for (bootdef = bootdef_aliases; bootdef->alias; bootdef++) {
		prefixlen = str_has_prefix(name, bootdef->alias);
		if (prefixlen)
			break;
	}

	if (!prefixlen)
		return 0;

	end = name + prefixlen;
	if (*end == '.')
		partition = end + 1;

	resolved_name = bootdef->resolve(bootdef, partition);
	if (IS_ERR_OR_NULL(resolved_name))
		return PTR_ERR_OR_ZERO(resolved_name);

	pr_info("%s resolved to %s\n", name, resolved_name);
	ret = bootentry_create_from_name(entries, resolved_name);

	free(resolved_name);
	return ret;
}

static int bootdef_entry_init(void)
{
	bootentry_register_provider(bootdef_add_entry);

	return 0;
}
device_initcall(bootdef_entry_init);
