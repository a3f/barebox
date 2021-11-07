// SPDX-License-Identifier: GPL-2.0
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include "common.h"
#include "compiler.h"
#include "rkimage.h"

#include "../crypto/rc4.c"
#include "../include/mach/rockchip/rkimage-rk30.h"

#define panic(fmt, ...) do { \
	fprintf(stderr, fmt ": %s\n", ##__VA_ARGS__, strerror(errno)); \
	exit(1); \
} while (0)

#define ALIGN(x, a)        (((x) + (a) - 1) & ~((a) - 1))

#define RK_SPL_RC4	(1 << 0)
#define RK_AARCH64	(1 << 1)

/**
 * struct rkheader - spl header info for each chip
 *
 * @spl_hdr:		Boot ROM requires a 4-bytes spl header
 * @spl_size:		Spl size(include extra 4-bytes spl header)
 * @spl_rc4:		RC4 encode the SPL binary (same key as header)
 */
struct rkheader {
	const char *magic;
	const uint32_t size;
	int flags;
};

static struct rkheader rkheaders[] = {
	[RK3036] = { "RK30", 0x1000, 0 },
	[RK3066] = { "RK30", 0x8000, RK_SPL_RC4 },
	[RK3128] = { "RK31", 0x1800, 0 },
	[PX3SE]  = { "RK31", 0x1800, 0 },
	[RK3188] = { "RK31", 0x8000 - 0x800, RK_SPL_RC4 },
	[RK322x] = { "RK32", 0x8000 - 0x1000, 0 },
	[RK3288] = { "RK32", 0x8000, 0 },
	[RK3308] = { "RK33", 0x40000 - 0x1000, RK_AARCH64},
	[RK3328] = { "RK32", 0x8000 - 0x800, RK_AARCH64 },
	[RK3368] = { "RK33", 0x8000 - 0x1000, RK_AARCH64 },
	[RK3399] = { "RK33", 0x30000 - 0x2000, RK_AARCH64 },
	[RK3326] = { "RK33", 0x4000 - 0x1000, RK_AARCH64 },
	[PX30]   = { "RK33", 0x3000, RK_AARCH64 },
	[RV1108] = { "RK11", 0x1800, 0 },
	[RV1126] = { "110B", 0xf000, 0 },
	[RK1808] = { "RK18", 0x1fe000, RK_AARCH64 },
};

struct rkimage_params {
	const char *init_file;
	uint32_t init_size;
	char *boot_file;
	uint32_t boot_size;

	const struct rkheader *hdr;
};

static int imagetool_get_filesize(const char *fname)
{
	struct stat sbuf;
	int fd;

	fd = open(fname, O_RDONLY);
	if (fd < 0)
		panic("Can't open %s", fname);

	if (fstat(fd, &sbuf) < 0)
		panic("Can't stat %s", fname);

	close(fd);

	return sbuf.st_size;
}

static int rkcommon_get_aligned_size(const char *fname)
{
	int size;

	size = imagetool_get_filesize(fname);
	if (size < 0)
		return -1;

	/*
	 * Pad to a 2KB alignment, as required for init/boot size by the ROM
	 * (see https://lists.denx.de/pipermail/u-boot/2017-May/293268.html)
	 */
	return ALIGN(size, RK_SIZE_ALIGN);
}

static int rkcommon_check_params(struct rkimage_params *params)
{
	int size;

	size = rkcommon_get_aligned_size(params->init_file);
	if (size < 0)
		return -1;
	params->init_size = size;

	/* Boot file is optional, and only for back-to-bootrom functionality. */
	if (params->boot_file) {
		size = rkcommon_get_aligned_size(params->boot_file);
		if (size < 0)
			return -1;
		params->boot_size = size;
	}

	if (params->init_size > params->hdr->size)
		fprintf(stderr, "Error: PBL image is too large (size %#x than %#x)\n",
			params->init_size, params->hdr->size);

	return 0;
}

