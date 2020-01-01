// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Ahmad Fatoum <a.fatoum@pengutronix.de>
 */

#include <common.h>
#include <malloc.h>

#undef memalign
#undef malloc
#undef free
#undef calloc
#undef realloc

void *memalign(size_t alignment, size_t bytes);
void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size);
void *calloc(size_t n, size_t elem_size);

void malloc_stats(void)
{
}

void *barebox_memalign(size_t alignment, size_t bytes)
{
	return memalign(alignment, bytes);
}

void *barebox_malloc(size_t size)
{
	return malloc(size);
}

void barebox_free(void *ptr)
{
	free(ptr);
}

void *barebox_realloc(void *ptr, size_t size)
{
	return realloc(ptr, size);
}

void *barebox_calloc(size_t n, size_t elem_size)
{
	return calloc(n, elem_size);
}
