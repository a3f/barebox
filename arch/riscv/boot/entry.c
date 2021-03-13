/* SPDX-License-Identifier: GPL-2.0 */
#include <types.h>

#include <asm/barebox-riscv.h>

#include "entry.h"
#include <debug_ll.h>

/*
 * Main RISC-V entry point. Call this with the memory region you can
 * spare for barebox. This doesn't necessarily have to be the full
 * SDRAM. The currently running binary can be inside or outside of
 * this region. The PBL can be running inside or outside of this
 * region.
 *
 * -> membase + memsize
 *   STACK_SIZE              - stack
 *   128KiB                  - early memory space
 * -> maximum end of barebox binary
 */

void __noreturn __barebox_riscv_entry(unsigned long membase,
				      unsigned long memsize,
				      void *boarddata,
				      unsigned long sp);

void __noreturn barebox_riscv_entry(unsigned long membase,
				    unsigned long memsize, void *boarddata)
{
	__barebox_riscv_entry(membase, memsize, boarddata,
			      riscv_mem_stack_top(membase, membase + memsize));
}

