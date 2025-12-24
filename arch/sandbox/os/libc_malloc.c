// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 Ahmad Fatoum <a.fatoum@pengutronix.de>
 */

#include <stdlib.h>
#include <malloc.h>

#define ZERO_SIZE_PTR ((void *)16)

#define ZERO_OR_NULL_PTR(x) ((unsigned long)(x) <= \
				(unsigned long)ZERO_SIZE_PTR)
#define BAREBOX_ENOMEM 12
#define BAREBOX_MALLOC_MAX_SIZE 0x40000000

extern int barebox_errno;

void *libc_memalign(size_t alignment, size_t bytes, void *ctx)
{
	if (alignment > BAREBOX_MALLOC_MAX_SIZE || bytes > BAREBOX_MALLOC_MAX_SIZE)
		return NULL;

	return memalign(alignment, bytes);
}

void *libc_malloc(size_t size, void *ctx)
{
	if (size > BAREBOX_MALLOC_MAX_SIZE)
		return NULL;

	return malloc(size);
}

void *libc_calloc(size_t n, size_t elem_size, void *ctx)
{
	size_t size;

	if (__builtin_mul_overflow(n, elem_size, &size) ||
	    size > BAREBOX_MALLOC_MAX_SIZE)
		return NULL;

	return calloc(n, elem_size);
}

size_t libc_malloc_usable_size(const void *mem)
{
	if (ZERO_OR_NULL_PTR(mem))
		return 0;
	return malloc_usable_size((void *)mem);
}

void libc_free(void *ptr)
{
	if (ZERO_OR_NULL_PTR(ptr))
		return;
	free(ptr);
}

void *libc_realloc(void *ptr, size_t size, void *ctx)
{
	if (size > BAREBOX_MALLOC_MAX_SIZE)
		return NULL;

	return realloc(ptr, size);
}

#ifdef CONFIG_DEBUG_MEMLEAK
void barebox_memleak_check(void)
{
	void __lsan_do_recoverable_leak_check(void);

	__lsan_do_recoverable_leak_check();
}
#endif
