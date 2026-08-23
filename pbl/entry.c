// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Ahmad Fatoum
/*
 * C implementation of the v1 PBL head: the stage dispatch
 * pbl_head_start_v1() the image head branches to and its continuation
 * pbl_head_reloc_v1().
 *
 * pbl_head_start_v1() sets up the C environment in the first place, so
 * two sets of rules apply before it is done:
 *
 * - __prereloc code runs unrelocated: no global variables and every address
 *   of a function or linker symbol needs the runtime offset added by hand.
 *
 * - __unsafe_prestack code runs before the stack is selected and must not
 *   spill.
 *
 * The naked attribute keeps the compiler from emitting a prologue, but it
 * remains free to spill, so doing this in C is fragile. This is therefore
 * weak and serves as reference and as fallback for architectures that
 * don't override it in their head file.
 */

#include <linux/types.h>
#include <linux/align.h>
#include <asm/reloc.h>
#include <asm/sections.h>
#include <asm-generic/registers.h>
#include <barebox.h>
#include <pbl/entry.h>
#include <pbl/soc-type.h>
#include <pbl/handoff-data.h>

/*
 * runtime_address() for code running before the stack is set up: its
 * volatile temporary, which defeats rematerialization across relocation,
 * would cost a stack slot. Nothing running this early relocates, so
 * RELOC_HIDE() alone suffices.
 */
#define prestack_runtime_address(sym) \
	((void *)RELOC_HIDE((char *)(sym), global_variable_offset()))

static struct pbl_stage *current_stage;

struct pbl_stage *pbl_get_stage(void)
{
	return current_stage;
}

static __always_inline __prereloc __unsafe_prestack bool
stage_has_entry(const struct pbl_stage *stage)
{
	return stage->soc_init || stage->board_init || stage->soc_chainload;
}

static __always_inline __prereloc __unsafe_prestack int
pbl_determine_stage_idx(struct pbl_board_info *pbi)
{
	int earliest_stage_idx = -1;
	int stage_idx;

	if (pbi->assembly_locate_pc) {
		pbl_locate_pc_fn_t locate_pc;

		locate_pc = (void *)((ulong)pbi->assembly_locate_pc +
				     get_runtime_offset());
		return locate_pc();
	}

	for (stage_idx = 0; stage_idx < PBL_STAGE_COUNT; stage_idx++) {
		struct pbl_stage *stage = pbl_stage(pbi, stage_idx);

		if (!stage)
			continue;

		if (stage_has_entry(stage) && earliest_stage_idx < 0)
			earliest_stage_idx = stage_idx;

		if (pc_inside(stage->mem_start,
			      stage->mem_maxsz ?: stage->mem_minsz))
			return stage_idx;
	}

	return earliest_stage_idx;
}

static void __noreturn pbl_head_cpu_init_v1(ulong stage_ul, ulong entry_data);
static void __noreturn pbl_head_entry_v1(struct pbl_board_info *, ulong);

/*
 * Runs without a stack up to the switch in call_with_stack(), so the
 * assembly_locate_pc() callback must be a leaf function that doesn't
 * spill, as the assembly heads require of it anyway.
 */
void __weak __pbl_naked __prereloc __unsafe_prestack
pbl_head_start_v1(unsigned long entry_data)
{
	struct pbl_board_info *pbi =
		prestack_runtime_address(&__pbl_board_info);
	struct pbl_stage *stage;
	ulong stage_ul;
	ulong stacktop;

	stage = pbl_stage(pbi, pbl_determine_stage_idx(pbi));
	if (!stage)
		__hang();

	stage_ul = (ulong)stage;
	stacktop = stage->stacktop;

	if (stacktop == PBL_STACKTOP_BEFORE_BAREBOX)
		stacktop = (ulong)prestack_runtime_address(__image_start);
	else if (stacktop == PBL_STACKTOP_AFTER_BAREBOX)
		stacktop = (ulong)prestack_runtime_address(__image_end) +
			CONFIG_STACK_SIZE;

	stacktop = ALIGN_DOWN(stacktop, 16);

	if (stacktop)
		call_with_stack(stacktop, pbl_head_cpu_init_v1,
				stage_ul, entry_data);
	else
		pbl_head_cpu_init_v1(stage_ul, entry_data);
}

/*
 * Mirrors the CPU init call in the assembly heads: runs first thing on
 * the selected stack, but still unrelocated, so the callback pointer
 * read from the board info needs the runtime offset added.
 */
static void __noreturn __prereloc
pbl_head_cpu_init_v1(ulong stage_ul, ulong entry_data)
{
	struct pbl_stage *stage = (void *)stage_ul;

	if (stage && stage->prereloc_cpu_init) {
		pbl_board_fn_t cpu_init;

		cpu_init = (void *)((ulong)stage->prereloc_cpu_init +
				    get_runtime_offset());
		cpu_init();
	}

	pbl_head_reloc_v1(stage_ul, entry_data);
}

void __prereloc noinline
pbl_head_reloc_v1(ulong stage_ul, ulong entry_data)
{
	struct pbl_stage *stage = (void *)stage_ul;

	if (!stage)
		__hang();

	if (stage->relocaddr)
		relocate_to_adr_full(stage->relocaddr);
	else
		relocate_to_current_adr();

	setup_c();

	pbl_head_entry_v1(&__pbl_board_info, entry_data);
}

static void noinline
pbl_head_entry_v1(struct pbl_board_info *pbi, ulong entry_data)
{
	struct pbl_stage *stage;
	int stage_idx;

	stage_idx = pbl_determine_stage_idx(pbi);
	stage = pbl_stage(pbi, stage_idx);
	if (!stage)
		hang();

	/* A multi-SoC image may still override this from its board init */
	pbl_set_soc_type(pbi->soc_type);
	pbl_handoff_soc_type();

	current_stage = stage;

	if (stage->soc_init)
		stage->soc_init();
	if (stage->board_init)
		stage->board_init(entry_data);
	if (stage->soc_chainload)
		stage->soc_chainload(stage->board_data);

	hang();
}
