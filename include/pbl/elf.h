/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __PBL_ELF_H
#define __PBL_ELF_H

#include <elf.h>

void pbl_elf_parse(struct elf_image *elf, void *buf, size_t len);
void pbl_elf_relocate(struct elf_image *elf, void *reloc_base);
void pbl_elf_load(struct elf_image *elf);

#endif
