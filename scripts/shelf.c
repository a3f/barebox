/*
 * shelf - ELF RELR Relocation Conversion Tool
 *
 * This tool converts between REL/RELA and RELR relocation formats in ELF files.
 * RELR is a compact encoding for relative relocations that achieves ~97% space
 * savings compared to RELA format.
 *
 * Usage:
 *   shelf --to-relr <input> -o <output>    # Convert REL/RELA to RELR
 *   shelf --from-relr <input> -o <output>  # Convert RELR back to REL/RELA
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <elf.h>

#include "common.h"
#include "common.c"

/* RELR constants (may not be in older elf.h) */
#ifndef DT_RELR
#define DT_RELRSZ    35
#define DT_RELR      36
#define DT_RELRENT   37
#endif

/* Architecture-specific relocation types for RELATIVE */
#define R_ARM_RELATIVE      23
#define R_AARCH64_RELATIVE  1027
#define R_RISCV_RELATIVE    3

struct elf_file {
	int class;          /* ELFCLASS32 or ELFCLASS64 */
	int is_bigendian;
	uint8_t *data;
	size_t size;
};

static const char *argv0;
static int verbose;

#define log(fmt, ...) printf("%s: " fmt, argv0, ##__VA_ARGS__)

#define log_verbose(args...) do {	\
	if (verbose)			\
		log(args);		\
} while (0)

#define panic(fmt, ...) do {                                    \
	fprintf(stderr, "%s: " fmt, argv0, ##__VA_ARGS__);      \
	exit(6);                                                \
} while (0)

/*
 * ELF accessor macros for runtime 32/64-bit handling
 * Pattern from barebox include/elf.h lines 414-432
 */

