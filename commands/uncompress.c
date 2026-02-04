// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: © 2010 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix

/* uncompress.c - uncompress a compressed file */

#include <common.h>
#include <command.h>
#include <errno.h>
#include <fcntl.h>
#include <fs.h>
#include <uncompress.h>
#include <loadable.h>
#include <linux/kstrtox.h>

static int uncompress_mem(const char *src_file, const char *dst_file,
			  loff_t src_start, loff_t src_size,
			  loff_t dst_start, loff_t dst_size)
{
	struct loadable *l, *decompressed;
	ssize_t written;
	int fd;

	if (!IS_ENABLED(CONFIG_CMD_UNCOMPRESS_MEMORY)) {
		printf("uncompress: memory support not compiled in\n");
		return -ENOSYS;
	}

	/* Create loadable from source */
	if (src_start != -1) {
		if (src_size == (loff_t)~0) {
			printf("source size must be specified\n");
			return 1;
		}

		l = loadable_from_mem((void *)(uintptr_t)src_start, src_size,
				      LOADABLE_UNSPECIFIED);
	} else {
		l = loadable_from_file(src_file, LOADABLE_UNSPECIFIED);
	}

	if (IS_ERR(l)) {
		printf("failed to create loadable: %pe\n", l);
		return 1;
	}

	/* Wrap with decompression */
	decompressed = loadable_decompress(l);

	if (dst_start != -1) {
		if (dst_size == (loff_t)~0) {
			printf("destination size must be specified\n");
			loadable_release(&decompressed);
			return 1;
		}

		written = loadable_extract_into_buf_full(decompressed,
							 (void *)(uintptr_t)dst_start,
							 dst_size);
	} else {
		fd = open(dst_file, O_WRONLY | O_CREAT);
		if (fd < 0) {
			perror("open");
			loadable_release(&decompressed);
			return 1;
		}

		written = loadable_extract_into_fd(decompressed, fd);
		close(fd);
	}

	loadable_release(&decompressed);

	if (written < 0) {
		printf("failed to extract: %s\n", strerror(-written));
		return 1;
	}

	return 0;
}


static int do_uncompress(int argc, char *argv[])
{
	int from, to, ret;
	loff_t src_start = -1, src_size = -1;
	loff_t dst_start = -1, dst_size = -1;
	bool src_is_mem, dst_is_mem;

	if (argc != 3)
		return COMMAND_ERROR_USAGE;

	src_is_mem = !parse_area_spec(argv[1], &src_start, &src_size);
	dst_is_mem = !parse_area_spec(argv[2], &dst_start, &dst_size);

	if (src_is_mem || dst_is_mem)
		return uncompress_mem(argv[1], argv[2],
				      src_start, src_size,
				      dst_start, dst_size);

	from = open(argv[1], O_RDONLY);
	if (from < 0) {
		perror("open");
		return 1;
	}

	to = open(argv[2], O_WRONLY | O_CREAT);
	if (to < 0) {
		perror("open");
		ret = 1;
		goto exit_close;
	}

	ret = uncompress_fd_to_fd(from, to, uncompress_err_stdout);

	if (ret)
		printf("failed to decompress\n");

	close(to);
exit_close:
	close(from);
	return ret;
}


BAREBOX_CMD_HELP_START(uncompress)
BAREBOX_CMD_HELP_TEXT("Uncompress INFILE to OUTFILE.")
#ifdef CONFIG_CMD_UNCOMPRESS_MEMORY
BAREBOX_CMD_HELP_TEXT("Both arguments can be either file paths or memory areas")
BAREBOX_CMD_HELP_TEXT("(e.g., 0x10000000+0x100000).")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("Memory areas are specified as START+SIZE or START-END.")
#endif
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(uncompress)
	.cmd            = do_uncompress,
	BAREBOX_CMD_DESC("uncompress a compressed file")
	BAREBOX_CMD_OPTS("INFILE OUTFILE")
	BAREBOX_CMD_GROUP(CMD_GRP_FILE)
	BAREBOX_CMD_HELP(cmd_uncompress_help)
BAREBOX_CMD_END
