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
extern int barebox_errno;

void barebox_malloc_stats(void)
{
}

void *barebox_memalign(size_t alignment, size_t bytes)
{
	void *mem;

	if (!bytes)
		return ZERO_SIZE_PTR;

	mem = memalign(alignment, bytes);
	if (!mem)
		barebox_errno = BAREBOX_ENOMEM;

	return mem;
}

void *barebox_malloc(size_t size)
{

	void *mem;

	if (!size)
		return ZERO_SIZE_PTR;

	mem = malloc(size);
	if (!mem)
		barebox_errno = BAREBOX_ENOMEM;

	return mem;
}

size_t barebox_malloc_usable_size(void *mem)
{
	if (ZERO_OR_NULL_PTR(mem))
		return 0;
	return malloc_usable_size(mem);
}

void barebox_free(void *ptr)
{
	if (ZERO_OR_NULL_PTR(ptr))
		return;
	free(ptr);
}

void *barebox_realloc(void *ptr, size_t size)
{
	void *mem;

	if (!size) {
		free(ptr);
		return ZERO_SIZE_PTR;
	}

	mem = realloc(ptr, size);
	if (!mem)
		barebox_errno = BAREBOX_ENOMEM;

	return mem;
}

void *barebox_calloc(size_t n, size_t elem_size)
{
	void *mem;

	if (n * elem_size == 0)
		return ZERO_SIZE_PTR;

	mem = calloc(n, elem_size);
	if (!mem)
		barebox_errno = BAREBOX_ENOMEM;

	return mem;
}