static void rkcommon_set_header0(void *buf, const struct rkimage_params *params)
{
	struct rkheader0_info *hdr = buf;
	uint32_t init_boot_size;

	hdr->signature   = cpu_to_le32(RK_SIGNATURE);
	hdr->init_offset = cpu_to_le16(RK_INIT_OFFSET);
	hdr->init_size   = cpu_to_le16(params->init_size / RK_BLK_SIZE);

	if (!(params->hdr->flags & RK_SPL_RC4))
		hdr->disable_rc4 = cpu_to_le32(1);

	/*
	 * init_boot_size needs to be set, as it is read by the BootROM
	 * to determine the size of the next-stage bootloader (e.g. barebox),
	 * when used with the back-to-bootrom functionality.
	 *
	 * see https://lists.denx.de/pipermail/u-boot/2017-May/293267.html
	 * for a more detailed explanation by Andy Yan
	 */
	if (params->boot_file)
		init_boot_size = params->init_size + params->boot_size;
	else
		init_boot_size = params->init_size + RK_MAX_BOOT_SIZE;
	hdr->init_boot_size = cpu_to_le16(init_boot_size / RK_BLK_SIZE);

	rk_bootrom_rc4_encode(hdr, sizeof(*hdr));
}

/**
 * rkcommon_set_header() - set up the header for a Rockchip boot image
 *
 * This sets up a 2KB header which can be interpreted by the Rockchip boot ROM.
 *
 * @buf:	Pointer to header place (must be at least 2KB in size)
 */
static void rkcommon_set_header(void *buf, const struct rkimage_params *params)
{
	struct rkheader1_info *hdr = buf + RK_SPL_HDR_START;

	rkcommon_set_header0(buf, params);

	/* Set up the SPL name (i.e. copy spl_hdr over) */
	memcpy(&hdr->magic, params->hdr->magic, RK_SPL_HDR_SIZE);

	if (!(params->hdr->flags & RK_SPL_RC4))
		return;

	buf = rk_bootrom_rc4_encode(hdr, params->init_size);

	if (!params->boot_file)
		return;

	rk_bootrom_rc4_encode(buf, params->boot_size);
}

static void *read_file_padded(void *outbuf, const char *file, unsigned padded_size)
{
	int infd, ret;

	infd = open(file, O_RDONLY);
	if (infd < 0)
		panic("Can't open %s\n", file);

	ret = read_full(infd, outbuf, padded_size);
	close(infd);
	if (ret < 0)
		panic("Copy to %s failed\n", file);

	return outbuf + padded_size;
}

static void rockchip_copy_image(void *outbuf, const struct rkimage_params *params)
{
	outbuf = read_file_padded(outbuf, params->init_file, params->init_size);

	if (params->boot_file)
		read_file_padded(outbuf, params->boot_file, params->boot_size);
}

int handle_rk30(enum rksoc soc, const char *outfile, int n_code, char *argv[])
{
	struct rkimage_params params = {};
	unsigned file_size;
	char *outbuf;
	int outfd, ret;

	params.hdr = &rkheaders[soc];
	params.init_file = argv[0];
	params.boot_file = argv[1];

	ret = rkcommon_check_params(&params);
	if (ret)
		return ret;

	file_size = RK_SPL_HDR_START + params.init_size + params.boot_size;
	file_size = ALIGN(file_size, RK_SIZE_ALIGN);

	outfd = open(outfile, O_CREAT | O_RDWR | O_TRUNC, 0644);
	if (outfd < 0) {
		panic("Cannot open %s\n", outfile);
		return -1;
	}

	ret = ftruncate(outfd, file_size);
	if (ret)
		return ret;

	outbuf = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, outfd, 0);
	if (outbuf == MAP_FAILED)
		return -1;

	/*
	 * The SPL image looks as follows:
	 *
	 * 0x0    header0 (see rkcommon.c)
	 * 0x800  spl_name ('RK30', ..., 'RK33')
	 *        (start of the payload for AArch64 payloads: we expect the
	 *        first 4 bytes to be available for overwriting with our
	 *        spl_name)
	 * 0x804  first instruction to be executed
	 *        (start of the image/payload for 32bit payloads)
	 *
	 * For AArch64 (ARMv8) payloads, natural alignment (8-bytes) is
	 * required for its sections (so the image we receive needs to
	 * have the first 4 bytes reserved for the spl_name).
	 */

	rockchip_copy_image(outbuf + RK_SPL_HDR_START, &params);
	rkcommon_set_header(outbuf, &params);

	return 0;
}
