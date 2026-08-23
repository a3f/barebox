// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <fdt.h>
#include <linux/sizes.h>
#include <asm/barebox-arm-head.h>
#include <asm/barebox-arm.h>
#include <io.h>
#include <debug_ll.h>
#include <asm/cache.h>
#include <asm/sections.h>
#include <pbl.h>
#include <pbl/handoff-data.h>

extern char __dtb_fallback_start[];

static noinline void __noreturn dt_2nd_continue(void *fdt)
{
	unsigned long membase, memsize;

	if (fdt && get_unaligned_be32(fdt) != FDT_MAGIC)
		fdt = NULL;

	if (IS_ENABLED(CONFIG_EXTERNAL_DTS_ONLY)) {
		/*
		 * The device tree built from the external dts fragments replaces
		 * the firmware-provided device tree, which remains accessible for
		 * barebox proper in the handoff data. It's linked in uncompressed,
		 * because the memory it describes has to be read before there is
		 * memory to decompress it into.
		 */
		if (fdt)
			handoff_data_add(HANDOFF_DATA_EXTERNAL_DT, fdt,
					 get_unaligned_be32(fdt + 4));
		fdt = __dtb_fallback_start;
	}

	if (!fdt)
		hang();

	fdt_find_mem(fdt, &membase, &memsize);

	barebox_arm_entry(membase, memsize, fdt);
}

#ifdef CONFIG_CPU_V8

/* called from assembly */
void dt_2nd_aarch64(void *fdt);

void dt_2nd_aarch64(void *fdt)
{
	putc_ll('>');

	/* entry point already set up stack */

	arm_cpu_lowlevel_init();

	relocate_to_current_adr();
	setup_c();

	dt_2nd_continue(fdt);
}

#else

ENTRY_FUNCTION(start_dt_2nd, r0, r1, r2)
{
	unsigned long image_start = (unsigned long)_text + global_variable_offset();

	arm_cpu_lowlevel_init();

	arm_setup_stack(image_start);

	relocate_to_current_adr();
	setup_c();
	barrier();

	dt_2nd_continue((void *)r2);
}
#endif

/*
 * ELF loaders, e.g. the Xilinx Zynq FSBL fed by bootgen, pass no
 * arguments and are assumed to leave a usable stack behind.
 */
ENTRY_FUNCTION_WITHSTACK(start_dt_2nd_elf, 0, r0, r1, r2)
{
	arm_cpu_lowlevel_init();

	relocate_to_current_adr();
	setup_c();
	barrier();

	dt_2nd_continue(NULL);
}
