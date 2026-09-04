// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <asm/sections.h>
#include <linux/sizes.h>
#include <asm/ns16550.h>
#include <asm/barebox-riscv.h>
#include <pbl.h>
#include <pbl/entry.h>
#include <fdt.h>

static void virt_ns16550_putc(void *base, int ch)
{
	early_ns16550_putc(ch, IOMEM(base), 0, readb, writeb);
}

static void virt_ns16550_setup(void *base)
{
	early_ns16550_init(IOMEM(base), 3686400 / CONFIG_BAUDRATE, 0, writeb);
}

static struct pbl_console riscv_virtio_console = {
	.setup = virt_ns16550_setup,
	.arg = (void *)0x10000000,
	.driver = &(struct pbl_console_driver) { virt_ns16550_putc }
};

static const struct fdt_device_id console_ids[] = {
	{ .compatible = "riscv-virtio", .data = &riscv_virtio_console },
	{ /* sentinel */ }
};

/* entry_data is the device tree the image was started with, if any */
static void dt_2nd_bdinit(unsigned long entry_data)
{
	struct fdt_header *fdt = (void *)entry_data;
	const struct pbl_console *console;

	if (!fdt)
		hang();

	console = fdt_device_get_match_data(fdt, "/", console_ids);
	if (console)
		__pbl_board_info.console = *console;

	pbl_get_stage()->board_data = fdt;
}

static void __noreturn dt_2nd_chainload(void *boarddata)
{
	unsigned long membase, memsize, endmem, endfdt, barebox_size;
	unsigned long boarddata_ul = (unsigned long)boarddata;
	struct fdt_header *fdt = boarddata;

	fdt_find_mem(fdt, &membase, &memsize);
	endmem = membase + memsize;
	endfdt = boarddata_ul + be32_to_cpu(fdt->totalsize);

	/*
	 * QEMU likes to place the FDT at the end of RAM, where barebox
	 * would normally extract itself to. Accommodate this by moving
	 * memory end, so it doesn't overlap FDT
	 */
	barebox_size = pbl_barebox_memsz(input_data_len());

	/* The FDT is handed over behind barebox proper, so account for it */
	handoff_data_add_dt(fdt);

	if (riscv_mem_barebox_image(membase, endmem, barebox_size, NULL) < endfdt &&
	    boarddata_ul < riscv_mem_stack_top(membase, endmem))
		memsize = ALIGN_DOWN(boarddata_ul - membase, SZ_1M);

	/* The head has recorded the mode and stashed the hart ID in tp already */
	barebox_riscv_entry(membase, memsize, NULL, RISCV_S_MODE);
}

/*
 * Generic second stage entry point: the head in board-dt-2nd-head.S
 * provides the kernel image header and branches into the supervisor mode
 * PBL head, which records the device tree as entry data.
 *
 * Which UART is the console depends on the machine, so no console is
 * described; the board init callback looks it up by root compatible.
 */
extern char __dt_2nd_head[];

ENTRY_POINT_HEAD(start_dt_2nd, __dt_2nd_head)
	.soc_type = SOC_TYPE_GENERIC_DT,
	.second_stage.stacktop = PBL_STACKTOP_BEFORE_BAREBOX,
	.second_stage.board_init = dt_2nd_bdinit,
	.second_stage.soc_chainload = dt_2nd_chainload,
ENTRY_POINT_END
