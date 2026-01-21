/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef __LOADABLE_H
#define __LOADABLE_H

#include <linux/types.h>
#include <linux/list.h>
#include <filetype.h>
#include <image.h>

#define UIMAGE_SOME_ADDRESS (UIMAGE_INVALID_ADDRESS - 1)
#define UIMAGE_IS_ADDRESS_VALID(addr) \
	((addr) != UIMAGE_INVALID_ADDRESS && \
	 (addr) != UIMAGE_SOME_ADDRESS)

struct loadable;
struct resource;

/**
 * enum loadable_type - type of boot component
 */
enum loadable_type {
	LOADABLE_UNSPECIFIED,
	LOADABLE_KERNEL,
	LOADABLE_INITRD,
	LOADABLE_FDT,
	LOADABLE_TEE,
};

/**
 * struct loadable_info - metadata about a loadable (no data loaded yet)
 */
struct loadable_info {
	size_t size;                    /* uncompressed size (0 if unknown) */
	size_t compressed_size;         /* original size if compressed */
	unsigned long load_addr;        /* suggested load address or UIMAGE_SOME_ADDRESS */
	unsigned long entry_offset;     /* entry point offset from load_addr */
	enum filetype filetype;         /* detected file type */
	bool compressed;                /* needs decompression during commit */
};

/**
 * struct loadable_ops - operations for a loadable
 */
struct loadable_ops {
	/**
	 * get_info - obtain metadata without loading data
	 *
	 * Must not allocate large buffers or decompress. Should read only
	 * headers/properties needed to determine size and addresses.
	 * Result is cached in loadable->info.
	 */
	int (*get_info)(struct loadable *l, struct loadable_info *info);

	/**
	 * commit - load/decompress to target address
	 *
	 * @load_addr: final RAM address where data should reside
	 * @size: size of buffer at load_addr (0 if unknown/unlimited)
	 *
	 * This is where data transfer happens.
	 * For compressed data: decompress to load_addr.
	 * For uncompressed data: read/copy to load_addr.
	 *
	 * Behavior:
	 *   - Must respect the provided load_addr
	 *   - If size > 0, must check if buffer is sufficient, return -ENOSPC if too small
	 *   - Must call request_sdram_region() to register the memory region
	 *   - Must populate l->res with the region descriptor
	 *
	 * Returns: actual number of bytes written on success, negative errno on error
	 */
	int (*commit)(struct loadable *l, unsigned long load_addr, size_t size);

	/**
	 * release - free resources associated with this loadable
	 *
	 * Called during cleanup. Does NOT release l->res (handled by caller).
	 */
	void (*release)(struct loadable *l);

	/**
	 * describe - format human-readable description for debugging/testing
	 */
	int (*describe)(struct loadable *l, char *buf, size_t len);
};

/**
 * struct loadable - lazy-loadable boot component
 *
 * Represents something that can be loaded to RAM (kernel, initrd, fdt, tee).
 * Metadata can be queried without loading. Actual loading happens on commit().
 */
struct loadable {
	const char *name;               /* descriptive name for debugging */
	enum loadable_type type;

	const struct loadable_ops *ops;
	void *priv;                     /* format-specific private data */

	/* Populated by get_info(), cached */
	struct loadable_info info;
	bool info_valid;

	/* Populated after successful commit() */
	struct resource *res;

	struct list_head list;          /* for image_data.loadables */
};

/* Core API */
int loadable_get_info(struct loadable *l, struct loadable_info *info);
int loadable_commit(struct loadable *l, unsigned long load_addr, size_t size);
void loadable_release(struct loadable *l);

/* Factory functions - to be implemented per format */
struct fit_handle;
struct uimage_handle;

struct loadable *loadable_from_fit(struct fit_handle *fit,
				   void *config,
				   const char *image_name,
				   int index,
				   enum loadable_type type);

struct loadable *loadable_from_uimage(struct uimage_handle *uimage,
				      int part_num,
				      enum loadable_type type);

struct loadable *loadable_from_file(const char *path,
				    enum loadable_type type);

#endif /* __LOADABLE_H */
