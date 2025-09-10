/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __ASM_GENERIC_CACHE_H_

#include <linux/types.h>

void sync_caches_for_execution(void *addr, size_t size);

#endif
