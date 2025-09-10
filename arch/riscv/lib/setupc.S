/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: Copyright (c) 2021 Ahmad Fatoum, Pengutronix */

#include <linux/linkage.h>
#include <asm/sections.h>
#include <asm/asm.h>

/*
 * void __memcpy_exec(void *dst, void *src, size_t size)
 *
 * Copy executable memory region from src to dst while doing cache manintenance.
 */
.section .text.__memcpy_exec
ENTRY(__memcpy_exec)
	addi	sp, sp, -SZREG * 2
	REG_S	s1, (sp)
	REG_S	ra, SZREG(sp)

	mv	s1, a2		/* s1: size */
	jal	__memcpy
				/* a0: dst */
	mv	a1, s1		/* a1: size */
	jal	sync_caches_for_execution

	REG_L	ra, SZREG(sp)
	REG_L	s1, (sp)
	addi	sp, sp, SZREG * 2
	ret
ENDPROC(__memcpy_exec)

/*
 * setup_c: clear bss
 */
.section .text.setup_c
ENTRY(setup_c)
	lla	a0, __bss_start
	li	a1, 0
	lla	a2, __bss_stop
	sub	a2, a2, a0
	j	__memset
ENDPROC(setup_c)

/*
 * void relocate_to_adr(unsigned long targetadr)
 *
 * Copy binary to targetadr, relocate code and continue
 * executing at new address.
 */
.section .text.relocate_to_adr
ENTRY(relocate_to_adr)
					/* a0: target address */
	addi	sp, sp, -SZREG * 2
	lla	a1, _text		/* a1: source address */

	/* adjust return address */
	sub	ra, ra, a1		/* sub address where we are actually running */
	add	ra, ra, a0		/* add address where we are going to run */
	REG_S	ra, SZREG(sp)

	beq	a0, a1, copied		/* skip if already at new address */

	lla	a2, copied
	sub	a2, a2, a1
	add	a2, a2, a0
	REG_S	a2, (sp)

	lla	a2, __bss_start
	sub	a2, a2, a1		 /* a2: size */

	jal	__memcpy_exec

	REG_L	a0, (sp)
	jr	a0 			/* jump to relocated address */
copied:
	REG_L	ra, SZREG(sp)
	addi	sp, sp, SZREG * 2
	j	relocate_to_current_adr	/* relocate binary */
ENDPROC(relocate_to_adr)
