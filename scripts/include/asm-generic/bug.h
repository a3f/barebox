/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _ASM_GENERIC_BUG_H
#define _ASM_GENERIC_BUG_H

#include <printk.h>

#define BUG() do { \
	printf("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __FUNCTION__); \
	panic("BUG!"); \
} while (0)

#endif
