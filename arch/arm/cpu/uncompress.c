// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2010-2013 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
// SPDX-FileCopyrightText: 2012 Jean-Christophe PLAGNIOL-VILLARD <plagnioj@jcrosoft.com>

/* uncompress.c - uncompressor code for self extracing pbl image */

#define pr_fmt(fmt) "uncompress.c: " fmt

#include <common.h>
#include <init.h>
#include <linux/sizes.h>
#include <pbl.h>
#include <pbl/elf.h>
#include <pbl/handoff-data.h>
#include <asm/barebox-arm.h>
#include <asm/barebox-arm-head.h>
#include <asm-generic/memory_layout.h>
#include <asm/sections.h>
#include <asm/secure.h>
#include <asm/cache.h>
#include <asm/mmu.h>
#include <asm/unaligned.h>
#include <compressed-dtb.h>

#include <debug_ll.h>

#include "entry.h"

extern unsigned char input_data[];
extern unsigned char input_data_end[];

void __noreturn barebox_pbl_start(unsigned long membase, unsigned long memsize,
				  void *boarddata)
{
	uint32_t pg_len, uncompressed_len;
	void __noreturn (*barebox)(unsigned long, unsigned long, void *);
	unsigned long endmem = membase + memsize;
	unsigned long barebox_base;
	struct elf_image elf;
	void *pg_start, *pg_end;
	unsigned long pc = get_pc();
	void *handoff_data;

	/* piggy data is not relocated, so determine the bounds now */
	pg_start = runtime_address(input_data);
	pg_end = runtime_address(input_data_end);

	/*
	 * If we run from inside the memory just relocate the binary
	 * to the current address. Otherwise it may be a readonly location.
	 * Copy and relocate to the start of the memory in this case.
	 */
	if (pc > membase && pc - membase < memsize)
		relocate_to_current_adr();
	else
		relocate_to_adr(membase);

	pg_len = pg_end - pg_start;

	setup_c();

	pr_debug("memory at 0x%08lx, size 0x%08lx\n", membase, memsize);

	arm_pbl_init_exceptions();

	/* The sequence of operations is important here: */

	/* 1. We parse the ELF to determine uncompressed size */
	pbl_elf_parse(&elf, pg_start, pg_len);
	uncompressed_len = ALIGN(elf.high_addr - elf.low_addr, 8);

	/* 2. Add handoff data, so arm_mem_barebox_image takes it into account */
	if (boarddata)
		handoff_data_add_dt(boarddata);

	handoff_data_add(HANDOFF_DATA_ELF_IMAGE_INFO, &elf.info,
			 sizeof(elf.info));

	/* 3. Calculate where barebox proper should be placed */
	barebox_base = arm_mem_barebox_image(membase, endmem,
					     uncompressed_len, NULL);

#ifdef DEBUG
	print_pbl_mem_layout(membase, endmem, barebox_base);
#endif

	/* 4. Setup MMU as early as possible to speed up execution */
	if (IS_ENABLED(CONFIG_MMU))
		mmu_early_enable(membase, memsize, barebox_base);
	else if (IS_ENABLED(CONFIG_ARMV7R_MPU))
		set_cr(get_cr() | CR_C);

	/* 5. Relocate barebox. This will update the handoff data! */
	pbl_elf_relocate(&elf, (void *)barebox_base);

	/* 6. Move the handoff data, including the update ELF info */
	handoff_data = (void *)barebox_base + uncompressed_len + MAX_BSS_SIZE;
	handoff_data_move(handoff_data);

	/* 7. For later decompression, register a malloc pool */
	malloc_add_pool((void *)barebox_base - ARM_MEM_EARLY_MALLOC_SIZE,
			ARM_MEM_EARLY_MALLOC_SIZE);

	pr_debug("uncompressing barebox binary at 0x%p (size 0x%08x) to 0x%08lx (uncompressed size: 0x%08x)\n",
			pg_start, pg_len, barebox_base, uncompressed_len);

	/* 8. Actually load the ELF */
	pbl_elf_load(&elf);

	// TODO: remove
	if ((ulong)elf.low_addr != (ulong)barebox_base)
		panic("%lx != %lx\n", (ulong)elf.low_addr, barebox_base);

	sync_caches_for_execution();

	barebox = elf.entry;
	pr_debug("jumping to uncompressed image at 0x%p\n", barebox);

	if (IS_ENABLED(CONFIG_CPU_V7) && boot_cpu_mode() == HYP_MODE)
		armv7_switch_to_hyp();

	// TODO: does the THUMB2 ELF entry have | 1 already?
	if (IS_ENABLED(CONFIG_THUMB2_BAREBOX))
		barebox = (void *)((uintptr_t)barebox | 1);

	barebox(membase, memsize, handoff_data);
}
