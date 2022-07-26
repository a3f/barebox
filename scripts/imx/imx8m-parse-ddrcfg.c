// SPDX-License-Identifier: GPL-2.0+

#include <stdio.h>
#include <stdlib.h>
#include "../compiler.h"
#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/sizes.h>
#include <ctype.h>
#include "../common.h"

#include "../common.c"

unsigned int cols, rows, banks, width;

static inline u64 memory_sdram_size(unsigned int _cols,
				    unsigned int _rows,
				    unsigned int _banks,
				    unsigned int _width)
{
	cols =_cols;
	rows =_rows;
	banks =_banks;
	width =_width;
	return (u64)banks * width << (rows + cols);
}

#define __iomem
#define readl(addr) (*(u32 *)(addr))
#define BUG()	exit(5)

#include "../../arch/arm/mach-imx/esdctl.c"

#define DRAM_TYPE_MASK	0x3d

const char *dram_type[DRAM_TYPE_MASK] = {
	[0x01] = "ddr3",
	[0x04] = "lpddr2",
	[0x08] = "lpddr3",
	[0x10] = "ddr4",
	[0x20] = "lpddr4",
};

int main(int argc, char *argv[])
{
	const char *type;
	unsigned long long size;
	char suffix;
	uint32_t *ddrcfg, *mmdc, *p;
	ssize_t ret;

	ddrcfg = malloc(0x4001);
	mmdc = malloc(0x4001);

	ret = read_full(0, ddrcfg, 0x4001);
	if (ret < 0) {
		perror("read_full");
		return 1;
	}

	if (ret % 8 != 0) {
		fprintf(stderr, "invalid DDR configuration\n");
		return 2;
	}

	for (p = ddrcfg; p < &ddrcfg[ret / sizeof(u32)]; p += 2) {
		u32 addr = p[0] - 0x3d400000;
		if (addr >= 0x4000 || addr % 4 != 0) {
			fprintf(stderr, "invalid value 0x%08x in DDR configuration\n", *p);
			return 4;
		}
		mmdc[addr / sizeof(u32)] = p[1];
	}

	size = imx8m_ddrc_sdram_size(mmdc, 16);
	printf("cols = %u, rows = %u, banks = %u, width = %u => size = %lluM\n",
	       cols, rows, banks, width, size / 1024 / 1024);

	suffix = tolower(format_size_exact(&size));

	type = dram_type[readl(mmdc) & DRAM_TYPE_MASK] ?: "ddr";

	printf("%s_ddrc_cfg_%llu%c_%ur%ucx%u\n", type, size, suffix, rows, cols, width * 8);

	return 0;
}
