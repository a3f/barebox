/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _LINUX_GFP_H
#define _LINUX_GFP_H

/* unused in barebox, just bogus values */
#define GFP_KERNEL	0
#define GFP_NOFS	0
#define GFP_USER	0
#define GFP_ATOMIC	0
#define __GFP_NOWARN	0

#define ___GFP_LAST_BIT 0

/* Room for N __GFP_FOO bits */
#define __GFP_BITS_SHIFT ___GFP_LAST_BIT
#define __GFP_BITS_MASK ((__force gfp_t)((1 << __GFP_BITS_SHIFT) - 1))

#endif
