// SPDX-License-Identifier: GPL-2.0-only

#include <alloc.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/overflow.h>
#include <linux/export.h>
#include <linux/bug.h>
#include <asm/sections.h>
#include <errno.h>

/**
 * talloc_size() - Allocate a (contiguous) memory chunk.
 *
 * @parent: pointer to previously talloc'ed memory chunk from which this
 *          chunk depends, or NULL.
 * @size: amount of memory requested (in bytes).
 *
 * Return: pointer to the allocated memory chunk, or NULL if there was an error.
 */
void *malloc_a(size_t size, alloc_t alloc)
{
	void *mem;

	if (!size)
		return ZERO_SIZE_PTR;

	alloc = alloc ?: &default_alloc;
	mem = alloc->ops->malloc(size, alloc->ctx);
	if (!mem)
		errno = ENOMEM;

	return mem;
}
EXPORT_SYMBOL(malloc_a);

/**
 * tzalloc() - Allocate a zeroed (contiguous) memory chunk.
 *
 * @parent: pointer to previously talloc'ed memory chunk from which this
 *          chunk depends, or NULL.
 * @size: amount of memory requested (in bytes).
 *
 * Return: pointer to the allocated memory chunk, or NULL if there was an error.
 */
void *calloc_a(size_t n, size_t elem_size, alloc_t alloc)
{
	size_t size;
	void *r;

	alloc = alloc ?: &default_alloc;
	if (alloc->ops->calloc)
		return alloc->ops->calloc(n, elem_size, alloc->ctx);

	size = size_mul(elem_size, n);
	r = malloc_a(size, alloc);
	if (!ZERO_OR_NULL_PTR(r) && !want_init_on_alloc())
		memset(r, 0x0, size);

	return r;
}
EXPORT_SYMBOL(calloc_a);

static void *__realloc_a(void *ptr, size_t oldsize, size_t newsize, alloc_t alloc)
{
	if (alloc->ops->externally_sized.realloc)
		return alloc->ops->externally_sized.realloc(ptr, oldsize, newsize, alloc->ctx);
	if (alloc->ops->internally_sized.realloc)
		return alloc->ops->internally_sized.realloc(ptr, newsize, alloc->ctx);

	return NULL;
}

/**
 * trealloc() - Modify the size of a talloc'ed memory chunk.
 *
 * @parent: parent to set if mem is NULL.
 * @mem: pointer to previously talloc'ed memory chunk.
 * @size: amount of memory requested (in bytes).
 *
 * Return: pointer to the allocated memory chunk, or NULL if there was an error.
 */
void *realloc_a(void *ptr, size_t oldsize, size_t newsize, alloc_t alloc)
{
	void *mem;

	alloc = alloc ?: &default_alloc;

	if (!newsize) {
		free_a(ptr, oldsize, alloc);
		return ZERO_SIZE_PTR;
	}
	if (ZERO_OR_NULL_PTR(ptr))
		ptr = NULL;

	mem = __realloc_a(ptr, oldsize, newsize, alloc);
	if (!mem)
		errno = ENOMEM;
	if (!newsize)
		return ZERO_SIZE_PTR;

	return mem;
}
EXPORT_SYMBOL(realloc_a);

void *memalign_a(size_t alignment, size_t size, alloc_t alloc)
{
	void *mem;

	if (!size)
		return ZERO_SIZE_PTR;

	alloc = alloc ?: &default_alloc;
	if (!alloc->ops->memalign) {
		errno = ENOSYS;
		return NULL;
	}

	mem = alloc->ops->memalign(alignment, size, alloc->ctx);
	if (!mem)
		errno = ENOMEM;

	return mem;
}
EXPORT_SYMBOL(memalign_a);

/**
 * talloc_free() - Deallocate a talloc'ed memory chunk and all the chunks depending on it.
 *
 * @mem: pointer to previously talloc'ed memory chunk.
 */
void free_a(void *mem, size_t size, alloc_t alloc)
{
	if (ZERO_OR_NULL_PTR(mem))
		return;

	alloc = alloc ?: &default_alloc;

	if (!size) {
		if (!alloc->ops->internally_sized.free) {
			pr_err("Leaking allocation 0x%p with unknown size\n", alloc);
			return;
		}

		return alloc->ops->internally_sized.free(mem, alloc->ctx);
	}

	return alloc->ops->externally_sized.free(mem, size, alloc->ctx);
}
EXPORT_SYMBOL(free_a);

