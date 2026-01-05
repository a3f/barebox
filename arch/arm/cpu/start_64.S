/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/linkage.h>

/*
 * First function in the uncompressed image. We get here from
 * the pbl. The stack already has been set up by the pbl.
 */
.section .text_entry, "ax"
ENTRY(start)
	stp	x0, x1, [sp, #-16]!
	stp	x2, x3, [sp, #-16]!

	bl	relocate_to_current_adr
	bl	setup_c

	ldp	x2, x3, [sp], #16
	ldp	x0, x1, [sp], #16
	b	barebox_non_pbl_start
ENDPROC(start)
