/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __ARM_ASM_MMUINFO_H__
#define __ARM_ASM_MMUINFO_H__

enum mmuinfo;

int mmuinfo_v7(enum mmuinfo, void *addr);
int mmuinfo_v8(enum mmuinfo, void *addr);

#endif
