// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2010 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix

#define pr_fmt(fmt) "start.c: " fmt

#ifdef CONFIG_DEBUG_INITCALLS
#define DEBUG
#endif

#include <common.h>
#include <init.h>
#include <linux/sizes.h>
#include <of.h>
#include <asm/barebox-riscv.h>
#include <asm-generic/memory_layout.h>
#include <asm/sections.h>
#include <asm/unaligned.h>
#include <linux/kasan.h>
#include <memory.h>
#include <pbl/handoff-data.h>
#include <uncompress.h>
#include <malloc.h>
#include <compressed-dtb.h>
#include <asm/irq.h>

#include <debug_ll.h>

#include "entry.h"

unsigned long riscv_stack_top;
static unsigned long riscv_endmem;

void *barebox_riscv_boot_dtb(void)
{
	void *dtb;
	int ret = 0;
	struct barebox_boarddata_compressed_dtb *compressed_dtb;
	static void *boot_dtb;
	void *blob;
	size_t size;

	if (boot_dtb)
		return boot_dtb;

	blob = handoff_data_get_entry(HANDOFF_DATA_INTERNAL_DT, &size);
	if (blob)
		return blob;

	blob = handoff_data_get_entry(HANDOFF_DATA_INTERNAL_DT_Z, &size);
	if (!blob)
		return NULL;

	if (!fdt_blob_can_be_decompressed(blob))
		return NULL;

	compressed_dtb = blob;

	pr_debug("%s: using compressed_dtb\n", __func__);

	dtb = malloc(ALIGN(compressed_dtb->datalen_uncompressed, 4));
	if (!dtb)
		return NULL;

	if (IS_ENABLED(CONFIG_IMAGE_COMPRESSION_NONE))
		memcpy(dtb, compressed_dtb->data,
		       compressed_dtb->datalen_uncompressed);
	else
		ret = uncompress(compressed_dtb->data, compressed_dtb->datalen,
				 NULL, NULL, dtb, NULL, NULL);

	if (ret) {
		pr_err("uncompressing dtb failed\n");
		free(dtb);
		return NULL;
	}

	boot_dtb = dtb;

	return boot_dtb;
}

unsigned long riscv_mem_ramoops_get(void)
{
	return riscv_mem_ramoops(0, riscv_stack_top);
}
EXPORT_SYMBOL_GPL(riscv_mem_ramoops_get);

unsigned long riscv_mem_endmem_get(void)
{
	return riscv_endmem;
}
EXPORT_SYMBOL_GPL(riscv_mem_endmem_get);

/*
 * First function in the uncompressed image. We get here from
 * the pbl. The stack already has been set up by the pbl.
 */
__noreturn
void barebox_non_pbl_start(unsigned long membase, unsigned long memsize,
			   struct handoff_data *hd)
{
	unsigned long endmem = membase + memsize;
	unsigned long malloc_start, malloc_end;
	unsigned long barebox_base = riscv_mem_barebox_image(membase, endmem,
							     barebox_image_size,
							     hd);
	size_t size;

	handoff_data_set(hd);

	/* As EFI payload, we keep the firmware's trap handlers */
	if (!handoff_data_get_entry(HANDOFF_DATA_EFI, &size))
		irq_init_vector(riscv_mode());

	pr_debug("memory at 0x%08lx, size 0x%08lx\n", membase, memsize);

	riscv_endmem = endmem;
	riscv_stack_top = riscv_mem_stack_top(membase, endmem);
	malloc_end = barebox_base;

	/*
	 * Maximum malloc space is the Kconfig value if given
	 * or 1GB.
	 */
	if (MALLOC_SIZE > 0) {
		malloc_start = malloc_end - MALLOC_SIZE;
		if (malloc_start < membase)
			malloc_start = membase;
	} else {
		malloc_start = malloc_end - (malloc_end - membase) / 2;
		if (malloc_end - malloc_start > SZ_1G)
			malloc_start = malloc_end - SZ_1G;
	}

	pr_debug("initializing malloc pool at 0x%08lx (size 0x%08lx)\n",
			malloc_start, malloc_end - malloc_start);

	mem_malloc_init((void *)malloc_start, (void *)malloc_end - 1);

	pr_debug("starting barebox...\n");

	start_barebox();
}
