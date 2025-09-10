/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __X86_ASM_CACHE_H_
#define __X86_ASM_CACHE_H_

static inline void sync_caches_for_execution_anywhere(void)
{
	unsigned int eax = 0, ebx, ecx, edx;
	/*
	 * cpuid flushes icache, dcache maintenance is unnecessary
	 * if we we only use temporal stores
	 */
	asm volatile ("cpuid"
		      : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
		      : "a"(eax) : "memory");
}

#endif
