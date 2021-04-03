// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2012 Jan Luebbe <j.luebbe@pengutronix.de>

#define _BSD_SOURCE
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <endian.h>

static void usage(char *prgname)
{
	printf("usage: %s [OPTION] FILE > IMAGE\n"
	       "\n"
	       "options:\n"
	       "  -B           number of bytes to swap each time\n",
	       prgname);
}

int main(int argc, char *argv[])
{
	FILE *input;
	int opt;
	size_t size;
	unsigned long bytes = 0;
	uint32_t temp_u32;

	while((opt = getopt(argc, argv, "B:")) != -1) {
		switch (opt) {
		case 'B':
			bytes = strtoul(optarg, NULL, 0);
			break;
		}
	}

	if (optind >= argc) {
		usage(argv[0]);
		return 1;
	}

	if (bytes != 4) {
		fprintf(stderr, "-B %s unsupported\n", optarg);
		return 1;
	}

	input = fopen(argv[optind], "r");
	if (input == NULL) {
		perror("fopen");
		return 1;
	}

	for (;;) {
		size = fread(&temp_u32, 1, sizeof(uint32_t), input);
		if (!size)
			break;
		if (size < 4 && !feof(input)) {
			perror("fread");
			return 1;
		}

		temp_u32 = htobe32(le32toh(temp_u32));
		if (fwrite(&temp_u32, 1, sizeof(uint32_t), stdout) != 4) {
			perror("fwrite");
			return 1;
		}
	}

	if (fclose(input) != 0) {
		perror("fclose");
		return 1;
	}

	return 0;
}
