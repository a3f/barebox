#include <linux/linkage.h>
#include <init.h>

#ifdef CONFIG_CPU_ENDIAN_BE8
#define ARM_BE8(code...) code
#else
#define ARM_BE8(code...)
#endif


/*
 * dcache_line_size - get the minimum D-cache line size from the CTR register
 * on ARMv7.
 */
        .macro  dcache_line_size, reg, tmp
#ifdef CONFIG_CPU_V7M
        movw    \tmp, #:lower16:BASEADDR_V7M_SCB + V7M_SCB_CTR
        movt    \tmp, #:upper16:BASEADDR_V7M_SCB + V7M_SCB_CTR
        ldr     \tmp, [\tmp]
#else
        mrc     p15, 0, \tmp, c0, c0, 1         @ read ctr
#endif
        lsr     \tmp, \tmp, #16
        and     \tmp, \tmp, #0xf                @ cache line size encoding
        mov     \reg, #4                        @ bytes per word
        mov     \reg, \reg, lsl \tmp            @ actual cache line size
        .endm

/*
 * icache_line_size - get the minimum I-cache line size from the CTR register
 * on ARMv7.
 */
        .macro  icache_line_size, reg, tmp
#ifdef CONFIG_CPU_V7M
        movw    \tmp, #:lower16:BASEADDR_V7M_SCB + V7M_SCB_CTR
        movt    \tmp, #:upper16:BASEADDR_V7M_SCB + V7M_SCB_CTR
        ldr     \tmp, [\tmp]
#else
        mrc     p15, 0, \tmp, c0, c0, 1         @ read ctr
#endif
        and     \tmp, \tmp, #0xf                @ cache line size encoding
        mov     \reg, #4                        @ bytes per word
        mov     \reg, \reg, lsl \tmp            @ actual cache line size
        .endm


.section .text.v7_mmu_cache_on
#ifdef CONFIG_CPU_DCACHE_WRITETHROUGH
#define CB_BITS 0x08
#else
#define CB_BITS 0x0c
#endif

__setup_mmu:    sub     r3, r4, #16384          @ Page directory size
                bic     r3, r3, #0xff           @ Align the pointer
                bic     r3, r3, #0x3f00
/*
 * Initialise the page tables, turning on the cacheable and bufferable
 * bits for the RAM area only.
 */
                mov     r0, r3
                mov     r9, r0, lsr #18
                mov     r9, r9, lsl #18         @ start of RAM
                add     r10, r9, #0x10000000    @ a reasonable RAM size
                mov     r1, #0x12               @ XN|U + section mapping
                orr     r1, r1, #3 << 10        @ AP=11
                add     r2, r3, #16384
1:              cmp     r1, r9                  @ if virt > start of RAM
                cmphs   r10, r1                 @   && end of RAM > virt
                bic     r1, r1, #0x1c           @ clear XN|U + C + B
                orrlo   r1, r1, #0x10           @ Set XN|U for non-RAM
                orrhs   r1, r1, r6              @ set RAM section settings
                str     r1, [r0], #4            @ 1:1 mapping
                add     r1, r1, #1048576
                teq     r0, r2
                bne     1b
/*
 * If ever we are running from Flash, then we surely want the cache
 * to be enabled also for our execution instance...  We map 2MB of it
 * so there is no map overlap problem for up to 1 MB compressed kernel.
 * If the execution is in RAM then we would only be duplicating the above.
 */
                orr     r1, r6, #0x04           @ ensure B is set for this
                orr     r1, r1, #3 << 10
                mov     r2, pc
                mov     r2, r2, lsr #20
                orr     r1, r1, r2, lsl #20
                add     r0, r3, r2, lsl #2
                str     r1, [r0], #4
                add     r1, r1, #1048576
                str     r1, [r0]
                mov     pc, lr
ENDPROC(__setup_mmu)

ENTRY(v7_mmu_cache_on)
                mov     r12, lr
#ifdef CONFIG_MMU
                mrc     p15, 0, r11, c0, c1, 4  @ read ID_MMFR0
                tst     r11, #0xf               @ VMSA
                movne   r6, #CB_BITS | 0x02     @ !XN
                blne    __setup_mmu
                mov     r0, #0
                mcr     p15, 0, r0, c7, c10, 4  @ drain write buffer
                tst     r11, #0xf               @ VMSA
                mcrne   p15, 0, r0, c8, c7, 0   @ flush I,D TLBs
