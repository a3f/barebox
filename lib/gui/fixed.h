/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef FIXED_H_
#define FIXED_H_

#include <linux/types.h>

// Fixed-point math routines (signed 19.12)
// Largest positive value:  524287.999755859375
// Smallest positive value: 0.000244140625

#define FRAC_BITS      12
#define INT_MASK       0x7FFFF000 // 20 bits
#define FIXED_1        4096    // (1 << FRAC_BITS)
#define FIXED_255      1044480 // (255 << FRAC_BITS)

typedef s32 fixed_t;

static inline fixed_t int_to_fixed(s32 x)
{
	return x << FRAC_BITS;
}

static inline s32 fixed_to_int(fixed_t x)
{
	return x >> FRAC_BITS;
}

static inline fixed_t fixed_mul(fixed_t x, fixed_t y)
{
	return (fixed_t)(((s64)x * y) >> FRAC_BITS);
}

static inline fixed_t fixed_div(fixed_t x, fixed_t y)
{
	return (fixed_t)(((s64)x << FRAC_BITS) / y);
}

static inline fixed_t fixed_floor(fixed_t x)
{
	return x & INT_MASK;
}

#endif
