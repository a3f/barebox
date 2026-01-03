// SPDX-License-Identifier: GPL-2.0

#include <elf.h>
#include <pbl/elf.h>
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

void pbl_elf_load(struct elf_image *elf)
{
	void *buf = elf->hdr_buf;
	void *phdr = (void *) (buf + elf_hdr_e_phoff(elf, buf));

	for (int i = 0; i < elf_hdr_e_phnum(elf, buf); i++) {
		u64 p_offset = elf_phdr_p_offset(elf, phdr);
		u64 p_filesz = elf_phdr_p_filesz(elf, phdr);
		u64 p_memsz = elf_phdr_p_memsz(elf, phdr);
		void *dst = elf_phdr_relocated_paddr(elf, phdr);

		pr_debug("Loading phdr offset 0x%llx to 0x%p (%llu bytes)\n",
			 p_offset, dst, p_filesz);

		memcpy(dst, elf->hdr_buf + p_offset, p_filesz);

		if (p_filesz < p_memsz)
			memset(dst + p_filesz, 0x00, p_memsz - p_filesz);

		phdr += elf_size_of_phdr(elf);
	}
}
