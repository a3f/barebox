// SPDX-License-Identifier: GPL-2.0-only

#include <init.h>
#include <linux/printk.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <pbl/soc-type.h>
#include <pbl/handoff-data.h>

#ifdef __PBL__

static unsigned int __pbl_soc_type __section(.data);

void pbl_set_soc_type(unsigned int type)
{
	__pbl_soc_type = type;
}

unsigned int pbl_get_soc_type(void)
{
	return __pbl_soc_type;
}

/*
 * The handoff entry references the variable itself, so pbl_set_soc_type()
 * still takes effect until the handoff data is moved out of the PBL.
 */
void pbl_handoff_soc_type(void)
{
	size_t size;

	/* Firmware returning into the PBL could add the entry twice */
	if (handoff_data_get_entry(HANDOFF_DATA_SOC_TYPE, &size))
		return;

	handoff_data_add(HANDOFF_DATA_SOC_TYPE,
			 &__pbl_soc_type, sizeof(__pbl_soc_type));
}

#else /* barebox proper */

unsigned int barebox_soc_type(void)
{
	unsigned int *type;
	size_t size;

	type = handoff_data_get_entry(HANDOFF_DATA_SOC_TYPE, &size);
	if (!type)
		return 0;

	return *type;
}

/*
 * A missing entry means an ENTRY_FUNCTION() entry point, which declares
 * no SoC; an entry left at SOC_TYPE_UNKNOWN is a board info that forgot.
 */
static int soc_type_check(void)
{
	unsigned int *type;
	size_t size;

	type = handoff_data_get_entry(HANDOFF_DATA_SOC_TYPE, &size);
	if (type && *type == SOC_TYPE_UNKNOWN)
		pr_err("PBL entry point must declare SoC type\n");

	return 0;
}
postcore_initcall(soc_type_check);

#endif
