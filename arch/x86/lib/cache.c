// SPDX-License-Identifier: GPL-2.0-only
#include <asm/cache.h>

void sync_caches_for_execution(void *addr, size_t size)
{
	sync_caches_for_execution_anywhere();
}
