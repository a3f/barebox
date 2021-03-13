/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2021 Ahmad Fatoum, Pengutronix */
#include <linux/linkage.h>
#include <asm/image.h>

.section .text_head_entry_start_dt_2nd
ENTRY(start_dt_2nd)
	auipc sp, 0                /* code0 */
	j dt_2nd_riscv             /* code1 */
	.balign 8
#if __riscv_xlen == 64
	.dword 0x200000            /* Image load offset(2MB) from start of RAM */
#else
	.dword 0x400000            /* Image load offset(4MB) from start of RAM */
#endif
	.dword _barebox_image_size /* Effective Image size */
	.dword 0                   /* Kernel flags */
	.word RISCV_HEADER_VERSION /* version */
	.word 0                    /* reserved */
	.dword 0                   /* reserved */
	.ascii RISCV_IMAGE_MAGIC   /* magic1 */
	.balign 4
	.ascii RISCV_IMAGE_MAGIC2  /* magic2 */
	.word   0                  /* reserved (PE-COFF offset) */
	.asciz "barebox"           /* unused for now */
END(start_dt_2nd)
