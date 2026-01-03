/* SPDX-License-Identifier: GPL-2.0-only */

#include <asm-generic/pointer.h>

        .section .piggydata,"a"
        .globl  input_data
	.p2align ASM_LGPTR
input_data:
        .incbin "images/barebox.z"
        .globl  input_data_end
input_data_end:
