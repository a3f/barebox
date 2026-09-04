/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/linkage.h>
#include <asm/image.h>

#if __riscv_xlen == 64
#define IMAGE_LOAD_OFFSET 0x200000 /* Image load offset (2MB) from start of RAM */
#else
#define IMAGE_LOAD_OFFSET 0x400000 /* Image load offset (4MB) from start of RAM */
#endif

/*
 * Custom head for barebox-dt-2nd.img: keep the Linux RISC-V kernel image
 * header, so the image is bootable everywhere a kernel image is, then
 * continue in the supervisor mode PBL head. The stack grows down from
 * the image start into the load offset requested above.
 */
.section .text_image_header_dt_2nd, "ax"
ENTRY(__dt_2nd_head)
	/* Relaxation and compressed instructions would shift the fields */
	.option push
	.option norelax
	.option norvc
	nop				/* code0 */
	j	1f			/* code1 */
	.balign	8
	.dword	IMAGE_LOAD_OFFSET	/* Image load offset from start of RAM */
	.dword	_barebox_image_size	/* Effective Image size */
	.dword	__HEAD_FLAGS		/* Kernel flags */
	.word	RISCV_HEADER_VERSION	/* version */
	.word	0			/* reserved */
	.dword	0			/* reserved */
	.ascii	RISCV_IMAGE_MAGIC	/* magic 1 */
	.ascii	RISCV_IMAGE_MAGIC2	/* magic 2 */
	.word	0			/* reserved (PE-COFF offset) */
	.option pop
1:
	j	__barebox_pbl_head_v1_supervisor
ENDPROC(__dt_2nd_head)