#define ELF_GET_FIELD(__s, __field, __type) \
static inline __type elf_##__s##_##__field(struct elf_file *elf, void *arg) { \
	if (elf->class == ELFCLASS32) \
		return (__type) ((Elf32_##__s *) arg)->__field; \
	else \
		return (__type) ((Elf64_##__s *) arg)->__field; \
}

#define ELF_SET_FIELD(__s, __field, __type) \
static inline void elf_set_##__s##_##__field(struct elf_file *elf, void *arg, __type val) { \
	if (elf->class == ELFCLASS32) \
		((Elf32_##__s *) arg)->__field = val; \
	else \
		((Elf64_##__s *) arg)->__field = val; \
}

/* Generate accessors for ELF header */
ELF_GET_FIELD(Ehdr, e_type, uint16_t)
ELF_GET_FIELD(Ehdr, e_machine, uint16_t)
ELF_GET_FIELD(Ehdr, e_phoff, uint64_t)
ELF_GET_FIELD(Ehdr, e_phentsize, uint16_t)
ELF_GET_FIELD(Ehdr, e_phnum, uint16_t)

/* Generate accessors for program header */
ELF_GET_FIELD(Phdr, p_type, uint32_t)
ELF_GET_FIELD(Phdr, p_offset, uint64_t)
ELF_GET_FIELD(Phdr, p_vaddr, uint64_t)
ELF_GET_FIELD(Phdr, p_paddr, uint64_t)
ELF_GET_FIELD(Phdr, p_filesz, uint64_t)
ELF_GET_FIELD(Phdr, p_memsz, uint64_t)
ELF_GET_FIELD(Phdr, p_flags, uint32_t)
ELF_GET_FIELD(Phdr, p_align, uint64_t)
ELF_SET_FIELD(Phdr, p_filesz, uint64_t)

/* Generate accessors for dynamic entries */
ELF_GET_FIELD(Dyn, d_tag, int64_t)

/* Special accessors for d_un.d_val which is in a union */
static inline uint64_t elf_Dyn_d_val(struct elf_file *elf, void *arg) {
	if (elf->class == ELFCLASS32)
		return (uint64_t) ((Elf32_Dyn *) arg)->d_un.d_val;
	else
		return (uint64_t) ((Elf64_Dyn *) arg)->d_un.d_val;
}

static inline void elf_set_Dyn_d_val(struct elf_file *elf, void *arg, uint64_t val) {
	if (elf->class == ELFCLASS32)
		((Elf32_Dyn *) arg)->d_un.d_val = val;
	else
		((Elf64_Dyn *) arg)->d_un.d_val = val;
}

ELF_SET_FIELD(Dyn, d_tag, int64_t)

/* Generate accessors for REL relocations */
ELF_GET_FIELD(Rel, r_offset, uint64_t)
ELF_GET_FIELD(Rel, r_info, uint64_t)
ELF_SET_FIELD(Rel, r_offset, uint64_t)
ELF_SET_FIELD(Rel, r_info, uint64_t)

/* Generate accessors for RELA relocations */
ELF_GET_FIELD(Rela, r_offset, uint64_t)
ELF_GET_FIELD(Rela, r_info, uint64_t)
ELF_GET_FIELD(Rela, r_addend, int64_t)
ELF_SET_FIELD(Rela, r_offset, uint64_t)
ELF_SET_FIELD(Rela, r_info, uint64_t)
ELF_SET_FIELD(Rela, r_addend, int64_t)

/* Helper to get size of ELF structures */
static inline size_t elf_size_of(struct elf_file *elf, const char *type)
{
	if (elf->class == ELFCLASS32) {
		if (strcmp(type, "Ehdr") == 0) return sizeof(Elf32_Ehdr);
		if (strcmp(type, "Phdr") == 0) return sizeof(Elf32_Phdr);
		if (strcmp(type, "Dyn") == 0) return sizeof(Elf32_Dyn);
		if (strcmp(type, "Rel") == 0) return sizeof(Elf32_Rel);
		if (strcmp(type, "Rela") == 0) return sizeof(Elf32_Rela);
		if (strcmp(type, "Relr") == 0) return 4;  /* word size */
	} else {
		if (strcmp(type, "Ehdr") == 0) return sizeof(Elf64_Ehdr);
		if (strcmp(type, "Phdr") == 0) return sizeof(Elf64_Phdr);
		if (strcmp(type, "Dyn") == 0) return sizeof(Elf64_Dyn);
		if (strcmp(type, "Rel") == 0) return sizeof(Elf64_Rel);
		if (strcmp(type, "Rela") == 0) return sizeof(Elf64_Rela);
		if (strcmp(type, "Relr") == 0) return 8;  /* word size */
	}
	return 0;
}

/* Helper to extract relocation type from r_info */
static inline uint32_t elf_r_type(struct elf_file *elf, uint64_t r_info)
{
	if (elf->class == ELFCLASS32)
		return ELF32_R_TYPE(r_info);
	else
		return ELF64_R_TYPE(r_info);
}

/* Helper to extract relocation symbol from r_info */
static inline uint32_t elf_r_sym(struct elf_file *elf, uint64_t r_info)
{
	if (elf->class == ELFCLASS32)
		return ELF32_R_SYM(r_info);
	else
		return ELF64_R_SYM(r_info);
}

/* Helper to construct r_info from symbol and type */
static inline uint64_t elf_r_info(struct elf_file *elf, uint32_t sym, uint32_t type)
{
	if (elf->class == ELFCLASS32)
		return ELF32_R_INFO(sym, type);
	else
		return ELF64_R_INFO(sym, type);
}

/* Get pointer to program header table */
static inline void *elf_get_phdr_table(struct elf_file *elf)
{
	uint64_t phoff = elf_Ehdr_e_phoff(elf, elf->data);
	return elf->data + phoff;
}

/* Get pointer to specific program header */
static inline void *elf_get_phdr(struct elf_file *elf, int index)
{
	void *phdr_table = elf_get_phdr_table(elf);
	size_t phdr_size = elf_size_of(elf, "Phdr");
	return phdr_table + (index * phdr_size);
}

/* Simplified libelf-like API */

static int elf_identify(const uint8_t *data, size_t size)
{
	if (size < EI_NIDENT)
		return -1;
	if (memcmp(data, ELFMAG, SELFMAG) != 0)
		return -1;
	return 0;
}

static int elf_begin(const char *filename, struct elf_file *elf)
{
	const char *bitstr[] = { "32", "64" };
	memset(elf, 0, sizeof(*elf));

	elf->data = read_file(filename, &elf->size);
	if (!elf->data)
		panic("Failed to read file: %m\n");

	log_verbose("%s: read %zu bytes\n", filename, elf->size);

	if (elf_identify(elf->data, elf->size) < 0)
		panic("Not a valid ELF file\n");

	elf->class = elf->data[EI_CLASS];
	elf->is_bigendian = (elf->data[EI_DATA] == ELFDATA2MSB);

	if (elf->class != ELFCLASS32 && elf->class != ELFCLASS64)
		panic("Invalid ELF class %d\n", elf->class);

	log_verbose("%s: %s-bit %s-endian\n", filename,
		    bitstr[elf->class == ELFCLASS64],
		    elf->is_bigendian ? "big" : "little");

	return 0;
}

static void elf_end(struct elf_file *elf)
{
	free(elf->data);
	memset(elf, 0, sizeof(*elf));
}

/* Structure to hold parsed dynamic section info */
struct dynamic_info {
	/* REL/RELA section */
	uint64_t rel_addr;      /* DT_REL or DT_RELA virtual address */
	uint64_t rel_size;      /* DT_RELSZ or DT_RELASZ */
	uint64_t rel_offset;    /* File offset (converted from vaddr) */
	int is_rela;            /* 1 if RELA, 0 if REL */

	/* RELR section */
	uint64_t relr_addr;     /* DT_RELR virtual address */
	uint64_t relr_size;     /* DT_RELRSZ */
	uint64_t relr_offset;   /* File offset (converted from vaddr) */

	/* Dynamic section itself */
	uint64_t dyn_offset;    /* File offset of PT_DYNAMIC */
	uint64_t dyn_size;      /* Size of PT_DYNAMIC */
};

/* Convert virtual address to file offset using program headers */
static uint64_t vaddr_to_offset(struct elf_file *elf, uint64_t vaddr)
{
	uint16_t phnum = elf_Ehdr_e_phnum(elf, elf->data);
	int i;

	for (i = 0; i < phnum; i++) {
		void *phdr = elf_get_phdr(elf, i);
		uint32_t type = elf_Phdr_p_type(elf, phdr);

		if (type != PT_LOAD)
			continue;

		uint64_t p_vaddr = elf_Phdr_p_vaddr(elf, phdr);
		uint64_t p_memsz = elf_Phdr_p_memsz(elf, phdr);
		uint64_t p_offset = elf_Phdr_p_offset(elf, phdr);

		if (vaddr >= p_vaddr && vaddr < p_vaddr + p_memsz) {
			return p_offset + (vaddr - p_vaddr);
		}
	}

	return 0;  /* Not found */
}

/* Parse PT_DYNAMIC segment to find relocation sections */
static int parse_dynamic(struct elf_file *elf, struct dynamic_info *info)
{
	uint16_t phnum = elf_Ehdr_e_phnum(elf, elf->data);
	void *dyn_data = NULL;
	size_t dyn_size = 0;
	int i;

	memset(info, 0, sizeof(*info));

	/* Find PT_DYNAMIC segment */
	for (i = 0; i < phnum; i++) {
		void *phdr = elf_get_phdr(elf, i);
		uint32_t type = elf_Phdr_p_type(elf, phdr);

		if (type == PT_DYNAMIC) {
			info->dyn_offset = elf_Phdr_p_offset(elf, phdr);
			info->dyn_size = elf_Phdr_p_filesz(elf, phdr);
			dyn_data = elf->data + info->dyn_offset;
			dyn_size = info->dyn_size;
			break;
		}
	}

	if (!dyn_data) {
		log_verbose("No PT_DYNAMIC segment found\n");
		return -1;
	}

	/* Parse dynamic entries */
	size_t dyn_entry_size = elf_size_of(elf, "Dyn");
	size_t num_entries = dyn_size / dyn_entry_size;

	for (i = 0; i < num_entries; i++) {
		void *dyn = dyn_data + (i * dyn_entry_size);
		int64_t tag = elf_Dyn_d_tag(elf, dyn);
		uint64_t val = elf_Dyn_d_val(elf, dyn);

		if (tag == DT_NULL)
			break;

		switch (tag) {
		case DT_REL:
			info->rel_addr = val;
			info->is_rela = 0;
			break;
		case DT_RELSZ:
			if (!info->is_rela)
				info->rel_size = val;
			break;
		case DT_RELA:
			info->rel_addr = val;
			info->is_rela = 1;
			break;
		case DT_RELASZ:
			if (info->is_rela)
				info->rel_size = val;
			break;
		case DT_RELR:
			info->relr_addr = val;
			break;
		case DT_RELRSZ:
			info->relr_size = val;
			break;
		}
	}

	/* Convert virtual addresses to file offsets */
	if (info->rel_addr) {
		info->rel_offset = vaddr_to_offset(elf, info->rel_addr);
		log_verbose("Found %s at vaddr 0x%lx, file offset 0x%lx, size %lu\n",
			    info->is_rela ? "DT_RELA" : "DT_REL",
			    info->rel_addr, info->rel_offset, info->rel_size);
	}

	if (info->relr_addr) {
		info->relr_offset = vaddr_to_offset(elf, info->relr_addr);
		log_verbose("Found DT_RELR at vaddr 0x%lx, file offset 0x%lx, size %lu\n",
			    info->relr_addr, info->relr_offset, info->relr_size);
	}

	return 0;
}

/* Relocation entry for internal representation */
struct reloc_entry {
	uint64_t offset;
	uint32_t type;
	uint32_t sym;
	int64_t addend;
};

/* Get the RELATIVE relocation type for the current architecture */
static uint32_t get_relative_type(struct elf_file *elf)
{
	uint16_t machine = elf_Ehdr_e_machine(elf, elf->data);

	switch (machine) {
	case EM_ARM:
		return R_ARM_RELATIVE;
	case EM_AARCH64:
		return R_AARCH64_RELATIVE;
	case EM_RISCV:
		return R_RISCV_RELATIVE;
	default:
		panic("Unsupported architecture: %d\n", machine);
	}
}

/* Parse REL or RELA relocations into internal format */
static int parse_relocations(struct elf_file *elf, struct dynamic_info *info,
			     struct reloc_entry **relocs_out, size_t *count_out)
{
	size_t entry_size = info->is_rela ?
		elf_size_of(elf, "Rela") : elf_size_of(elf, "Rel");
	size_t count = info->rel_size / entry_size;
	struct reloc_entry *relocs;
	void *rel_data = elf->data + info->rel_offset;
	int i;

	relocs = calloc(count, sizeof(*relocs));
	if (!relocs)
		panic("malloc failed\n");

	for (i = 0; i < count; i++) {
		void *entry = rel_data + (i * entry_size);
		uint64_t r_offset = info->is_rela ?
			elf_Rela_r_offset(elf, entry) : elf_Rel_r_offset(elf, entry);
		uint64_t r_info = info->is_rela ?
			elf_Rela_r_info(elf, entry) : elf_Rel_r_info(elf, entry);

		relocs[i].offset = r_offset;
		relocs[i].type = elf_r_type(elf, r_info);
		relocs[i].sym = elf_r_sym(elf, r_info);
		relocs[i].addend = info->is_rela ?
			elf_Rela_r_addend(elf, entry) : 0;
	}

	*relocs_out = relocs;
	*count_out = count;
	return 0;
}

/* Comparison function for sorting relocations by offset */
static int compare_relocs(const void *a, const void *b)
{
	const struct reloc_entry *ra = a;
	const struct reloc_entry *rb = b;

	if (ra->offset < rb->offset)
		return -1;
	if (ra->offset > rb->offset)
		return 1;
	return 0;
}

/* Encode relative relocations into RELR format */
static size_t encode_relr(struct elf_file *elf, struct reloc_entry *relocs,
			  size_t count, uint8_t **relr_out)
{
	size_t word_size = elf_size_of(elf, "Relr");
	size_t bitmap_bits = (word_size * 8) - 1;  /* LSB is type flag */
	uint8_t *relr_data;
	size_t relr_capacity = count * word_size;  /* Upper bound */
	size_t relr_pos = 0;
	uint64_t base = 0;
	uint64_t bitmap = 0;
	int bitmap_valid = 0;
	size_t i;

	relr_data = malloc(relr_capacity);
	if (!relr_data)
		panic("malloc failed\n");

	/* Sort relocations by offset */
	qsort(relocs, count, sizeof(*relocs), compare_relocs);

	for (i = 0; i < count; i++) {
		uint64_t offset = relocs[i].offset;

		/* Check if offset is word-aligned */
		if (offset % word_size != 0)
			panic("Relocation at 0x%lx not word-aligned\n", offset);

		/* Can we encode this in current bitmap? */
		if (bitmap_valid && offset >= base &&
		    offset < base + (bitmap_bits * word_size)) {
			/* Yes, set the appropriate bit */
			int bit = (offset - base) / word_size;
			bitmap |= (1ULL << bit);
		} else {
			/* Flush current bitmap if valid */
			if (bitmap_valid) {
				uint64_t bitmap_entry = (bitmap << 1) | 1;
				if (elf->class == ELFCLASS32) {
					*(uint32_t *)(relr_data + relr_pos) = bitmap_entry;
					relr_pos += 4;
				} else {
					*(uint64_t *)(relr_data + relr_pos) = bitmap_entry;
					relr_pos += 8;
				}
			}

			/* Start new base address */
			if (elf->class == ELFCLASS32) {
				*(uint32_t *)(relr_data + relr_pos) = offset;
				relr_pos += 4;
			} else {
				*(uint64_t *)(relr_data + relr_pos) = offset;
				relr_pos += 8;
			}

			base = offset + word_size;
			bitmap = 0;
			bitmap_valid = 1;
		}
	}

	/* Flush final bitmap if valid */
	if (bitmap_valid && bitmap != 0) {
		uint64_t bitmap_entry = (bitmap << 1) | 1;
		if (elf->class == ELFCLASS32) {
			*(uint32_t *)(relr_data + relr_pos) = bitmap_entry;
			relr_pos += 4;
		} else {
			*(uint64_t *)(relr_data + relr_pos) = bitmap_entry;
			relr_pos += 8;
		}
	}

	*relr_out = relr_data;
	return relr_pos;
}

/* Decode RELR format into relocation entries */
static int decode_relr(struct elf_file *elf, struct dynamic_info *info,
		       struct reloc_entry **relocs_out, size_t *count_out)
{
	size_t word_size = elf_size_of(elf, "Relr");
	uint8_t *relr_data = elf->data + info->relr_offset;
	size_t relr_size = info->relr_size;
	size_t entry_count = relr_size / word_size;
	size_t capacity = entry_count * 64;  /* Upper bound estimate */
	struct reloc_entry *relocs;
	size_t count = 0;
	uint64_t base = 0;
	size_t i, j;
	uint32_t rel_type = get_relative_type(elf);

	relocs = malloc(capacity * sizeof(*relocs));
	if (!relocs)
		panic("malloc failed\n");

	for (i = 0; i < entry_count; i++) {
		uint64_t val;

		if (elf->class == ELFCLASS32)
			val = *(uint32_t *)(relr_data + i * 4);
		else
			val = *(uint64_t *)(relr_data + i * 8);

		if ((val & 1) == 0) {
			/* Even: base address */
			if (count >= capacity)
				panic("RELR decode overflow\n");

			relocs[count].offset = val;
			relocs[count].type = rel_type;
			relocs[count].sym = 0;
			relocs[count].addend = 0;
			count++;

			base = val + word_size;
		} else {
			/* Odd: bitmap */
			uint64_t bitmap = val >> 1;
			size_t bitmap_bits = (word_size * 8) - 1;

			for (j = 0; j < bitmap_bits; j++) {
				if (bitmap & (1ULL << j)) {
					if (count >= capacity)
						panic("RELR decode overflow\n");

					relocs[count].offset = base + (j * word_size);
					relocs[count].type = rel_type;
					relocs[count].sym = 0;
					relocs[count].addend = 0;
					count++;
				}
			}

			base += bitmap_bits * word_size;
		}
	}

	*relocs_out = relocs;
	*count_out = count;
	return 0;
}

/* Update PT_DYNAMIC entries in the ELF file */
static int update_dynamic_entries(struct elf_file *elf, struct dynamic_info *info,
				  uint64_t new_rel_size, uint64_t new_relr_addr,
				  uint64_t new_relr_size)
{
	void *dyn_data = elf->data + info->dyn_offset;
	size_t dyn_entry_size = elf_size_of(elf, "Dyn");
	size_t num_entries = info->dyn_size / dyn_entry_size;
	int found_relr = 0, found_relrsz = 0, found_relrent = 0;
	size_t i;

	/*
	 * First pass: update or remove existing entries
	 *
	 * If new_rel_size == 0, all relocations converted to RELR, so remove
	 * DT_REL/DT_RELA, DT_RELSZ/DT_RELASZ, DT_RELENT/DT_RELAENT.
	 *
	 * If new_rel_size > 0, some non-relative relocations remain, so keep
	 * all three entries and update size.
	 *
	 * If new_relr_size == 0, remove DT_RELR, DT_RELRSZ, DT_RELRENT.
	 * If new_relr_size > 0, update or add them.
	 */
	for (i = 0; i < num_entries; i++) {
		void *dyn = dyn_data + (i * dyn_entry_size);
		int64_t tag = elf_Dyn_d_tag(elf, dyn);

		if (tag == DT_NULL)
			break;

		switch (tag) {
		case DT_REL:
		case DT_RELA:
			if (new_rel_size == 0) {
				/* All relocations converted to RELR, remove entry */
				elf_set_Dyn_d_tag(elf, dyn, DT_NULL);
				elf_set_Dyn_d_val(elf, dyn, 0);
				log_verbose("Removed DT_%s\n",
					    tag == DT_REL ? "REL" : "RELA");
			}
			/* If new_rel_size > 0, keep DT_REL/DT_RELA as-is */
			break;
		case DT_RELSZ:
		case DT_RELASZ:
			if (new_rel_size == 0) {
				/* All relocations converted to RELR, remove entry */
				elf_set_Dyn_d_tag(elf, dyn, DT_NULL);
				elf_set_Dyn_d_val(elf, dyn, 0);
				log_verbose("Removed DT_%s\n",
					    tag == DT_RELSZ ? "RELSZ" : "RELASZ");
			} else if (new_rel_size != (uint64_t)-1) {
				/* Update size for remaining non-relative relocations */
				elf_set_Dyn_d_val(elf, dyn, new_rel_size);
				log_verbose("Updated DT_%s to %lu\n",
					    tag == DT_RELSZ ? "RELSZ" : "RELASZ",
					    new_rel_size);
			}
			break;
		case DT_RELENT:
		case DT_RELAENT:
			if (new_rel_size == 0) {
				/* All relocations converted to RELR, remove entry */
				elf_set_Dyn_d_tag(elf, dyn, DT_NULL);
				elf_set_Dyn_d_val(elf, dyn, 0);
				log_verbose("Removed DT_%s\n",
					    tag == DT_RELENT ? "RELENT" : "RELAENT");
			}
			/* If new_rel_size > 0, keep DT_RELENT/DT_RELAENT as-is */
			break;
		case DT_RELR:
			if (new_relr_size == 0) {
				/* Removing RELR, zero out entry */
				elf_set_Dyn_d_tag(elf, dyn, DT_NULL);
				elf_set_Dyn_d_val(elf, dyn, 0);
				log_verbose("Removed DT_RELR\n");
			} else {
				/* Update RELR address */
				elf_set_Dyn_d_val(elf, dyn, new_relr_addr);
				found_relr = 1;
				log_verbose("Updated DT_RELR to 0x%lx\n", new_relr_addr);
			}
			break;
		case DT_RELRSZ:
			if (new_relr_size == 0) {
				/* Removing RELR, zero out entry */
				elf_set_Dyn_d_tag(elf, dyn, DT_NULL);
				elf_set_Dyn_d_val(elf, dyn, 0);
				log_verbose("Removed DT_RELRSZ\n");
			} else {
				/* Update RELR size */
				elf_set_Dyn_d_val(elf, dyn, new_relr_size);
				found_relrsz = 1;
				log_verbose("Updated DT_RELRSZ to %lu\n", new_relr_size);
			}
			break;
		case DT_RELRENT:
			if (new_relr_size == 0) {
				/* Removing RELR, zero out entry */
				elf_set_Dyn_d_tag(elf, dyn, DT_NULL);
				elf_set_Dyn_d_val(elf, dyn, 0);
				log_verbose("Removed DT_RELRENT\n");
			} else {
				found_relrent = 1;
			}
			break;
		}
	}

	/* Second pass: add missing RELR entries if needed */
	if (new_relr_size > 0 && (!found_relr || !found_relrsz || !found_relrent)) {
		size_t word_size = elf_size_of(elf, "Relr");

		for (i = 0; i < num_entries; i++) {
			void *dyn = dyn_data + (i * dyn_entry_size);
			int64_t tag = elf_Dyn_d_tag(elf, dyn);

			if (tag == DT_NULL) {
				/* Found a NULL slot, use it */
				if (!found_relr) {
					elf_set_Dyn_d_tag(elf, dyn, DT_RELR);
					elf_set_Dyn_d_val(elf, dyn, new_relr_addr);
					log_verbose("Added DT_RELR = 0x%lx\n", new_relr_addr);
					found_relr = 1;
					i++;
					if (i >= num_entries)
						break;
					dyn = dyn_data + (i * dyn_entry_size);
				}

				if (!found_relrsz) {
					elf_set_Dyn_d_tag(elf, dyn, DT_RELRSZ);
					elf_set_Dyn_d_val(elf, dyn, new_relr_size);
					log_verbose("Added DT_RELRSZ = %lu\n", new_relr_size);
					found_relrsz = 1;
					i++;
					if (i >= num_entries)
						break;
					dyn = dyn_data + (i * dyn_entry_size);
				}

				if (!found_relrent) {
					elf_set_Dyn_d_tag(elf, dyn, DT_RELRENT);
					elf_set_Dyn_d_val(elf, dyn, word_size);
					log_verbose("Added DT_RELRENT = %lu\n", word_size);
					found_relrent = 1;
				}

				break;
			}
		}

		if (!found_relr || !found_relrsz || !found_relrent)
			panic("No space in PT_DYNAMIC for RELR entries\n");
	}

	return 0;
}

/* Convert REL/RELA to RELR */
static int convert_to_relr(struct elf_file *elf, const char *output_file)
{
	struct dynamic_info info;
	struct reloc_entry *relocs = NULL, *relative_relocs = NULL;
	struct reloc_entry *non_relative_relocs = NULL;
	size_t count = 0, relative_count = 0, non_relative_count = 0;
	uint8_t *relr_data = NULL;
	size_t relr_size = 0;
	uint32_t relative_type;
	size_t i;
	int ret;

	if (parse_dynamic(elf, &info) < 0)
		panic("Failed to parse PT_DYNAMIC\n");

	if (!info.rel_addr || !info.rel_size) {
		log("No REL/RELA section found, nothing to convert\n");
		return -1;
	}

	/* Parse existing relocations */
	parse_relocations(elf, &info, &relocs, &count);

	relative_type = get_relative_type(elf);

	/* Partition into relative and non-relative */
	relative_relocs = malloc(count * sizeof(*relative_relocs));
	non_relative_relocs = malloc(count * sizeof(*non_relative_relocs));
	if (!relative_relocs || !non_relative_relocs)
		panic("malloc failed\n");

	for (i = 0; i < count; i++) {
		if (relocs[i].type == relative_type) {
			relative_relocs[relative_count++] = relocs[i];
		} else {
			non_relative_relocs[non_relative_count++] = relocs[i];
		}
	}

	log("Found %zu relocations: %zu relative, %zu non-relative\n",
	    count, relative_count, non_relative_count);

	if (relative_count == 0) {
		log("No relative relocations to convert\n");
		ret = -1;
		goto out;
	}

	/* Encode RELR */
	relr_size = encode_relr(elf, relative_relocs, relative_count, &relr_data);

	/* Calculate space requirements */
	size_t entry_size = info.is_rela ?
		elf_size_of(elf, "Rela") : elf_size_of(elf, "Rel");
	size_t original_size = count * entry_size;
	size_t remaining_rel_size = non_relative_count * entry_size;
	size_t freed_space = original_size - remaining_rel_size;

	log("Original REL/RELA size: %zu, remaining: %zu, freed: %zu\n",
	    original_size, remaining_rel_size, freed_space);
	log("RELR size: %zu\n", relr_size);

	if (relr_size > freed_space) {
		log("Error: Not enough space for RELR (%zu needed, %zu available)\n",
		    relr_size, freed_space);
		ret = -1;
		goto out;
	}

	/* Write non-relative relocations back to REL/RELA section */
	uint8_t *rel_data = elf->data + info.rel_offset;

	for (i = 0; i < non_relative_count; i++) {
		void *entry = rel_data + (i * entry_size);
		struct reloc_entry *r = &non_relative_relocs[i];

		if (info.is_rela) {
			elf_set_Rela_r_offset(elf, entry, r->offset);
			elf_set_Rela_r_info(elf, entry,
				elf_r_info(elf, r->sym, r->type));
			elf_set_Rela_r_addend(elf, entry, r->addend);
		} else {
			elf_set_Rel_r_offset(elf, entry, r->offset);
			elf_set_Rel_r_info(elf, entry,
				elf_r_info(elf, r->sym, r->type));
		}
	}

	/* Write RELR data after remaining REL/RELA */
	memcpy(rel_data + remaining_rel_size, relr_data, relr_size);

	/* Zero out any remaining freed space */
	size_t used_space = remaining_rel_size + relr_size;
	if (used_space < original_size)
		memset(rel_data + used_space, 0, original_size - used_space);

	/* Update PT_DYNAMIC */
	uint64_t relr_vaddr = info.rel_addr + remaining_rel_size;
	update_dynamic_entries(elf, &info, remaining_rel_size,
			       relr_vaddr, relr_size);

	/* Write output */
	if (write_file(output_file, elf->data, elf->size) < 0)
		panic("Failed to write output file: %m\n");

	log("Successfully converted to RELR: %s\n", output_file);
	ret = 0;

out:
	free(relocs);
	free(relative_relocs);
	free(non_relative_relocs);
	free(relr_data);
	return ret;
}

/* Convert RELR back to REL/RELA */
static int convert_from_relr(struct elf_file *elf, const char *output_file)
{
	struct dynamic_info info;
	struct reloc_entry *relr_relocs = NULL, *rel_relocs = NULL;
	struct reloc_entry *merged_relocs = NULL;
	size_t relr_count = 0, rel_count = 0, merged_count = 0;
	size_t entry_size;
	size_t i;
	int ret;

	if (parse_dynamic(elf, &info) < 0)
		panic("Failed to parse PT_DYNAMIC\n");

	if (!info.relr_addr || !info.relr_size) {
		log("No RELR section found, nothing to convert\n");
		return -1;
	}

	/* Decode RELR */
	decode_relr(elf, &info, &relr_relocs, &relr_count);
	log("Decoded %zu relocations from RELR\n", relr_count);

	/* Parse existing REL/RELA (non-relative relocations) */
	if (info.rel_addr && info.rel_size) {
		parse_relocations(elf, &info, &rel_relocs, &rel_count);
		log("Found %zu existing non-relative relocations\n", rel_count);
	}

	/* Merge both sets */
	merged_count = relr_count + rel_count;
	merged_relocs = malloc(merged_count * sizeof(*merged_relocs));
	if (!merged_relocs)
		panic("malloc failed\n");

	memcpy(merged_relocs, relr_relocs, relr_count * sizeof(*relr_relocs));
	if (rel_count > 0)
		memcpy(merged_relocs + relr_count, rel_relocs,
		       rel_count * sizeof(*rel_relocs));

	/* Sort by offset */
	qsort(merged_relocs, merged_count, sizeof(*merged_relocs), compare_relocs);

	/* Calculate space needed */
	entry_size = info.is_rela ?
		elf_size_of(elf, "Rela") : elf_size_of(elf, "Rel");
	size_t needed_size = merged_count * entry_size;
	size_t original_size = info.rel_size;

	log("Need %zu bytes for %zu relocations (original had %zu bytes)\n",
	    needed_size, merged_count, original_size);

	/* Check if we can fit in original space and have a valid REL/RELA section */
	uint8_t *rel_data = NULL;
	if (info.rel_offset > 0)
		rel_data = elf->data + info.rel_offset;

	/*
	 * The physical section size doesn't change during to-relr conversion,
	 * only DT_RELASZ changes. So for round-trip conversion, we can always
	 * write back in-place if we have a valid rel_offset. We just need to
	 * ensure we don't exceed the file bounds.
	 */
	size_t max_available = elf->size - info.rel_offset;

	if (rel_data && needed_size <= max_available) {
		/* Fits in original space, write in-place */
		for (i = 0; i < merged_count; i++) {
			void *entry = rel_data + (i * entry_size);
			struct reloc_entry *r = &merged_relocs[i];

			if (info.is_rela) {
				elf_set_Rela_r_offset(elf, entry, r->offset);
				elf_set_Rela_r_info(elf, entry,
					elf_r_info(elf, r->sym, r->type));
				elf_set_Rela_r_addend(elf, entry, r->addend);
			} else {
				elf_set_Rel_r_offset(elf, entry, r->offset);
				elf_set_Rel_r_info(elf, entry,
					elf_r_info(elf, r->sym, r->type));
			}
		}

		/* Update PT_DYNAMIC to remove RELR entries */
		update_dynamic_entries(elf, &info, needed_size, 0, 0);

		/* Write output */
		if (write_file(output_file, elf->data, elf->size) < 0)
			panic("Failed to write output file: %m\n");
	} else {
		/* Need to append at end of file */
		log("Appending REL/RELA at end of file\n");

		size_t new_size = elf->size + needed_size;
		uint8_t *new_data = malloc(new_size);
		uint8_t *orig_data = elf->data;
		if (!new_data)
			panic("malloc failed\n");

		memcpy(new_data, elf->data, elf->size);

		/* Temporarily switch to new_data for writing relocations */
		elf->data = new_data;

		/* Write relocations at end */
		uint8_t *new_rel_data = new_data + elf->size;
		for (i = 0; i < merged_count; i++) {
			void *entry = new_rel_data + (i * entry_size);
			struct reloc_entry *r = &merged_relocs[i];

			if (info.is_rela) {
				elf_set_Rela_r_offset(elf, entry, r->offset);
				elf_set_Rela_r_info(elf, entry,
					elf_r_info(elf, r->sym, r->type));
				elf_set_Rela_r_addend(elf, entry, r->addend);
			} else {
				elf_set_Rel_r_offset(elf, entry, r->offset);
				elf_set_Rel_r_info(elf, entry,
					elf_r_info(elf, r->sym, r->type));
			}
		}

		/* Update PT_DYNAMIC in new data */
		elf->size = new_size;

		/* We need to update DT_REL/DT_RELA pointer - but this is complex
		 * as it's a virtual address. For simplicity, we'll use the existing
		 * location if it fits. */
		update_dynamic_entries(elf, &info, needed_size, 0, 0);

		/* Write output */
		if (write_file(output_file, new_data, new_size) < 0)
			panic("Failed to write output file: %m\n");

		/* Free original data, keep new data for elf_end to clean up */
		free(orig_data);
	}

	log("Successfully converted from RELR: %s\n", output_file);
	ret = 0;

	free(relr_relocs);
	free(rel_relocs);
	free(merged_relocs);
	return ret;
}

static void usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [OPTIONS] <input> -o <output>\n", prog);
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  --to-relr      Convert REL/RELA to RELR format\n");
	fprintf(stderr, "  --from-relr    Convert RELR back to REL/RELA format\n");
	fprintf(stderr, "  -o <output>    Output file\n");
	fprintf(stderr, "  -v             Verbose output\n");
	fprintf(stderr, "  -h, --help     Show this help\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Example:\n");
	fprintf(stderr, "  %s --to-relr barebox -o barebox.relr\n", prog);
	fprintf(stderr, "  %s --from-relr barebox.relr -o barebox\n", prog);
}

int main(int argc, char **argv)
{
	const char *output_file = NULL;
	const char *input_file = NULL;
	struct elf_file elf;
	int to_relr = 0, from_relr = 0;
	int opt;
	int ret;

	static struct option long_options[] = {
		{"to-relr", no_argument, 0, 't'},
		{"from-relr", no_argument, 0, 'f'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	if (getenv("KBUILD_VERBOSE"))
		verbose = 1;

	argv0 = argv[0];

	while ((opt = getopt_long(argc, argv, "o:vh", long_options, NULL)) != -1) {
		switch (opt) {
		case 't':
			to_relr = 1;
			break;
		case 'f':
			from_relr = 1;
			break;
		case 'o':
			output_file = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'h':
			usage(argv[0]);
			return 0;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "Error: No input file specified\n");
		usage(argv[0]);
		return 1;
	}

	input_file = argv[optind];

	if (!to_relr && !from_relr) {
		fprintf(stderr, "Error: Must specify --to-relr or --from-relr\n");
		usage(argv[0]);
		return 1;
	}

	if (to_relr && from_relr) {
		fprintf(stderr, "Error: Cannot specify both --to-relr and --from-relr\n");
		usage(argv[0]);
		return 1;
	}

	if (!output_file) {
		fprintf(stderr, "Error: No output file specified\n");
		usage(argv[0]);
		return 1;
	}

	if (elf_begin(input_file, &elf) < 0)
		return 1;

	if (to_relr)
		ret = convert_to_relr(&elf, output_file);
	else
		ret = convert_from_relr(&elf, output_file);

	elf_end(&elf);

	return ret;
}
