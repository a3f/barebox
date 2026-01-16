/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _ASM_GENERIC_RELOC_H_
#define _ASM_GENERIC_RELOC_H_

#include <linux/build_bug.h>
#include <linux/compiler.h>

#ifndef global_variable_offset
#define global_variable_offset() get_runtime_offset()
#endif

/*
 * Using sizeof() on incomplete types always fails, so we use GCC's
 * __builtin_object_size() instead. This is the mechanism underlying
 * FORTIFY_SOURCE. &symbol should always be something GCC can compute
 * a size for, even without annotations, unless it's incomplete.
 * The second argument ensures we get 0 for failure.
 */
#define __has_type_complete(sym) __builtin_object_size(&(sym), 2)

#define __has_type_byte_array(sym) (sizeof(*sym) == 1 + __must_be_array(sym))

/*
 * runtime_address() defined below is supposed to be used exclusively
 * with linker defined symbols, e.g. unsigned char input_end[].
 *
 * We can't completely ensure that, but this gets us close enough
 * to avoid most abuse of runtime_address().
 */
#define __is_incomplete_byte_array(sym) \
	(!__has_type_complete(sym) && __has_type_byte_array(sym))

/*
 * While accessing global variables before C environment is setup is
 * questionable, we can't avoid it when we decide to write our
 * relocation routines in C. This invites a tricky problem with
 * this naive code:
 *
 *   var = &variable + global_variable_offset(); relocate_to_current_adr();
 *
 * Compiler is within rights to rematerialize &variable after
 * relocate_to_current_adr(), which is unfortunate because we
 * then end up adding a relocated &variable with the relocation
 * offset once more. We avoid this here by hiding address with
 * RELOC_HIDE. This is required as a simple compiler barrier()
 * with "memory" clobber is not immune to compiler proving that
 * &sym fits in a register and as such is unaffected by the memory
 * clobber. barrier_data(&sym) would work too, but that comes with
 * aforementioned compiler "memory" barrier, that we don't care for.
 *
 * We don't necessarily need the volatile variable assignment when
 * using the compiler-gcc.h RELOC_HIDE implementation as __asm__
 * __volatile__ takes care of it, but the generic RELOC_HIDE
 * implementation has GCC misscompile runtime_address() when not passing
 * in a volatile object. Volatile casts instead of variable assignments
 * also led to miscompilations with GCC v11.1.1 for THUMB2.
 */

#define runtime_address(sym) ({					\
	void *volatile __addrof_sym = (sym);			\
	if (!__is_incomplete_byte_array(sym))			\
		__unsafe_runtime_address();			\
	RELOC_HIDE(__addrof_sym, global_variable_offset());	\
})

/*
 * Above will fail for "near" objects, e.g. data in the same
 * translation unit or with LTO, as the compiler can be smart
 * enough to omit relocation entry and just generate PC relative
 * accesses leading to base address being added twice. We try to
 * catch most of these here by triggering an error when runtime_address()
 * is used with anything that is not a byte array of unknown size.
 */
extern void *__compiletime_error(
	"runtime_address() may only be called on linker defined symbols."
) __unsafe_runtime_address(void);

/*
 * RELR relocation support
 *
 * RELR is a compact format for encoding relative relocations that achieves
 * ~97% space savings compared to RELA. It uses a simple encoding:
 * - Even entries (LSB=0): Base address - apply relocation here
 * - Odd entries (LSB=1): Bitmap - each set bit indicates a relocation
 *
 * This implementation is architecture-agnostic and uses the ELF_CLASS-based
 * types defined in include/elf.h.
 */
#ifdef __BAREBOX__

#include <elf.h>

/*
 * Apply RELR relocations.
 *
 * Each entry is either:
 * - Even (LSB=0): Base address - apply relocation here
 * - Odd (LSB=1): Bitmap - each set bit indicates a relocation at
 *                base + bit_position * word_size
 */
static inline void __prereloc
relocate_relr(unsigned long offset, void *relr, size_t relr_size)
{
	elf_relr_t *entry = relr;
	elf_relr_t *end = (elf_relr_t *)((char *)relr + relr_size);
	elf_relr_t base = 0;

	while (entry < end) {
		elf_relr_t val = *entry++;

		if ((val & 1) == 0) {
			/* Even: base address, apply relocation */
			elf_relr_t *fixup = (elf_relr_t *)(val + offset);
			*fixup += offset;
			base = val + ELF_RELR_WORD_SIZE;
		} else {
			/* Odd: bitmap */
			for (int i = 0; i < ELF_RELR_BITMAP_BITS; i++) {
				if (val & ((elf_relr_t)2 << i)) {
					elf_relr_t *fixup = (elf_relr_t *)(base + i * ELF_RELR_WORD_SIZE + offset);
					*fixup += offset;
				}
			}
			base += ELF_RELR_BITMAP_BITS * ELF_RELR_WORD_SIZE;
		}
	}
}

/*
 * Find and apply RELR relocations by parsing PT_DYNAMIC.
 *
 * This function:
 * 1. Validates the ELF header at the given location
 * 2. Finds the PT_DYNAMIC segment
 * 3. Parses dynamic entries to locate DT_RELR and DT_RELRSZ
 * 4. Converts the RELR virtual address to file offset
 * 5. Applies the RELR relocations
 *
 * @offset: Relocation offset to apply
 * @elf_start: Pointer to the ELF header (e.g., runtime_address(_text) for self-relocation,
 *             or elf->hdr_buf for external ELF)
 */
static inline void __prereloc
relocate_relr_dynamic(unsigned long offset, void *elf_start)
{
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdr;
	Elf_Dyn *dyn;
	void *relr_start = NULL;
	elf_relr_t relr_size = 0;
	int i, j;

	/* Get ELF header at the provided location */
	ehdr = (Elf_Ehdr *)elf_start;

	/* Validate ELF header */
	if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
	    ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
	    ehdr->e_ident[EI_MAG3] != ELFMAG3)
		return;  /* Not an ELF file, no RELR to process */

	/* Find PT_DYNAMIC segment */
	phdr = (Elf_Phdr *)((char *)ehdr + ehdr->e_phoff);
	for (i = 0; i < ehdr->e_phnum; i++) {
		if (phdr[i].p_type == PT_DYNAMIC) {
			dyn = (Elf_Dyn *)((char *)ehdr + phdr[i].p_offset);

			/* Parse dynamic entries to find RELR */
			for (j = 0; j < phdr[i].p_filesz / sizeof(Elf_Dyn); j++) {
				if (dyn[j].d_tag == DT_NULL)
					break;

				if (dyn[j].d_tag == DT_RELR) {
					/* Convert virtual address to file offset */
					elf_relr_t relr_vaddr = dyn[j].d_un.d_val;
					int k;
					for (k = 0; k < ehdr->e_phnum; k++) {
						if (phdr[k].p_type == PT_LOAD &&
						    relr_vaddr >= phdr[k].p_vaddr &&
						    relr_vaddr < phdr[k].p_vaddr + phdr[k].p_memsz) {
							relr_start = (char *)ehdr + phdr[k].p_offset +
								     (relr_vaddr - phdr[k].p_vaddr);
							break;
						}
					}
				} else if (dyn[j].d_tag == DT_RELRSZ) {
					relr_size = dyn[j].d_un.d_val;
				}
			}
			break;
		}
	}

	/* Apply RELR relocations if found */
	if (relr_start && relr_size > 0)
		relocate_relr(offset, relr_start, relr_size);
}

#endif /* __BAREBOX__ */

#endif /* _ASM_GENERIC_RELOC_H_ */
