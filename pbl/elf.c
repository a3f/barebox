// SPDX-License-Identifier: GPL-2.0

#include <elf.h>
#include <pbl.h>
#include <mmu.h>
#include <pbl/elf.h>
#include <asm/reloc.h>
#include <linux/string.h>
#include <linux/printk.h>

static void *elf_phdr_relocated_paddr(struct elf_image *elf, void *phdr)
{
	return elf->reloc_base + elf_phdr_p_paddr(elf, phdr);
}

static void *elf_hdr_relocated_entry(struct elf_image *elf)
{
	return elf->reloc_base + elf_hdr_e_entry(elf, elf->hdr_buf);
}

static void process_elf_segment(struct elf_image *elf, void *phdr)
{
	void *dst = elf_phdr_relocated_paddr(elf, phdr);
	u64 p_memsz = elf_phdr_p_memsz(elf, phdr);

	/* we care only about PT_LOAD segments */
	if (elf_phdr_p_type(elf, phdr) != PT_LOAD || !p_memsz)
		return;

	if (dst < elf->low_addr)
		elf->low_addr = dst;
	if (dst + p_memsz > elf->high_addr)
		elf->high_addr = dst + p_memsz;
}

static void elf_parse_segments(struct elf_image *elf)
{
	void *buf = elf->hdr_buf;
	void *phdr = (void *) (buf + elf_hdr_e_phoff(elf, buf));

	elf->entry = elf_hdr_relocated_entry(elf);

	for (int i = 0; i < elf_hdr_e_phnum(elf, buf) ; ++i) {
		process_elf_segment(elf, phdr);
		phdr += elf_size_of_phdr(elf);
	}

	pr_debug("ELF would be loaded to %p-%p\n",
		 elf->low_addr, elf->high_addr);
}

static void elf_check_image(struct elf_image *elf, void *buf)
{
	if (memcmp(buf, ELFMAG, SELFMAG))
		panic("ELF magic not found.\n");

	elf->class = ((char *) buf)[EI_CLASS];
	elf->type = elf_hdr_e_type(elf, buf);

	if (elf->type != ET_DYN)
		panic("Non DYN ELF image.\n");

	if (!elf_hdr_e_phnum(elf, buf))
		panic("No phdr found.\n");
}

static void elf_init_struct(struct elf_image *elf)
{
	elf->low_addr = (void *) (unsigned long) -1;
	elf->high_addr = 0;
	elf->reloc_base = 0;
}

void pbl_elf_parse(struct elf_image *elf, void *buf, size_t len)
{
	pbl_verify_piggy(buf, len);

	elf_init_struct(elf);

	elf->hdr_buf = buf;

	elf_check_image(elf, buf);

	elf_parse_segments(elf);
}

void pbl_elf_relocate(struct elf_image *elf, void *reloc_base)
{
	elf_init_struct(elf);
	elf->reloc_base = reloc_base;
	elf_parse_segments(elf);
}

static inline void *elf_reloc_ptr(const struct elf_image *elf, const elf_dyn *reloc)
{
	return elf->reloc_base + reloc->d_un.d_ptr;
}

static void pbl_elf_apply_relocs(struct elf_image *elf,
				 const elf_dyn *relocs, size_t count)
{
	void *rela = NULL, *rel = NULL, *dynsym = NULL;
	ulong rela_sz = 0, rel_sz = 0;
	void *dstart, *dend;

	for (int i = 0; i < count; i++) {
		const elf_dyn *reloc = &relocs[i];

		switch (reloc->d_tag) {
		case DT_RELA:
			rela = elf_reloc_ptr(elf, reloc);
			break;
		case DT_RELASZ:
			rela_sz = reloc->d_un.d_val;
			break;
		case DT_REL:
			rel = elf_reloc_ptr(elf, reloc);
			break;
		case DT_RELSZ:
			rel_sz = reloc->d_un.d_val;
			break;
		case DT_SYMTAB:
			dynsym = elf_reloc_ptr(elf, reloc);
			break;
		}
	}

	if (rela && rela_sz) {
		dstart = rela;
		dend = rela + rela_sz;
	} else if (rel && rel_sz) {
		dstart = rel;
		dend = rel + rel_sz;
	} else {
		pr_debug("No relocations to fixup\n");
		return;
	}

	relocate_image((ulong)elf->reloc_base,
		       dstart, dend, dynsym, NULL);
}

static maptype_t get_maptype(unsigned flags)
{
	switch (flags & (PF_R | PF_W | PF_X)) {
	case PF_R | PF_X:
		return MAP_CODE;
	case PF_R | PF_W:
		return MAP_CACHED;
	case PF_R:
		return MAP_RO;
	}

	pr_warn("Unexpected ELF segment flags: 0x%x\n", flags);
	return MAP_CACHED;
}

void pbl_elf_load(struct elf_image *elf)
{
	void *buf = elf->hdr_buf;
	void *phdr = (void *) (buf + elf_hdr_e_phoff(elf, buf));
	elf_dyn *dyn = NULL;
	size_t ndyn;

	for (int i = 0; i < elf_hdr_e_phnum(elf, buf); i++) {
		u64 p_offset = elf_phdr_p_offset(elf, phdr);
		u64 p_filesz = elf_phdr_p_filesz(elf, phdr);
		u64 p_memsz = elf_phdr_p_memsz(elf, phdr);
		void *dst = elf_phdr_relocated_paddr(elf, phdr);
		void *src = elf->hdr_buf + p_offset;
		unsigned flags = elf_phdr_p_flags(elf, phdr);
		bool is_compressed = flags & PF_COMPRESSED;

		pr_debug("%s phdr offset 0x%llx+%llu to 0x%p (%llu bytes)\n",
			 is_compressed ? "Decompressing" : "Copying",
			 p_offset, p_filesz, dst, p_memsz);

		if (is_compressed) {
			long pos = pbl_barebox_uncompress_noverify(dst, src, p_filesz);
			if (pos != p_filesz)
				panic("Corruption detected: ELF segment size mismatch\n");
		} else {
			memcpy(dst, src, p_filesz);
			if (p_filesz < p_memsz)
				memset(dst + p_filesz, 0x00, p_memsz - p_filesz);
		}


		if (elf_phdr_p_type(elf, phdr) == PT_DYNAMIC) {
			dyn = dst;
			ndyn = p_filesz / sizeof(*dyn);
		}

		phdr += elf_size_of_phdr(elf);
	}

	if (dyn)
		pbl_elf_apply_relocs(elf, dyn, ndyn);

	phdr = (void *) (buf + elf_hdr_e_phoff(elf, buf));

	for (int i = 0; i < elf_hdr_e_phnum(elf, buf); i++) {
		u64 p_memsz = elf_phdr_p_memsz(elf, phdr);
		void *dst = elf_phdr_relocated_paddr(elf, phdr);
		unsigned flags = elf_phdr_p_flags(elf, phdr);

		if (PTR_IS_ALIGNED(dst, PAGE_SIZE))
			remap_range(dst, p_memsz, get_maptype(flags));
		else if ((flags & (PF_R | PF_W)) != (PF_R | PF_W))
			pr_err("Skipping remap for segment #%d (flags=0x%x) at 0x%p-%llu\n",
			       i, flags, dst, p_memsz);

		phdr += elf_size_of_phdr(elf);
	}
}
