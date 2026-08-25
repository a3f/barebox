/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/linkage.h>
#include <asm/image.h>
#include "efi-header.S"

#if __riscv_xlen == 64
#define IMAGE_LOAD_OFFSET 0x200000 /* Image load offset(2MB) from start of RAM */
#else
#define IMAGE_LOAD_OFFSET 0x400000 /* Image load offset(4MB) from start of RAM */
#endif

/*
 * Image header shared with the Linux kernel, so the image can be started
 * anywhere a kernel could be. With CONFIG_EFI_STUB, a PE/COFF header
 * follows, so the image can additionally be loaded as an EFI application.
 */
.section .text_head_entry_start_dt_2nd, "ax"
ENTRY(start_dt_2nd)
	/*
	 * The headers have a fixed layout, which must not be disturbed
	 * by the linker deleting alignment padding during relaxation
	 */
	.option push
	.option norelax
	efi_signature_nop		/* code0 */
	j	1f			/* code1 */
	.balign	8
	.dword	IMAGE_LOAD_OFFSET	/* Image load offset from start of RAM */
	.dword	_barebox_image_size	/* Effective Image size */
	.dword	__HEAD_FLAGS		/* Kernel flags */
	.word	RISCV_HEADER_VERSION	/* version */
	.word	0			/* reserved */
	.dword	0			/* reserved */
	.ascii	RISCV_IMAGE_MAGIC	/* magic 1 */
	.balign	4
	.ascii	RISCV_IMAGE_MAGIC2	/* magic 2 */
	.word	.Lpe_header_offset	/* reserved (PE-COFF offset) */

	__EFI_PE_HEADER
	.option pop
1:
	/*
	 * Because we can depend on being loaded at an offset, we can just
	 * use our load address as stack top. The stack then grows into
	 * the image load offset specified above. This is more than enough
	 * until the FDT /memory node is decoded.
	 */
	lla	sp, start_dt_2nd
	li	fp, 0
	/* a0 = hartid, a1 = FDT, as passed by the previous boot stage */
	tail	dt_2nd_riscv
ENDPROC(start_dt_2nd)
