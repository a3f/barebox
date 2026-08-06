/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __MTDRAM_H_
#define __MTDRAM_H_

#include <linux/types.h>

struct mtdram;
struct mtd_info;

struct mtdram *mtdram_init(size_t size, size_t erasesize, size_t writesize);
void mtdram_free(struct mtdram *mtdram);

struct mtd_info *mtdram_get_mtd(struct mtdram *mtdram);

void mtdram_setup(struct mtdram *mtdram, const void *data, size_t size);

#endif
