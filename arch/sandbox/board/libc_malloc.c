// SPDX-License-Identifier: GPL-2.0-or-later

#include <alloc.h>

void *libc_memalign(size_t alignment, size_t bytes, void *ctx);
void *libc_malloc(size_t size, void *ctx);
void *libc_calloc(size_t n, size_t elem_size, void *ctx);
void libc_free(void *ptr, void *ctx);
size_t libc_malloc_usable_size(const void *ptr, void *ctx);
void *libc_realloc(void *ptr, size_t size, void *ctx);

struct allocator_ops libc_malloc_ops = {
	.malloc = libc_malloc,
	.calloc = libc_calloc,
	.memalign = libc_memalign,
	.internally_sized.free = libc_free,
	.internally_sized.realloc = libc_realloc,
	.internally_sized.malloc_usable_size = libc_malloc_usable_size,
};

struct allocator default_alloc = {
	.ops = &libc_malloc_ops,
};
