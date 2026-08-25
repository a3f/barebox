/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: 2012 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix */

#include <linux/sizes.h>
#include <asm/barebox.lds.h>
#include <asm/image.h>
#include <asm-generic/memory_layout.h>
#include <asm-generic/pointer.h>

/*
 * The size of the PE/COFF section that covers the barebox image, which
 * runs from _stext to _edata, must be a round multiple of the PE/COFF
 * FileAlignment, which we set to its minimum value of 0x200, so padding
 * out _edata to a 0x200 aligned boundary should be sufficient.
 */
#ifdef CONFIG_EFI_STUB
#define PECOFF_EDATA_PADDING   \
	.pecoff_edata_padding : { BYTE(0); . = ALIGN(PECOFF_FILE_ALIGNMENT); }
#define PECOFF_SECTION_ALIGN	. = ALIGN(PECOFF_SECTION_ALIGNMENT);
#else
#define PECOFF_EDATA_PADDING
#define PECOFF_SECTION_ALIGN
#endif

OUTPUT_ARCH(BAREBOX_OUTPUT_ARCH)
OUTPUT_FORMAT(BAREBOX_OUTPUT_FORMAT)
SECTIONS
{
	. = 0x0;

	.image_start : { *(.__image_start) }

	. = ALIGN(4);
	._text : { *(._text) }
	.text      :
	{
		_stext = .;
		*(.text_head_entry*)
		__bare_init_start = .;
		*(.text_bare_init*)
		__bare_init_end = .;
		*(.text*)
	}

	BAREBOX_BARE_INIT_SIZE
	BAREBOX_PBL_SIZE

	. = ALIGN(4);
	__start_rodata = .;
	.rodata : { *(.rodata*) }

	. = ALIGN(ASM_SZPTR);
	__ctors_start = .;
	.init_array : {
		KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
		KEEP(*(.init_array))
	}
	__ctors_end = .;

	.barebox_imd : { BAREBOX_IMD }

	PECOFF_SECTION_ALIGN
	__end_rodata = .;
	_etext = .;
	_sdata = .;

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

	PECOFF_EDATA_PADDING

	__pecoff_data_rawsize = ABSOLUTE(. - _etext);

	/* .bss is dwarfed by piggydata size, so we just handle .bss
	 * as normal PE data
	 */

	__pecoff_data_size = ABSOLUTE(. - _etext);

	.image_end : { KEEP(*(.__image_end)) }

	pbl_image_size =  .;

	_barebox_image_size = __image_end;
	_barebox_pbl_size = __bss_start;
}
