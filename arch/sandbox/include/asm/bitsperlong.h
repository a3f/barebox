#ifndef __ASM_BITSPERLONG_H
#define __ASM_BITSPERLONG_H

#ifdef __x86_64__
#define BITS_PER_LONG 64
#else
#define BITS_PER_LONG 32
#endif

#define BITS_PER_LONG_LONG 64

#endif /* __ASM_BITSPERLONG_H */
