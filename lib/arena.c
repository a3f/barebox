// SPDX-License-Identifier: GPL-2.0
//
#include <linux/types.h>
#include <linux/string.h>
#include <linux/align.h>
#include <linux/minmax.h>
#include <printf.h>
#include <alloc.h>

struct arena_ctx {
	u8 *start;
	u32 size;
	u32 brk;
} __aligned(CONFIG_MALLOC_ALIGNMENT);

static void *arena_memalign(size_t align, size_t size, void *ctx)
{
	struct arena_ctx *a = ctx;
	u32 brk = ALIGN(a->brk, max_t(size_t, CONFIG_MALLOC_ALIGNMENT, align));

	if (brk + size > a->size)
		return NULL;

	a->brk = brk + size;
	return a->start + brk;
}

static void *arena_malloc(size_t size, void *ctx)
{
	return arena_memalign(CONFIG_MALLOC_ALIGNMENT, size, ctx);
}

static void arena_free(void *ptr, size_t size, void *ctx)
{
	struct arena_ctx *a = ctx;

	if (ptr + size == a->start + a->brk)
		a->brk -= size;
}

static void *arena_realloc(void *ptr, size_t oldsize, size_t newsize, void *ctx)
{
	struct arena_ctx *a = ctx;
	void *mem;

	if (ptr + oldsize == a->start + a->brk) {
		a->brk += newsize - oldsize;
		return ptr;
	}

	mem = arena_malloc(newsize, ctx);
	if (!mem)
		return NULL;

	memcpy(mem, ptr, oldsize);
	return mem;
}

static void arena_stats(void *ctx)
{
	struct arena_ctx *a = ctx;
	printf("arena %p arena: %u/%u bytes used\n", a->start,
	       a->brk, a->size);
}

static const struct allocator_ops arena_ops = {
	.malloc = arena_malloc,
	.memalign = arena_memalign,
	.stats = arena_stats,
	.externally_sized.free = arena_free,
	.externally_sized.realloc = arena_realloc,
};

/*
 * arena_arena() - create allocator in place
 * @buf:  backing buffer
 * @size: total size
 *
 * Layout:
 * [ struct allocator ][ struct arena_ctx ][ user area... ]
 */
alloc_t new_arena(void *buf, size_t size)
{
	struct allocator *a = buf;
	struct arena_ctx *ctx = PTR_ALIGN(buf + sizeof(*a),
					  __alignof__(struct arena_ctx));
	size_t pad = (void *)ctx + sizeof(*ctx) - buf;

	if (pad >= size)
		return NULL;

	ctx->start = buf;
	ctx->brk = pad;
	ctx->size = size - ctx->brk;

	a->ops = &arena_ops;
	a->ctx = ctx;
	return a;
}

#ifdef CONFIG_MALLOC_DUMMY
#include <malloc.h>

static void arena_leak(void *ptr, void *ctx)
{
	/* The dummy allocator is expected and documented to leak, so we define
	 * .internally_sized.free here to suppress the warning messages
	 */
}

static const struct allocator_ops default_alloc_ops = {
	.malloc = arena_malloc,
	.memalign = arena_memalign,
	.stats = arena_stats,
	.internally_sized.free = arena_leak,
	.externally_sized.free = arena_free,
	.externally_sized.realloc = arena_realloc,
};

struct allocator default_alloc = {
	.ops = &arena_ops,
};

void malloc_add_pool(void *mem, size_t size)
{
	struct arena_ctx *ctx = PTR_ALIGN(mem, __alignof__(struct arena_ctx));
	size_t pad = (void *)ctx + sizeof(*ctx) - mem;

	if (pad >= size)
		return;

	ctx->start = mem;
	ctx->brk = pad;
	ctx->size = size - ctx->brk;

	default_alloc.ops = &default_alloc_ops;
	default_alloc.ctx = ctx;
}
#endif
