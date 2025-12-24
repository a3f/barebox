/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __MALLOC_H
#define __MALLOC_H

#include <linux/compiler.h>
#include <types.h>
#include <alloc.h>

void malloc_add_pool(void *mem, size_t bytes);

#ifdef CONFIG_MALLOC_TLSF
void malloc_register_store(void (*cb)(size_t bytes));
bool malloc_store_is_registered(void);
#else
#include <linux/bug.h>
static inline void malloc_register_store(void (*cb)(size_t bytes)) { BUG(); }
static inline bool malloc_store_is_registered(void) { return false; }
#endif

void *malloc(size_t) __alloc_size(1);
size_t malloc_usable_size(const void *);
void free(void *);
void free_sensitive(void *);
void *realloc(void *, size_t) __realloc_size(2);
void *memalign(size_t, size_t) __alloc_size(2);
void *calloc(size_t, size_t) __alloc_size(1, 2);
void malloc_stats(void);

int mem_malloc_is_initialized(void);

#ifdef CONFIG_DEBUG_MEMLEAK
void memleak_check(void);
#else
static inline void memleak_check(void) {}
#endif

#endif /* __MALLOC_H */
