// SPDX-License-Identifier: GPL-2.0-only

#include <sconfig.h>
#include <generated/sconfig_names.h>
#include <string.h>
#include <errno.h>
#include <linux/kernel.h>

int sconfig_lookup(const char *name)
{
	for (int i = 0; i < ARRAY_SIZE(sconfig_names); i++) {
		if (!strcmp(name, sconfig_names[i]))
			return i;
	}

	return -ENOENT;
}
