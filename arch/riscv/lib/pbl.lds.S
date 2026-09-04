/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: 2012 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix */

#include <linux/sizes.h>
#include <asm/barebox.lds.h>
#include <asm-generic/memory_layout.h>
#include <asm-generic/pointer.h>
#include <asm/asm-offsets.h>

OUTPUT_ARCH(BAREBOX_OUTPUT_ARCH)
OUTPUT_FORMAT(BAREBOX_OUTPUT_FORMAT)
SECTIONS
{
	. = 0x0;

	.image_start : { *(.__image_start) }

	._text : { *(._text) }
	.text      :
	{
		_stext = .;
		/*
		 * Per-image headers branching into one of the common
		 * heads below, kept apart from the prologue sections so
		 * they win the image start regardless of link order.
		 */
		*(.text_image_header*)
		*(.text_head_prologue*)
		*(.text_head_entry*)
		__bare_init_start = .;
		*(.text_bare_init*)
		__bare_init_end = .;
		*(.text*)
	}

	BAREBOX_BARE_INIT_SIZE
	BAREBOX_PBL_SIZE

	. = ALIGN(ASM_SZPTR);
	__start_rodata = .;
	__pbl_board_info = .;
	.rodata.pbl_board_info : {
		*(.pbl_board_info_*)
	}
	ASSERT(. - __pbl_board_info <= PBI_SIZEOF, "Only one PBL per image allowed")

	. = ALIGN(4);
	.rodata : { *(.rodata*) }

	. = ALIGN(ASM_SZPTR);
	__ctors_start = .;
	.init_array : {
		KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
		KEEP(*(.init_array))
	}
	__ctors_end = .;

	.barebox_imd : { BAREBOX_IMD }

	__end_rodata = .;
	_etext = .;

	.data : { *(.data*) }

	__shasum_start = .;
	.shasum : {
		KEEP(*(.shasum))
	}
	__shasum_end = .;

	/DISCARD/ : { *(.rela.plt*) }
	.rela.dyn : {
		__rel_dyn_start = .;
		*(.rela*)
		__rel_dyn_end = .;
	}

	.dynsym : {
		__dynsym_start = .;
		*(.dynsym)
		__dynsym_end = .;
	}

	pbl_code_size =  .;

	.__bss_start :  { *(.__bss_start) }
	.bss : { *(.bss*) }
	.__bss_stop :  { *(.__bss_stop) }
	_end = .;

	pbl_memory_size =  .;

	. = ALIGN(4);
	__piggydata_start = .;
	.piggydata : {
		*(.piggydata)
	}
	.image_end : { KEEP(*(.__image_end)) }

	pbl_image_size =  .;

	_barebox_image_size = __image_end;
	_barebox_pbl_size = __bss_start;

	ASSERT(_stext == __image_start, "PBL entry code must be at the image start")
}
