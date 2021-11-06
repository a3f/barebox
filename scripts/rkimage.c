// SPDX-License-Identifier: GPL-2.0
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "common.c"
#include "rkimage.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#define ALIGN(x, a)        (((x) + (a) - 1) & ~((a) - 1))

struct option cbootcmd[] = {
	{"help", 0, NULL, 'h'},
	{"soc", 1, NULL, 's'},
	{"o", 1, NULL, 'o'},
	{0, 0, 0, 0},
};

static void usage(const char *prgname)
{
	printf(
"Usage: %s [OPTIONS] <FILES>\n"
"\n"
"Generate a Rockchip boot image from <FILES>\n"
"\n"
"Options:\n"
"  -o <file>   Output image to <file>\n"
"  -s <soc>    target SoC\n"
"  -h          This help\n",
	prgname);
}

static struct soc_info {
	const char *name;
	int (*handler)(enum rksoc, const char *outfile, int argc, char *argv[]);
} socs[] = {
	[RK3568] = { "rk3568", handle_rk3568 },
};

int main(int argc, char *argv[])
{
	int opt, i = RK3568;
	const char *outfile = NULL;

	while ((opt = getopt_long(argc, argv, "s:ho:", cbootcmd, NULL)) > 0) {
		switch (opt) {
		case 'o':
			outfile = optarg;
			break;
		case 's':
			for (i = 0; i < ARRAY_SIZE(socs); i++) {
				if (socs[i].name && strcmp(socs[i].name, optarg) == 0)
					break;
			}

			if (i == ARRAY_SIZE(socs)) {
				printf("Unsupported SoC type. Supported are: ");
				for (i = 0; i < ARRAY_SIZE(socs); i++)
					printf("%s ", socs[i].name);
				printf("\n");
				exit(1);
			}
			break;
		case 'h':
			usage(argv[0]);
			exit(0);
		}
	}

	if (i < 0 || !outfile) {
		usage(argv[0]);
		exit(0);
	}

	argc -= optind;
	argv += optind;

	if (!argc) {
		usage(argv[0]);
		exit(1);
	}

	return socs[i].handler(i, outfile, argc, argv);
}
