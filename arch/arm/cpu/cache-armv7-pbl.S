#include <linux/linkage.h>
#include <init.h>

#define ADDR_START 0x0000
#define ADDR_END   0xffff

.section .text.v7_mmu_cache_on
ENTRY(v7_mmu_cache_on)
		stmfd	sp!, {r11, lr}
		mov	r12, lr
#if 0
		movw	r0, #0
		movt	r0, #ADDR_START
		movw	r1, #0
		movt	r1, #ADDR_END
		bl	v7_dma_inv_range
#ifdef CONFIG_MMU
		mcr	p15, 0, r0, c8, c7, 0	@ invalidate whole TLB
#endif
		mcr     p15, 0, r0, c7, c5, 0   @ invalidate I+BTB
		mcr	p15, 0, r0, c7, c5, 6	@ invalidate BTC
#endif
#ifdef CONFIG_MMU
		mrc	p15, 0, r11, c0, c1, 4	@ read ID_MMFR0
		tst	r11, #0xf		@ VMSA
		mov	r0, #0
		dsb				@ drain write buffer
		tst	r11, #0xf		@ VMSA
		mcrne	p15, 0, r0, c8, c7, 0	@ flush I,D TLBs
#endif
		mrc	p15, 0, r0, c1, c0, 0	@ read control reg
		orr	r0, r0, #0x5000		@ I-cache enable, RR cache replacement
		orr	r0, r0, #0x003c		@ write buffer
#ifdef CONFIG_MMU
#ifdef CONFIG_CPU_ENDIAN_BE8
		orr	r0, r0, #1 << 25	@ big-endian page tables
#endif
		orrne	r0, r0, #1		@ MMU enabled
		movne	r1, #-1
		mcrne	p15, 0, r1, c3, c0, 0	@ load domain access control
#endif
		isb
		mcr	p15, 0, r0, c1, c0, 0	@ load control register
		mrc	p15, 0, r0, c1, c0, 0	@ and read it back
		mov	r0, #0
		isb
		ldmfd	sp!, {r11, pc}
ENDPROC(v7_mmu_cache_on)

.section .text.v7_mmu_cache_off
ENTRY(v7_mmu_cache_off)
		/* although 'r12' is an eabi scratch register which does
		   not need to be restored, save it to ensure an 8-byte
	           stack alignment */
		stmfd	sp!, {r4-r12, lr}
		mrc	p15, 0, r0, c1, c0
#ifdef CONFIG_MMU
		bic	r0, r0, #0x000d
#else
		bic	r0, r0, #0x000c
#endif
		mcr	p15, 0, r0, c1, c0	@ turn MMU and cache off
		bl	v7_mmu_cache_flush
		mov	r0, #0
#ifdef CONFIG_MMU
		mcr	p15, 0, r0, c8, c7, 0	@ invalidate whole TLB
#endif
		mcr     p15, 0, r0, c7, c5, 0   @ invalidate I+BTB
		mcr	p15, 0, r0, c7, c5, 6	@ invalidate BTC
		dsb
		isb
		ldmfd	sp!, {r4-r12, pc}
ENDPROC(v7_mmu_cache_off)

.section .text.v7_mmu_cache_flush_invalidate
ENTRY(v7_mmu_cache_invalidate)
		movw	r0, #0
		movt	r0, #ADDR_START
		movw	r1, #0
		movt	r1, #ADDR_END
		b	v7_dma_inv_range
#ifdef CONFIG_MMU
		mcr	p15, 0, r0, c8, c7, 0	@ invalidate whole TLB
#endif
		mcr     p15, 0, r0, c7, c5, 0   @ invalidate I+BTB
		mcr	p15, 0, r0, c7, c5, 6	@ invalidate BTC
ENDPROC(v7_mmu_cache_invalidate)

ENTRY(v7_mmu_cache_flush)
		movw	r0, #0
		movt	r0, #ADDR_START
		movw	r1, #0
		movt	r1, #ADDR_END
		b	v7_dma_clean_range
#ifdef CONFIG_MMU
		mcr	p15, 0, r0, c8, c7, 0	@ invalidate whole TLB
#endif
		mcr     p15, 0, r0, c7, c5, 0   @ invalidate I+BTB
		mcr	p15, 0, r0, c7, c5, 6	@ invalidate BTC
ENDPROC(v7_mmu_cache_flush)

/*
 * cache_line_size - get the cache line size from the CSIDR register
 * (available on ARMv7+). It assumes that the CSSR register was configured
 * to access the L1 data cache CSIDR.
 */
	.macro	dcache_line_size, reg, tmp
	mrc	p15, 1, \tmp, c0, c0, 0		@ read CSIDR
	and	\tmp, \tmp, #7			@ cache line size encoding
	mov	\reg, #16			@ size offset
	mov	\reg, \reg, lsl \tmp		@ actual cache line size
	.endm

/*
 *	v7_dma_inv_range(start,end)
 *
 *	Invalidate the data cache within the specified region; we will
 *	be performing a DMA operation in this region and we want to
 *	purge old data in the cache.
 *
 *	- start   - virtual start address of region
 *	- end     - virtual end address of region
 */
.section .text.v7_dma_inv_range
ENTRY(v7_dma_inv_range)
	dcache_line_size r2, r3
	sub	r3, r2, #1
	tst	r0, r3
	bic	r0, r0, r3
	mcrne	p15, 0, r0, c7, c14, 1		@ clean & invalidate D / U line

	tst	r1, r3
	bic	r1, r1, r3
	mcrne	p15, 0, r1, c7, c14, 1		@ clean & invalidate D / U line
1:
	mcr	p15, 0, r0, c7, c6, 1		@ invalidate D / U line
	add	r0, r0, r2
	cmp	r0, r1
	blo	1b
	dsb
	mov	pc, lr
ENDPROC(v7_dma_inv_range)

/*
 *	v7_dma_clean_range(start,end)
 *	- start   - virtual start address of region
 *	- end     - virtual end address of region
 */
.section .text.v7_dma_clean_range
ENTRY(v7_dma_clean_range)
	dcache_line_size r2, r3
	sub	r3, r2, #1
	bic	r0, r0, r3
1:
	mcr	p15, 0, r0, c7, c10, 1		@ clean D / U line
	add	r0, r0, r2
	cmp	r0, r1
	blo	1b
	dsb
	mov	pc, lr
ENDPROC(v7_dma_clean_range)

/*
 *	v7_dma_flush_range(start,end)
 *	- start   - virtual start address of region
 *	- end     - virtual end address of region
 */
.section .text.v7_dma_flush_range
ENTRY(v7_dma_flush_range)
	dcache_line_size r2, r3
	sub	r3, r2, #1
	bic	r0, r0, r3
1:
	mcr	p15, 0, r0, c7, c14, 1		@ clean & invalidate D / U line
	add	r0, r0, r2
	cmp	r0, r1
	blo	1b
	dsb
	mov	pc, lr
ENDPROC(v7_dma_flush_range)
