/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: 2026 Ahmad Fatoum */

#ifndef __PBL_ENTRY_H_
#define __PBL_ENTRY_H_

#include <linux/bits.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <asm-generic/memory_layout.h>
#include <pbl/soc-type.h>

/* Special stacktop values; a real stack top is at least 16-byte aligned */
#define PBL_STACKTOP_KEEP		0	/* leave SP as it is */
#define PBL_STACKTOP_BEFORE_BAREBOX	1	/* grows down from the image start */
#define PBL_STACKTOP_AFTER_BAREBOX	2	/* CONFIG_STACK_SIZE behind the image end */

enum pbl_stage_idx {
	PBL_STAGE_ROM,		/* pre-sram */
	PBL_STAGE_OCRAM,	/* pre-ram */
	PBL_STAGE_DRAM,		/* some ram */
	PBL_STAGE_FINAL,	/* all ram */
	PBL_STAGE_COUNT,	/* sentinel */
};

typedef void (*pbl_board_fn_t)(void);
typedef enum pbl_stage_idx (*pbl_locate_pc_fn_t)(void);

/*
 * Stage description. prereloc_cpu_init runs first thing after the head
 * has selected the stack, still unrelocated and before BSS is cleared,
 * like the lowlevel CPU init at the start of an ENTRY_FUNCTION() entry
 * point: register access only, no globals, no console and every address
 * taken through runtime_address(). This is where arm_cpu_lowlevel_init()
 * and programming the architected timer frequency go. Only the stage the
 * head determined on entry has its callback run, so a board entered
 * through more than one stage declares it on each.
 *
 * soc_init, board_init and soc_chainload run relocated
 * with BSS cleared, so the remaining SoC lowlevel initialization may use
 * polled timeouts and the PBL clocksource.
 */
struct pbl_stage {
	unsigned long stacktop;		/* stack top or PBL_STACKTOP_* */
	unsigned long relocaddr;	/* 0: don't relocate to specific addr */

	/*
	 * Memory the stage runs from: mem_minsz is what the board knows to
	 * be available and is handed to barebox proper as its initial
	 * memory, mem_maxsz the largest the SoC's memory window can be and
	 * is only used to locate the stage by PC. It defaults to mem_minsz.
	 */
	unsigned long mem_start;
	unsigned long mem_minsz;
	unsigned long mem_maxsz;

	void *board_data;

	pbl_board_fn_t prereloc_cpu_init;
	pbl_board_fn_t soc_init;
	void (*board_init)(unsigned long entry_data);
	void (*soc_chainload)(void *board_data);
};

/* Read by assembly before relocation, via adr, like the stages */
struct pbl_board_info {
	/*
	 * enum soc_type, usually from the SoC's PBL_SOC_<soc>_DEFAULTS. An
	 * image running on more than one SoC updates it from its board init
	 * callback with pbl_set_soc_type().
	 */
	unsigned int soc_type;

	pbl_locate_pc_fn_t assembly_locate_pc;

	struct pbl_stage rom_stage;	/* PBL_STAGE_ROM */
	struct pbl_stage first_stage;	/* PBL_STAGE_OCRAM */
	struct pbl_stage second_stage;	/* PBL_STAGE_DRAM */
};

extern struct pbl_board_info __pbl_board_info;

/*
 * pbl_stage - return the stage descriptor for an enum pbl_stage_idx, or
 * NULL if there is none. Always inlined, as it runs before relocation.
 */
static __always_inline struct pbl_stage *
pbl_stage(struct pbl_board_info *pbi, int idx)
{
	switch (idx) {
	case PBL_STAGE_ROM:
		return &pbi->rom_stage;
	case PBL_STAGE_OCRAM:
		return &pbi->first_stage;
	case PBL_STAGE_DRAM:
		return &pbi->second_stage;
	default:
		return NULL;
	}
}

struct pbl_stage *pbl_get_stage(void);

void setup_c(void);

#ifdef CONFIG_ARCH_HAS_NAKED
#define __pbl_naked __naked
#else
#define __pbl_naked
#endif

/*
 * pbl_head_start_v1 - stage dispatch entered from the image head
 *
 * The architecture's assembly image heads lay out the image header and
 * branch here with a boot argument register as entry_data, which reaches
 * the board init callback. RISC-V has one head per privilege mode, ARM
 * will add a generic __barebox_pbl_head_v1 once it is switched over.
 * Head and dispatch functions are versioned, so an incompatible entry
 * scheme can be added as a v2 next to v1 later on.
 *
 * Runs with nothing set up: no stack, unrelocated, BSS not cleared.
 * Determines the executing stage from struct pbl_board_info, selects its
 * stack, runs prereloc_cpu_init and continues in pbl_head_reloc_v1().
 *
 * pbl/entry.c has a weak C implementation, which is naked where the
 * architecture has naked functions, but may still spill to the stack.
 * Architectures override it with an assembly implementation in their
 * head file.
 */
void __noreturn pbl_head_start_v1(unsigned long entry_data);

/*
 * Continuation for pbl_head_start_v1(): relocates, zeroes BSS and
 * dispatches the stage entry points. Needs a valid stack pointer.
 */
void __noreturn pbl_head_reloc_v1(unsigned long stage,
				  unsigned long entry_data);

/*
 * ENTRY_POINT_HEAD/END - new-style entry point (recommended)
 *
 * The generated entry point function is not meant to be executed: it is
 * the image entry symbol that roots section garbage collection and thus
 * keeps head and board info alive. Once ARM is switched over, its
 * ENTRY_FUNCTION() head will branch to it directly though, so it must not
 * touch the stack before the continuation runs; hence naked where
 * available and __unsafe_prestack.
 */
#define ENTRY_POINT_HEAD(name, head)					\
	void name(ulong r0, ulong r1, ulong r2);			\
	void __pbl_naked __unsafe_prestack				\
		__section(.text_head_entry_##name) name			\
			(ulong r0, ulong r1, ulong r2)			\
	{								\
		__keep_symbolref(head);					\
	__diag_push();							\
	__diag_ignore_all("-Woverride-init",				\
			  "Allow board overrides for SoC defaults");	\
		static __section(.pbl_board_info_##name)		\
			struct pbl_board_info __board_info = {		\
				/* unset fields are zero (static) */

/*
 * Closes the board info definition, but leaves the entry point function
 * open for the continuation ARM's ENTRY_FUNCTION() boards will insert
 * once ARM is switched over.
 */
#define ENTRY_POINT_CONTINUE						\
			};						\
		__diag_pop();						\
		__keep_symbolref(__board_info);

#define ENTRY_POINT_END							\
	ENTRY_POINT_CONTINUE						\
	}


#define PBL_SOC_PASTE2(soc, field)	PBL_SOC_##soc##_##field
#define PBL_SOC_PASTE(soc, field)	PBL_SOC_PASTE2(soc, field)

/*
 * ENTRY_POINT takes head and board info defaults from the SoC, so SoCs
 * needing a different image header or register handoff can substitute
 * their own head.
 *
 * SoC functions meant for PBL_SOC_<soc>_DEFAULTS or as stage callbacks
 * carry the PBL_SOC_<soc>_ prefix.
 * Lowercase names remain the lowlevel API ENTRY_FUNCTION() boards call.
 */
#define ENTRY_POINT(soc, name)				\
	ENTRY_POINT_HEAD(name, PBL_SOC_PASTE(soc, HEAD))\
	PBL_SOC_PASTE(soc, DEFAULTS),

#endif
