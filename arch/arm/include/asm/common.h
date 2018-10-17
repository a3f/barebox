#ifndef __ASM_ARM_COMMON_H
#define __ASM_ARM_COMMON_H

static inline unsigned long get_pc(void)
{
	unsigned long pc;
#ifdef CONFIG_CPU_32
	__asm__ __volatile__(
                "mov    %0, pc\n"
                : "=r" (pc)
                :
                : "memory");
#else
	__asm__ __volatile__(
                "adr    %0, .\n"
                : "=r" (pc)
                :
                : "memory");
#endif
	return pc;
}

static inline unsigned long get_lr(void)
{
	unsigned long lr;

	__asm__ __volatile__(
                "mov    %0, lr\n"
                : "=r" (lr)
                :
                : "memory");

	return lr;
}

static inline unsigned long get_sp(void)
{
	unsigned long sp;

	__asm__ __volatile__(
                "mov    %0, sp\n"
                : "=r" (sp)
                :
                : "memory");

	return sp;
}

/*
 * At least with GCC 7.3.1, listing sp in the clobber register can generate
 * erroneous accesses to the fp, even in __naked functions. Therefore _always_
 * branch to a noinline function as soon as possible (usually after relocation
 * and c_setup).
 */
static inline void arm_setup_stack(unsigned long top)
{
	__asm__ __volatile__("mov sp, %0"
			     :
			     : "r"(top)
			     : "sp");
}

#endif /* __ASM_ARM_COMMON_H */
