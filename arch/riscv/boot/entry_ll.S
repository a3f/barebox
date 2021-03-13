/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/linkage.h>
#include <asm/sections.h>

/*
 * a0: memory base
 * a1: memory size
 * a2: board data
 * a3: new value for SP
 */
.section .text.__barebox_riscv_entry
ENTRY(__barebox_riscv_entry)
	move	sp, a3
	j	barebox_pbl_start
ENDPROC(__barebox_riscv_entry)
