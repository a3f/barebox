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
#include <asm/barebox-arm.h>
#include <asm/barebox-arm-head.h>
#include <asm-generic/memory_layout.h>
#include <asm/sections.h>
#include <asm/secure.h>
#include <asm/unaligned.h>
#include <asm/cache.h>
#include <asm/mmu.h>
#include <linux/kasan.h>
#include <memory.h>
#include <pbl/handoff-data.h>
#include <uncompress.h>
#include <compressed-dtb.h>
#include <malloc.h>
#include <asm/armv7r-mpu.h>
#include <efi/mode.h>

#include <debug_ll.h>

#include "entry.h"

static unsigned long arm_stack_top;
static unsigned long arm_endmem;
static unsigned long arm_membase;

u32 barebox_arm_machine(void)
{
	size_t size;
	unsigned int *machine;

	machine = handoff_data_get_entry(HANDOFF_DATA_ARM_MACHINE, &size);

	if (machine)
		return *machine;

	return 0;
}

void *barebox_arm_boot_dtb(void)
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

static struct elf_image_info *barebox_elf_info(void)
{
	static struct elf_image_info *elf_info;
	size_t size;

	if (elf_info)
		return elf_info;

	elf_info = handoff_data_get_entry(HANDOFF_DATA_ELF_IMAGE_INFO, &size);

	BUG_ON(!elf_info || size != sizeof(struct elf_image_info));

	return elf_info;
}

unsigned long arm_mem_ramoops_get(void)
{
	return arm_mem_ramoops(arm_endmem);
}
EXPORT_SYMBOL_GPL(arm_mem_ramoops_get);

unsigned long arm_mem_endmem_get(void)
{
	return arm_endmem;
}
EXPORT_SYMBOL_GPL(arm_mem_endmem_get);

unsigned long arm_mem_membase_get(void)
{
	return arm_membase;
}
EXPORT_SYMBOL_GPL(arm_mem_membase_get);

static int barebox_memory_areas_init(void)
{
	if (kasan_enabled())
		request_sdram_region("kasan shadow", kasan_shadow_base,
				     mem_malloc_start() - kasan_shadow_base,
				     MEMTYPE_BOOT_SERVICES_DATA, MEMATTRS_RW);

	if (efi_is_payload())
		return 0;

	if (!request_barebox_region("stack", arm_stack_top - STACK_SIZE, STACK_SIZE,
				    MEMATTRS_RW))
		pr_err("Error: Cannot request SDRAM region for stack\n");

	return 0;
}
coredevice_initcall(barebox_memory_areas_init);

/* This will be the barebox proper ELF entry point */
void start(unsigned long membase,
	   unsigned long memsize, struct handoff_data *hd);

__noreturn void start(unsigned long membase,
		unsigned long memsize, struct handoff_data *hd)
{
	unsigned long endmem = membase + memsize;
	unsigned long malloc_start, malloc_end;
	struct elf_image_info *elfinfo;
	unsigned long barebox_base;

	if (IS_ENABLED(CONFIG_CPU_V7))
		armv7_hyp_install();

	pbl_barebox_break();

	pr_debug("memory at 0x%08lx, size 0x%08lx\n", membase, memsize);

	handoff_data_set(hd);

	elfinfo = barebox_elf_info();
	barebox_base = (uintptr_t)elfinfo->low_addr;

	arm_membase = membase;
	arm_endmem = endmem;
	arm_stack_top = arm_mem_stack_top(endmem);
	malloc_end = barebox_base;

	if (IS_ENABLED(CONFIG_ARMV7R_MPU)) {
		malloc_end = ALIGN_DOWN(malloc_end - SZ_8M, SZ_8M);

		armv7r_mpu_init_coherent(malloc_end, REGION_8MB);
	}

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

	register_barebox_area(barebox_base,
			      arm_mem_barebox_image_end(endmem) - barebox_base);

	kasan_init(membase, memsize, malloc_start - (memsize >> KASAN_SHADOW_SCALE_SHIFT));

	mem_malloc_init((void *)malloc_start, (void *)malloc_end - 1);

	if (IS_ENABLED(CONFIG_BOOTM_OPTEE))
		of_add_reserve_entry(endmem - OPTEE_SIZE, endmem - 1);

	pr_debug("starting barebox...\n");

	start_barebox();
}
