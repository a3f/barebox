// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2010 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix

#include <linux/compiler.h>
#include <string.h>
#include <barebox.h>
#include <elf.h>
#include <debug_ll.h>
#include <asm/reloc.h>

/*
 * relocate binary to the currently running address
 */
void __prereloc relocate_image(unsigned long offset,
			       void *dstart, void *dend,
			       long *dynsym, long *dynend)
{
	while (dstart < dend) {
		struct elf64_rela *rel = dstart;
		unsigned long *fixup;

		switch(ELF64_R_TYPE(rel->r_info)) {
		case R_AARCH64_RELATIVE:
			fixup = (unsigned long *)(rel->r_offset + offset);

			*fixup = rel->r_addend + offset;
			rel->r_addend += offset;
			rel->r_offset += offset;
			break;
		case R_ARM_NONE:
			break;
		default:
			putc_ll('>');
			puthex_ll(rel->r_info);
			putc_ll(' ');
			puthex_ll(rel->r_offset);
			putc_ll(' ');
			puthex_ll(rel->r_addend);
			putc_ll('\n');
			__hang();
		}

		dstart += sizeof(*rel);
	}
}

/*
 * Apply RELR relocations.
 *
 * RELR is a compressed format for storing relative relocations.
 * The encoded sequence of entries looks like:
 *   [ AAAAAAAA BBBBBBB1 BBBBBBB1 ... AAAAAAAA BBBBBB1 ... ]
 *
 * i.e. start with an address, followed by any number of bitmaps. The
 * address entry encodes 1 relocation. The subsequent bitmap entries
 * encode up to 63 relocations each, at subsequent offsets following
 * the last address entry.
 *
 * The bitmap entries must have 1 in the least significant bit. The
 * assumption here is that an address cannot have 1 in lsb. Odd
 * addresses are not supported. Any odd addresses are stored in the
 * RELA section, which is handled separately.
 */
void __prereloc relocate_relr(unsigned long offset,
			      const void *start, const void *end)
{
	const u64 *relr;
	u64 *place = NULL;

	for (relr = start; relr < (const u64 *)end; relr++) {
		if ((*relr & 1) == 0) {
			place = (u64 *)(*relr + offset);
			*place++ += offset;
		} else {
			u64 *p = place;
			u64 r = *relr >> 1;

			for (; r; p++, r >>= 1)
				if (r & 1)
					*p += offset;
			place += 63;
		}
	}
}

/*
 * Apply ARM64 ELF relocations
 */
int elf_apply_relocations(struct elf_image *elf, const void *dyn_seg)
{
	void *rela_ptr = NULL, *relr_ptr = NULL, *symtab = NULL;
	u64 relasz, relrsz;
	phys_addr_t base = (phys_addr_t)elf->reloc_offset;
	bool have_rela, have_relr;

	have_rela = !elf_parse_dynamic_section_rela(elf, dyn_seg,
						    &rela_ptr, &relasz, &symtab);
	have_relr = !elf_parse_dynamic_section_relr(elf, dyn_seg,
						    &relr_ptr, &relrsz);

	if (!have_rela && !have_relr)
		return -EINVAL;

	if (have_rela)
		relocate_image(base, rela_ptr, rela_ptr + relasz, symtab, NULL);

	if (have_relr)
		relocate_relr(base, relr_ptr, relr_ptr + relrsz);

	return 0;
}
