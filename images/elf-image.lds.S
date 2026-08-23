/* SPDX-License-Identifier: GPL-2.0-only */

#include <asm/barebox.lds.h>

OUTPUT_FORMAT(BAREBOX_OUTPUT_FORMAT)
OUTPUT_ARCH(BAREBOX_OUTPUT_ARCH)
ENTRY(BAREBOX_ELF_LOADADDR)

SECTIONS
{
	. = BAREBOX_ELF_LOADADDR;

	/* ld -b binary places the input blob into a section named .data */
	.text : { *(.data) }
}
