// SPDX-License-Identifier: GPL-2.0

#include <malloc.h>
#include <linux/compiler.h>

void * __must_check __realloc_size(2)
rust_helper_realloc(const void *ptr, size_t new_size)
{
	return realloc((void *)ptr, new_size);
}
