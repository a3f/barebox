/* SPDX-License-Identifier: GPL-2.0-only */

#include <linux/linkage.h>

.section .text.call_with_stack
ENTRY(call_with_stack)
	mv	t1, a1
	mv	sp, a0
	mv	a0, a2
	mv	a1, a3
	jr	t1
ENDPROC(call_with_stack)