/**
 * talloc_usable_size() - Report the size of the tallocation
 *
 * @mem: pointer to previously talloc'ed memory chunk.
 *
 * Return: size of tallocation
 */
static size_t __malloc_usable_size_a(const void *mem, alloc_t alloc)
{
	return alloc->ops->internally_sized.malloc_usable_size(mem, alloc->ctx);
}

void free_sensitive_a(void *mem, size_t size, alloc_t alloc)
{
	alloc = alloc ?: &default_alloc;

	if (!size) {
		BUG_ON(!alloc->ops->internally_sized.malloc_usable_size);
		size = __malloc_usable_size_a(mem, alloc);
	}

	if (size)
		memzero_explicit(mem, size);

	free_a(mem, size, alloc);
}
EXPORT_SYMBOL(free_sensitive_a);

/**
 * talloc_stats() - Report the size of the tallocation
 *
 * @mem: pointer to previously talloc'ed memory chunk.
 */
void malloc_stats_a(alloc_t alloc)
{
	alloc = alloc ?: &default_alloc;
	if (alloc->ops->stats)
		alloc->ops->stats(alloc->ctx);
}
EXPORT_SYMBOL(malloc_stats_a);

/**
 * talloc_strdup() - Duplicate a string
 *
 * @parent: pointer to previously talloc'ed memory chunk from which this
 *          chunk depends, or NULL.
 * @str: string to duplicate
 *
 * Return: pointer to the duplicated string, or NULL if there was an error.
 */
char *strdup_a(const char *str, alloc_t alloc)
{
	size_t len = strlen(str) + 1;
	void *usr;

	usr = malloc_a(len, alloc);
	if (!usr)
		return NULL;

	return memcpy(usr, str, len);
}
EXPORT_SYMBOL(strdup_a);

/**
 * talloc_strdup_const() - Duplicate a string if not read-only
 *
 * @parent: pointer to previously talloc'ed memory chunk from which this
 *          chunk depends, or NULL.
 * @size: amount of memory requested (in bytes).
 *
 * Return: pointer to the allocated memory chunk, or NULL if there was an error.
 */
const char *strdup_const_a(const char *str, alloc_t alloc)
{
	if (is_barebox_rodata((ulong)str))
		return str;

	return strdup_a(str, alloc);
}
EXPORT_SYMBOL(strdup_const_a);

/**
 * free_const_a() - call free_a, unless read/only memory
 *
 * @mem: pointer to previously malloc_a'ed memory chunk.
 */
void free_const_a(const void *mem, size_t size, alloc_t alloc)
{
	if (is_barebox_rodata((ulong)mem))
		return;

	free_a((void *)mem, size, alloc);
}
EXPORT_SYMBOL(free_const_a);

void strfree_const_a(const char *str, alloc_t alloc)
{
	if (is_barebox_rodata((ulong)str))
		return;

	alloc = alloc ?: &default_alloc;

	if (alloc->ops->internally_sized.free)
		return alloc->ops->internally_sized.free((char *)str, alloc->ctx);

	return alloc->ops->externally_sized.free((char *)str,
						 strlen(str) + 1, alloc->ctx);
}
EXPORT_SYMBOL(strfree_const_a);

static void *noop_malloc(size_t size, void *ctx)
{
	pr_err("%s(%zu)\n", __func__, size);
	return NULL;
}

static void noop_free(void *ptr, void *ctx)
{
	pr_err("%s(0x%p)\n", __func__, ptr);
}

static void *noop_realloc(void *ptr, size_t size, void *ctx)
{
	pr_err("%s(0x%p, %zu)\n", __func__, ptr, size);
	return NULL;
}

static struct allocator_ops noop_alloc_ops = {
	.malloc = noop_malloc,
	.internally_sized.free = noop_free,
	.internally_sized.realloc = noop_realloc,
};

struct allocator default_alloc __weak= {
	.ops = &noop_alloc_ops,
};
