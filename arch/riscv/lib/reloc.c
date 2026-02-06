// SPDX-License-Identifier: GPL-2.0+
// SPDX-FileCopyrightText: Copyright (c) 2021 Ahmad Fatoum, Pengutronix

#include <common.h>
#include <linux/linkage.h>
#include <asm/sections.h>
#include <asm/barebox-riscv.h>
#include <asm/cache.h>
#include <debug_ll.h>
#include <asm-generic/module.h>

#include <elf.h>

#if __riscv_xlen == 64
#define Elf_Rela			Elf64_Rela
#define R_RISCV_ABSOLUTE		R_RISCV_64
#define DYNSYM_ENTRY(dynsym, rela)	dynsym[ELF_R_SYM(rela->r_info) * 3 + 1]
#elif __riscv_xlen == 32
#define Elf_Rela			Elf32_Rela
#define R_RISCV_ABSOLUTE		R_RISCV_32
#define DYNSYM_ENTRY(dynsym, rela)	dynsym[ELF_R_SYM(rela->r_info) * 4 + 1]
#else
#error unknown riscv target
#endif

#define RISC_R_TYPE(x)	((x) & 0xFF)

void sync_caches_for_execution(void)
{
	local_flush_icache_all();
}

static void relocate_image(unsigned long offset,
			   void *dstart, void *dend,
			   long *dynsym, long *dynend)
{
	Elf_Rela *rela;

	if (!offset)
		return;

	for (rela = dstart; (void *)rela < dend; rela++) {
		unsigned long *fixup;

		fixup = (unsigned long *)(rela->r_offset + offset);

		switch (RISC_R_TYPE(rela->r_info)) {
		case R_RISCV_RELATIVE:
			*fixup = rela->r_addend + offset;
			break;
		case R_RISCV_ABSOLUTE:
			*fixup = DYNSYM_ENTRY(dynsym, rela) + rela->r_addend + offset;
			break;
		default:
			putc_ll('>');
			puthex_ll(rela->r_info);
			putc_ll(' ');
			puthex_ll(rela->r_offset);
			putc_ll(' ');
			puthex_ll(rela->r_addend);
			putc_ll('\n');
			__hang();
		}
	}

}

void relocate_relr(unsigned long offset, const void *start, const void *end)
{
#if __riscv_xlen == 64
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
#elif __riscv_xlen == 32
	const u32 *relr;
	u32 *place = NULL;

	for (relr = start; relr < (const u32 *)end; relr++) {
		if ((*relr & 1) == 0) {
			place = (u32 *)(*relr + offset);
			*place++ += offset;
		} else {
			u32 *p = place;
			u32 r = *relr >> 1;

			for (; r; p++, r >>= 1)
				if (r & 1)
					*p += offset;
			place += 31;
		}
	}
#endif
}

void relocate_to_current_adr(void)
{
	unsigned long offset = get_runtime_offset();

	relocate_image(offset,
		       runtime_address(__rel_dyn_start),
		       runtime_address(__rel_dyn_end),
		       runtime_address(__dynsym_start),
		       NULL);

	relocate_relr(offset,
		      runtime_address(__relr_dyn_start),
		      runtime_address(__relr_dyn_end));

	sync_caches_for_execution();
}

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
