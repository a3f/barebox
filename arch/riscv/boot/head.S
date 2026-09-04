/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/linkage.h>
#include <asm/asm.h>
#include <asm/asm-offsets.h>

/* Linker will point this at the selected board's struct pbl_board_info */
.globl __pbl_board_info

/*
 * __barebox_pbl_head_v1_supervisor - v1 PBL image head for images that
 * firmware enters like a Linux kernel, i.e. in supervisor mode with the
 * hart ID in a0 and the device tree in a1
 *
 * The privilege mode is part of the RISC-V boot protocol and code as
 * early as the relocation's cache maintenance asks for it, so the head
 * has to record it: it writes sscratch as riscv_set_flags() would, keeps
 * the hart ID in tp where riscv_hartid() expects it and passes the device
 * tree to pbl_head_start_v1() as entry_data. Both stay in registers, so
 * executing in place from read-only memory works.
 *
 * The header layout is the one __barebox_riscv_head() emits, so
 * is_barebox_riscv_head() and the image tools are unaffected.
 */
.section .text_head_prologue_v1, "ax"
ENTRY(__barebox_pbl_head_v1_supervisor)
	/* Relaxation and compressed instructions would shift the fields */
	.option push
	.option norelax
	.option norvc
	nop				/* code0 */
	j	.Lsetup			/* code1 */
	.balign	8
	.dword	0			/* Image load offset from start of RAM */
	.dword	_barebox_image_size	/* Effective Image size */
	.dword	0			/* Kernel flags */
	.word	0			/* version */
	.word	0			/* reserved */
	.dword	0			/* reserved */
	.asciz	"barebox"		/* magic 1 */
	.balign	8
	.ascii	"RSCV"			/* magic 2 */
	.word	0			/* reserved (PE-COFF offset) */
	.option pop

.Lsetup:
	li	fp, 0

	/* Supervisor mode handoff, see above */
	li	t0, RISCV_S_MODE_ASM
	csrw	sscratch, t0
	mv	tp, a0
	mv	a0, a1			/* entry_data = device tree */
	tail	pbl_head_start_v1
ENDPROC(__barebox_pbl_head_v1_supervisor)

/*
 * pbl_head_start_v1 - RISC-V stage dispatch, overriding the weak C one
 * in pbl/entry.c: determine the executing stage from struct
 * pbl_board_info, set up its stack and hand over to the C continuation.
 *
 * Register use before the pbl_head_reloc_v1() call:
 *   a0  = entry_data on entry, then struct pbl_board_info *
 *   a1  = struct pbl_stage * (may be NULL)
 *   s1  = entry_data; callee-saved, so it survives the callbacks
 *   s2  = runtime offset, likewise
 *   s3  = struct pbl_stage *, kept across the prereloc_cpu_init() call
 *   t0-t6 = scratch
 */
ENTRY(pbl_head_start_v1)
	mv	s1, a0

	/* Board info pointers need the runtime offset until relocation ran */
	call	get_runtime_offset
	mv	s2, a0

	lla	a0, __pbl_board_info

	/* Stage determination, mirroring pbl_determine_stage_idx() */
	REG_L	t0, PBI_LOCATE_PC(a0)
	beqz	t0, .Lscan

	/* Must be a leaf function: there may be no stack yet */
	add	t0, t0, s2
	jalr	t0			/* returns stage index in a0 */
	mv	t1, a0			/* idx */
	lla	a0, __pbl_board_info
	li	a1, 0			/* pbl_stage(): NULL by default */

	/*
	 * pbl_stage_case - one case of the pbl_stage() switch; the cases
	 * are mutually exclusive, so each falls through to the next.
	 */
	.macro	pbl_stage_case idx, offs
	li	t2, \idx
	bne	t1, t2, .Lnext_case\@
	addi	a1, a0, \offs
.Lnext_case\@:
	.endm

	pbl_stage_case	PBL_STAGE_ROM_ASM, PBI_ROM_STAGE
	pbl_stage_case	PBL_STAGE_OCRAM_ASM, PBI_FIRST_STAGE
	pbl_stage_case	PBL_STAGE_DRAM_ASM, PBI_SECOND_STAGE
	j	.Lhave_stage		/* a1 stays NULL for other indices */

.Lscan:
	li	a1, 0			/* earliest stage with entry */
	auipc	t3, 0			/* pc, as get_pc() does */

	/*
	 * pbl_check_stage - one pbl_determine_stage_idx() iteration for the
	 * stage at byte offset \offs
	 */
	.macro	pbl_check_stage offs
	addi	t0, a0, \offs
	/* stage_has_entry()? */
	REG_L	t4, PBS_SOC_INIT(t0)
	REG_L	t5, PBS_BOARD_INIT(t0)
	or	t4, t4, t5
	REG_L	t5, PBS_SOC_CHAINLOAD(t0)
	or	t4, t4, t5
	beqz	t4, .Lcheck_pc\@
	bnez	a1, .Lcheck_pc\@
	mv	a1, t0			/* earliest ?: stage */
.Lcheck_pc\@:
	/* pc_inside(mem_start, mem_maxsz ?: mem_minsz)? */
	REG_L	t4, PBS_MEM_START(t0)
	REG_L	t5, PBS_MEM_MAXSZ(t0)
	bnez	t5, .Lhave_memsz\@
	REG_L	t5, PBS_MEM_MINSZ(t0)
.Lhave_memsz\@:
	sub	t4, t3, t4
	bltu	t4, t5, .Lfound
	.endm

	pbl_check_stage	PBI_ROM_STAGE
	pbl_check_stage	PBI_FIRST_STAGE
	pbl_check_stage	PBI_SECOND_STAGE
	j	.Lhave_stage		/* stage = earliest (a1) */
.Lfound:
	mv	a1, t0

.Lhave_stage:
	/* Misconfigured board info; there may be no stack to call with */
.Lno_stage:
	beqz	a1, .Lno_stage

	/* stage->stacktop is a stack top or a PBL_STACKTOP_* value */
	REG_L	t0, PBS_STACKTOP(a1)
	li	t1, PBS_STACKTOP_BEFORE_BAREBOX
	bne	t0, t1, .Lnot_before_barebox
	lla	t0, __image_start
	j	.Lhave_stacktop
.Lnot_before_barebox:
	li	t1, PBS_STACKTOP_AFTER_BAREBOX
	bne	t0, t1, .Lhave_stacktop
	lla	t0, __image_end
	li	t1, CONFIG_STACK_SIZE
	add	t0, t0, t1
.Lhave_stacktop:
	andi	t0, t0, -16		/* ALIGN_DOWN(stacktop, 16) */
	beqz	t0, .Lstack_set
	mv	sp, t0
.Lstack_set:
	/* CPU init callback, unrelocated like assembly_locate_pc() above */
	REG_L	t0, PBS_CPU_INIT(a1)
	beqz	t0, .Lcpu_init_done
	add	t0, t0, s2
	mv	s3, a1
	jalr	t0
	mv	a1, s3
.Lcpu_init_done:
	mv	a0, a1
	mv	a1, s1
	tail	pbl_head_reloc_v1	/* (a0 = stage, a1 = entry_data) */
ENDPROC(pbl_head_start_v1)
