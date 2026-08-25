// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <asm/sections.h>
#include <asm/barebox-riscv.h>
#include <linux/sizes.h>
#include <asm/ns16550.h>
#include <pbl.h>
#include <pbl/handoff-data.h>
#include <fdt.h>

static void virt_ns16550_putc(void *base, int ch)
{
	early_ns16550_putc(ch, base, 0, readb, writeb);
}

static void virt_ns16550_init(void)
{
	void __iomem *base = IOMEM(0x10000000);

	early_ns16550_init(base, 3686400 / CONFIG_BAUDRATE, 0, writeb);
	pbl_set_putc(virt_ns16550_putc, base);
}

static const struct fdt_device_id console_ids[] = {
	{ .compatible = "riscv-virtio", .data = virt_ns16550_init },
	{ /* sentinel */ }
};

/* called from assembly */
void __noreturn dt_2nd_riscv(unsigned long hartid, unsigned long _fdt);

void __noreturn dt_2nd_riscv(unsigned long hartid, unsigned long _fdt)
{
	unsigned long membase, memsize, endmem, endfdt, uncompressed_len;
	struct fdt_header *fdt = (void *)_fdt;
	void (*pbl_uart_init)(void);

	/* entry point already set up stack */

	if (!fdt)
		hang();

	/*
	 * We need to call this here, as a multiplatform build
	 * depends on querying mode for riscv_vendor_id()
	 */
	riscv_set_flags(RISCV_S_MODE);

	relocate_to_current_adr();
	setup_c();

	pbl_uart_init = fdt_device_get_match_data(fdt, "/", console_ids);
	if (pbl_uart_init) {
		pbl_uart_init();
		putchar('>');
	}

	fdt_find_mem(fdt, &membase, &memsize);
	endmem = membase + memsize;
	endfdt = _fdt + be32_to_cpu(fdt->totalsize);

	/*
	 * QEMU likes to place the FDT at the end of RAM, where barebox
	 * would normally extract itself to. Accommodate this by moving
	 * memory end, so it doesn't overlap FDT. The FDT will be copied
	 * into the handoff data following the uncompressed image, so
	 * account for that as well.
	 */
	uncompressed_len = input_data_len() + sizeof(struct handoff_data_entry) +
		ALIGN(be32_to_cpu(fdt->totalsize), 8);

	if (riscv_mem_barebox_image(membase, endmem, uncompressed_len, NULL) < endfdt &&
	    _fdt < riscv_mem_stack_top(membase, endmem))
		memsize = ALIGN_DOWN(_fdt - membase, SZ_1M);

	barebox_riscv_supervisor_entry(membase, memsize, hartid, fdt);
}
