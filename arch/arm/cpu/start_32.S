/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/linkage.h>

/*
 * First function in the uncompressed image. We get here from
 * the pbl. The stack already has been set up by the pbl.
 */
.section .text.entry, "ax"
ENTRY(start)
	push	{r0, r1, r2, r3}

#ifdef CONFIG_CPU_V7
	bl	armv7_hyp_install
#endif
	bl	relocate_to_current_adr
	/* BSS was zeroed by PBL's ELF loader */

	pop	{r0, r1, r2, r3}
	b	barebox_non_pbl_start
ENDPROC(start)
