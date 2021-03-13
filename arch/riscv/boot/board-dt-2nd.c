// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <asm/barebox-riscv.h>
#include <debug_ll.h>
#include <pbl.h>

static noinline void dt_2nd_continue(void *fdt)
{
	unsigned long membase, memsize;

	if (!fdt)
		hang();

	fdt_find_mem(fdt, &membase, &memsize);

	barebox_riscv_entry(membase, memsize, fdt);
}

/* called from assembly */
void dt_2nd_riscv(unsigned long a0, void *fdt);

void dt_2nd_riscv(unsigned long a0, void *fdt)
{
	relocate_to_current_adr();
	setup_c();

	dt_2nd_continue(fdt);
}
