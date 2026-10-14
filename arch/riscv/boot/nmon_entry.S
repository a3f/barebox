/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/linkage.h>
#include <asm/sections.h>

#include "asm/debug_ll.h"
#include "asm/riscv_nmon.h"

.section .text_bare_init

ENTRY(__barebox_nmon_entry)
	/* nmon uses s1-s3, keep them in registers it leaves alone */
	mv	a4, s1
	mv	a5, s2
	mv	a6, s3
	riscv_nmon
	mv	s1, a4
	mv	s2, a5
	mv	s3, a6
	ret
ENDPROC(__barebox_nmon_entry)