#endif
                mrc     p15, 0, r0, c1, c0, 0   @ read control reg
                bic     r0, r0, #1 << 28        @ clear SCTLR.TRE
                orr     r0, r0, #0x5000         @ I-cache enable, RR cache replacement
                orr     r0, r0, #0x003c         @ write buffer
                bic     r0, r0, #2              @ A (no unaligned access fault)
                orr     r0, r0, #1 << 22        @ U (v6 unaligned access model)
                                                @ (needed for ARM1176)
#ifdef CONFIG_MMU
 ARM_BE8(       orr     r0, r0, #1 << 25 )      @ big-endian page tables
                mrcne   p15, 0, r6, c2, c0, 2   @ read ttb control reg
                orrne   r0, r0, #1              @ MMU enabled
                movne   r1, #0xfffffffd         @ domain 0 = client
                bic     r6, r6, #1 << 31        @ 32-bit translation system
                bic     r6, r6, #(7 << 0) | (1 << 4)    @ use only ttbr0
                mcrne   p15, 0, r3, c2, c0, 0   @ load page table pointer
                mcrne   p15, 0, r1, c3, c0, 0   @ load domain access control
                mcrne   p15, 0, r6, c2, c0, 2   @ load ttb control
#endif
                mcr     p15, 0, r0, c7, c5, 4   @ ISB
                mcr     p15, 0, r0, c1, c0, 0   @ load control register
                mrc     p15, 0, r0, c1, c0, 0   @ and read it back
                mov     r0, #0
                mcr     p15, 0, r0, c7, c5, 4   @ ISB
                mov     pc, r12
ENDPROC(v7_mmu_cache_on)

.section .text.v7_mmu_cache_off
ENTRY(v7_mmu_cache_off)
                mrc     p15, 0, r0, c1, c0
#ifdef CONFIG_MMU
                bic     r0, r0, #0x000d
#else
                bic     r0, r0, #0x000c
#endif
                mcr     p15, 0, r0, c1, c0      @ turn MMU and cache off
                mov     r12, lr
                bl      v7_mmu_cache_flush
                mov     r0, #0
#ifdef CONFIG_MMU
                mcr     p15, 0, r0, c8, c7, 0   @ invalidate whole TLB
#endif
		mcr     p15, 0, r0, c7, c5, 0   @ invalidate I+BTB
                mcr     p15, 0, r0, c7, c5, 6   @ invalidate BTC
                mcr     p15, 0, r0, c7, c10, 4  @ DSB
                mcr     p15, 0, r0, c7, c5, 4   @ ISB
                mov     pc, r12
ENDPROC(v7_mmu_cache_off)

.section .text.v7_mmu_cache_flush_invalidate
ENTRY(v7_mmu_cache_invalidate)
		mov	r0, #1
		b	__v7_mmu_cache_flush_invalidate
ENDPROC(v7_mmu_cache_invalidate)

ENTRY(__v7_mmu_cache_flush_invalidate)
		dmb
		mrc	p15, 0, r12, c0, c1, 5	@ read ID_MMFR1
		tst	r12, #0xf << 16		@ hierarchical cache (ARMv7)
		mov	r12, #0
		beq	_hierarchical
		mcr	p15, 0, r12, c7, c14, 0	@ clean+invalidate D
		b	_out
_hierarchical:
		stmfd	sp!, {r4-r11}
		mov	r8, r0
		dmb
		mrc	p15, 1, r0, c0, c0, 1	@ read clidr
		ands	r3, r0, #0x7000000	@ extract loc from clidr
		mov	r3, r3, lsr #23		@ left align loc bit field
		beq	_finished		@ if loc is 0, then no need to clean
		cmp	r8, #0
THUMB(		ite	eq			)
		moveq	r12, #0
		subne	r12, r3, #2		@ start invalidate at outmost cache level
_loop1:
		add	r2, r12, r12, lsr #1	@ work out 3x current cache level
		mov	r1, r0, lsr r2		@ extract cache type bits from clidr
		and	r1, r1, #7		@ mask of the bits for current cache only
		cmp	r1, #2			@ see what cache we have at this level
		blt	_skip			@ skip if no cache, or just i-cache
		mcr	p15, 2, r12, c0, c0, 0	@ select current cache level in cssr
		isb				@ isb to sych the new cssr&csidr
		mrc	p15, 1, r1, c0, c0, 0	@ read the new csidr
		and	r2, r1, #7		@ extract the length of the cache lines
		add	r2, r2, #4		@ add 4 (line length offset)
		ldr	r4, =0x3ff
		ands	r4, r4, r1, lsr #3	@ find maximum number on the way size
		clz	r5, r4			@ find bit position of way size increment
		ldr	r7, =0x7fff
		ands	r7, r7, r1, lsr #13	@ extract max number of the index size
_loop2:
		mov	r9, r4			@ create working copy of max way size
_loop3:
ARM(		orr	r11, r12, r9, lsl r5	) @ factor way and cache number into r11
ARM(		orr	r11, r11, r7, lsl r2	) @ factor index number into r11
THUMB(		lsl	r6, r9, r5		)
THUMB(		orr	r11, r12, r6		) @ factor way and cache number into r11
THUMB(		lsl	r6, r7, r2		)
THUMB(		orr	r11, r11, r6		) @ factor index number into r11
		cmp	r8, #0
THUMB(		ite	eq			)
		mcreq	p15, 0, r11, c7, c14, 2	@ clean & invalidate by set/way
		mcrne	p15, 0, r11, c7, c6, 2	@ invalidate by set/way
		subs	r9, r9, #1		@ decrement the way
		bge	_loop3
		subs	r7, r7, #1		@ decrement the index
		bge	_loop2
_skip:
		cmp	r8, #0
		bne	_inval_check
		add	r12, r12, #2		@ increment cache number
		cmp	r3, r12
		b	_loop_end_check
_inval_check:
		cmp	r12, #0
		sub	r12, r12, #2		@ decrement cache number
_loop_end_check:
		dsb				@ work-around Cortex-A7 erratum 814220
		bgt	_loop1
_finished:
		ldmfd	sp!, {r4-r11}
		mov	r12, #0			@ switch back to cache level 0
		mcr	p15, 2, r12, c0, c0, 0	@ select current cache level in cssr
_out:
		dsb
		isb
		mov	pc, lr
ENDPROC(__v7_mmu_cache_flush_invalidate)

ENTRY(v7_invalidate_l1_linux)
       mov     r0, #0
       mcr     p15, 2, r0, c0, c0, 0
       mrc     p15, 1, r0, c0, c0, 0
       ldr     r1, =0x7fff
       and     r2, r1, r0, lsr #13
       ldr     r1, =0x3ff
       and     r3, r1, r0, lsr #3      @ NumWays - 1
       add     r2, r2, #1              @ NumSets
       and     r0, r0, #0x7
       add     r0, r0, #4      @ SetShift
       clz     r1, r3          @ WayShift
       add     r4, r3, #1      @ NumWays
1:     sub     r2, r2, #1      @ NumSets--
       mov     r3, r4          @ Temp = NumWays
2:     subs    r3, r3, #1      @ Temp--
       mov     r5, r3, lsl r1
       mov     r6, r2, lsl r0
       orr     r5, r5, r6      @ Reg = (Temp<<WayShift)|(NumSets<<SetShift)
       mcr     p15, 0, r5, c7, c6, 2
       bgt     2b
       cmp     r2, #0
       bgt     1b
       dsb     st
       isb
       mov     pc, lr
ENDPROC(v7_invalidate_l1_linux)

ENTRY(v7_mmu_cache_flush)
                tst     r4, #1
                bne     iflush
                mrc     p15, 0, r10, c0, c1, 5  @ read ID_MMFR1
                tst     r10, #0xf << 16         @ hierarchical cache (ARMv7)
                mov     r10, #0
                beq     hierarchical
                mcr     p15, 0, r10, c7, c14, 0 @ clean+invalidate D
                b       iflush
hierarchical:
                mcr     p15, 0, r10, c7, c10, 5 @ DMB
                stmfd   sp!, {r0-r7, r9-r11}
                mrc     p15, 1, r0, c0, c0, 1   @ read clidr
                ands    r3, r0, #0x7000000      @ extract loc from clidr
                mov     r3, r3, lsr #23         @ left align loc bit field
                beq     finished                @ if loc is 0, then no need to clean
                mov     r10, #0                 @ start clean at cache level 0
loop1:
                add     r2, r10, r10, lsr #1    @ work out 3x current cache level
                mov     r1, r0, lsr r2          @ extract cache type bits from clidr
                and     r1, r1, #7              @ mask of the bits for current cache only
                cmp     r1, #2                  @ see what cache we have at this level
                blt     skip                    @ skip if no cache, or just i-cache
                mcr     p15, 2, r10, c0, c0, 0  @ select current cache level in cssr
                mcr     p15, 0, r10, c7, c5, 4  @ isb to sych the new cssr&csidr
                mrc     p15, 1, r1, c0, c0, 0   @ read the new csidr
                and     r2, r1, #7              @ extract the length of the cache lines
                add     r2, r2, #4              @ add 4 (line length offset)
                ldr     r4, =0x3ff
                ands    r4, r4, r1, lsr #3      @ find maximum number on the way size
                clz     r5, r4                  @ find bit position of way size increment
                ldr     r7, =0x7fff
                ands    r7, r7, r1, lsr #13     @ extract max number of the index size
loop2:
                mov     r9, r4                  @ create working copy of max way size
loop3:
 ARM(           orr     r11, r10, r9, lsl r5    ) @ factor way and cache number into r11
 ARM(           orr     r11, r11, r7, lsl r2    ) @ factor index number into r11
 THUMB(         lsl     r6, r9, r5              )
 THUMB(         orr     r11, r10, r6            ) @ factor way and cache number into r11
 THUMB(         lsl     r6, r7, r2              )
 THUMB(         orr     r11, r11, r6            ) @ factor index number into r11
                mcr     p15, 0, r11, c7, c14, 2 @ clean & invalidate by set/way
                subs    r9, r9, #1              @ decrement the way
                bge     loop3
                subs    r7, r7, #1              @ decrement the index
                bge     loop2
skip:
                add     r10, r10, #2            @ increment cache number
                cmp     r3, r10
                bgt     loop1
finished:
                ldmfd   sp!, {r0-r7, r9-r11}
                mov     r10, #0                 @ switch back to cache level 0
                mcr     p15, 2, r10, c0, c0, 0  @ select current cache level in cssr
iflush:
                mcr     p15, 0, r10, c7, c10, 4 @ DSB
                mcr     p15, 0, r10, c7, c5, 0  @ invalidate I+BTB
                mcr     p15, 0, r10, c7, c10, 4 @ DSB
                mcr     p15, 0, r10, c7, c5, 4  @ ISB
                mov     pc, lr
ENDPROC(v7_mmu_cache_flush)

ENTRY(v7_coherent_kern_range_linux)
/*
 *	v7_coherent_kern_range_linux(start,end)
 *
 *	Ensure that the I and D caches are coherent within specified
 *	region.  This is typically used when code has been written to
 *	a memory region, and will be executed.
 *
 *	- start   - virtual start address of region
 *	- end     - virtual end address of region
 *
 *	It is assumed that:
 *	- the Icache does not read data from the write buffer
 */
#define USER(x...) x
#define ALT_SMP(x...)
#define ALT_UP(x...) x
	dcache_line_size r2, r3
	sub	r3, r2, #1
	bic	r12, r0, r3
#ifdef CONFIG_ARM_ERRATA_764369
	ALT_SMP(W(dsb))
	ALT_UP(W(nop))
#endif
1:
 USER(	mcr	p15, 0, r12, c7, c11, 1	)	@ clean D line to the point of unification
	add	r12, r12, r2
	cmp	r12, r1
	blo	1b
	dsb
	icache_line_size r2, r3
	sub	r3, r2, #1
	bic	r12, r0, r3
2:
 USER(	mcr	p15, 0, r12, c7, c5, 1	)	@ invalidate I line
	add	r12, r12, r2
	cmp	r12, r1
	blo	2b
	mov	r0, #0
	ALT_SMP(mcr	p15, 0, r0, c7, c1, 6)	@ invalidate BTB Inner Shareable
	ALT_UP(mcr	p15, 0, r0, c7, c5, 6)	@ invalidate BTB
	dsb
	isb
	mov	pc, lr
ENDPROC(v7_coherent_kern_range_linux)

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

