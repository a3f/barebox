// SPDX-License-Identifier: GPL-2.0-or-later

#include <malloc.h>
#include <alloc.h>
#include <string.h>
#include <linux/export.h>
#include <asm-generic/sections.h>

/* This #if clause will be dropped, once we migrated everything to the
 * new allocator API
 */
#if defined(CONFIG_MALLOC_LIBC)

void *malloc(size_t size)
{
	return malloc_a(size, &default_alloc);
}
EXPORT_SYMBOL(malloc);

void free(void *ptr)
{
	return free_a(ptr, 0, &default_alloc);
}
EXPORT_SYMBOL(free);

void free_sensitive(void *ptr)
{
	free_sensitive_a(ptr, 0, &default_alloc);
}
EXPORT_SYMBOL(free_sensitive);

void free_const(const void *str)
{
	strfree_const_a(str, &default_alloc);
}
EXPORT_SYMBOL(free_const);

void *realloc(void *ptr, size_t size)
{
	return realloc_a(ptr, 0, size, &default_alloc);
}
EXPORT_SYMBOL(realloc);

void *memalign(size_t align, size_t size)
{
	return memalign_a(align, size, &default_alloc);
}
EXPORT_SYMBOL(memalign);

void *calloc(size_t count, size_t size)
{
	return calloc_a(count, size, &default_alloc);
}
EXPORT_SYMBOL(calloc);

void malloc_stats(void)
{
	malloc_stats_a(&default_alloc);
}
EXPORT_SYMBOL(malloc_stats);

#else

void free_sensitive(void *mem)
{
	size_t size = malloc_usable_size(mem);

	if (size)
		memzero_explicit(mem, size);

	free(mem);
}

#endif
