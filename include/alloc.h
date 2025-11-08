/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ALLOC_H
#define __ALLOC_H

#include <linux/compiler.h>
#include <linux/types.h>

#define MALLOC_SHIFT_MAX	30
#define MALLOC_MAX_SIZE		(1UL << MALLOC_SHIFT_MAX)

/*
 * ZERO_SIZE_PTR will be returned for zero sized kmalloc requests.
 *
 * Dereferencing ZERO_SIZE_PTR will lead to a distinct access fault.
 *
 * ZERO_SIZE_PTR can be passed to free though in the same way that NULL can.
 * Both make free a no-op.
 */
#define ZERO_SIZE_PTR ((void *)16)

#define ZERO_OR_NULL_PTR(x) ((unsigned long)(x) <= \
			     (unsigned long)ZERO_SIZE_PTR)

struct allocator_ops {
	void *(*malloc)(size_t, void *ctx);
	void *(*calloc)(size_t, size_t, void *ctx);
	void *(*memalign)(size_t, size_t, void *ctx);
	void (*stats)(void *ctx);

	struct {
		void *(*realloc)(void *, size_t, void *ctx);
		void (*free)(void *, void *ctx);
		size_t (*malloc_usable_size)(const void *, void *ctx);
	} internally_sized;

	struct {
		void *(*realloc)(void *, size_t, size_t, void *ctx);
		void (*free)(void *, size_t, void *ctx);
	} externally_sized;
};

typedef const struct allocator {
	const struct allocator_ops *ops;
	void *ctx;
} *alloc_t;

extern struct allocator default_alloc;

void *malloc_a(size_t size, alloc_t alloc) __alloc_size(1);
void *calloc_a(size_t n, size_t elem_size, alloc_t alloc) __alloc_size(1, 2);
void *realloc_a(void *mem, size_t oldsize, size_t newsize,
		alloc_t alloc) __realloc_size(2);
void free_a(void *mem, size_t, alloc_t alloc);
void free_sensitive_a(void *mem, size_t, alloc_t alloc);
char *strdup_a(const char *str, alloc_t alloc);
const char *strdup_const_a(const char *str, alloc_t alloc);
void free_const_a(const void *ptr, size_t size, alloc_t alloc);
void strfree_const_a(const char *str, alloc_t alloc);
void *memalign_a(size_t alignment, size_t size, alloc_t alloc) __alloc_size(2);
void malloc_stats_a(alloc_t alloc);

static inline bool want_init_on_alloc(void)
{
	return IS_ENABLED(CONFIG_INIT_ON_ALLOC_DEFAULT_ON);
}

static inline bool want_init_on_free(void)
{
	return IS_ENABLED(CONFIG_INIT_ON_FREE_DEFAULT_ON);
}

#endif /* __ALLOC_H */
