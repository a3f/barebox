// SPDX-License-Identifier: GPL-2.0-only

#include <asm/cache.h>

void sync_caches_for_execution(void *addr, size_t size)
{
	__builtin___clear_cache(addr, addr + size);
}
