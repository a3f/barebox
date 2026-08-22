/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: 2026 Ahmad Fatoum */

#ifndef __ASM_GENERIC_REGISTERS_H
#define __ASM_GENERIC_REGISTERS_H

#include <linux/compiler.h>
#include <asm/common.h>

/*
 * pc_inside - is the program counter inside the region at @start?
 *
 * The subtraction wraps for a PC below @start, so a single unsigned
 * comparison covers both ends and, unlike start + size, doesn't wrap for
 * a region reaching the end of the address space.
 */
static __always_inline bool pc_inside(unsigned long start, unsigned long size)
{
	unsigned long pc = get_pc();

	return pc - start < size;
}

/*
 * call_with_stack - switch to the stack ending at stack_top and call fn
 *
 * stack_top must be aligned as the architecture requires and fn must not
 * return. No memory is accessed, so there needn't be a stack yet.
 */
void __noreturn call_with_stack(unsigned long stack_top,
				void (*fn)(unsigned long, unsigned long),
				unsigned long arg0, unsigned long arg1);

#endif
