// SPDX-License-Identifier: GPL-2.0
/*
 * fat.c - FAT filesystem barebox driver
 *
 * Copyright (c) 2011 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
 */

#define pr_fmt(fmt) "fat-pbl: " fmt

#include <common.h>
#include "integer.h"
#include "ff.h"
#include "diskio.h"
#include "pbl.h"

/* ---------------------------------------------------------------*/

DRESULT disk_read(FATFS *fat, BYTE *buf, DWORD sector, BYTE count)
{
	struct pbl_bio *bio = fat->userdata;
	int ret;

	debug("%s: sector: %ld count: %d\n", __func__, sector, count);

	ret = bio->read(bio, sector, buf, count);
	if (ret != count)
		return ret;

	return 0;
}

DSTATUS disk_status(FATFS *fat)
{
	return 0;
}

DWORD get_fattime(void)
{
	return 0;
}

DRESULT disk_ioctl (FATFS *fat, BYTE command, void *buf)
{
	return 0;
}

WCHAR ff_convert(WCHAR src, UINT dir)
{
	if (src <= 0x80)
		return src;
	else
		return '?';
}

WCHAR ff_wtoupper(WCHAR chr)
{
	if (chr > 0x80)
		return '?';

	if ('a' <= chr && chr <= 'z')
		return  chr + 'A' - 'a';

	return chr;
}

static int fat_loadimage(FATFS *fs, const char *filename, void *dest, unsigned int len)
{
	FIL	file = {};
	UINT	nread;
	int	ret;

	ret = f_open(fs, &file, filename, FA_OPEN_EXISTING | FA_READ);
	if (ret) {
		pr_debug("f_open(%s) failed: %d\n", filename, ret);
		return ret;
	}

	ret = f_read(&file, dest, len, &nread);
	if (ret) {
		pr_debug("f_read failed: %d\n", ret);
		return ret;
	}

	if (f_size(&file) > len)
		return -ENOSPC;

	return 0;
}

int pbl_fat_load(struct pbl_bio *bio, const char *filename, void *dest, unsigned int len)
{
	FATFS	fs = {};
	int	ret;

	fs.userdata = bio;

	/* mount fs */
	ret = f_mount(&fs);
	if (ret) {
		pr_debug("f_mount(%s) failed: %d\n", filename, ret);
		return ret;
	}

	pr_debug("Reading file %s to 0x%p\n", filename, dest);

	return fat_loadimage(&fs, filename, dest, len);
}

#define BS_55AA                     510     /* Boot sector signature (2) */
#define BS_FilSysType               54      /* File system type (1) */
#define BS_FilSysType32             82      /* File system type (1) */
#define MBR_Table           446     /* MBR: Partition table offset (2) */
#define MBR_StartSector             8

enum filetype is_fat_or_mbr(const unsigned char *sector, unsigned long *bootsec)
{
	/*
	 * bootsec can be used to return index of the first sector in the
	 * first partition
	 */
	if (bootsec)
		*bootsec = 0;

	/*
	 * Check record signature (always placed at offset 510 even if the
	 * sector size is > 512)
	 */
	if (get_unaligned_le16(&sector[BS_55AA]) != 0xAA55)
		return filetype_unknown;

	/* Check "FAT" string */
	if ((get_unaligned_le32(&sector[BS_FilSysType]) & 0xFFFFFF) == 0x544146)
		return filetype_fat;

	if ((get_unaligned_le32(&sector[BS_FilSysType32]) & 0xFFFFFF) == 0x544146)
		return filetype_fat;

	if (bootsec)
		/*
		 * This must be an MBR, so return the starting sector of the
		 * first partition so we could check if there is a FAT boot
		 * sector there
		 */
		*bootsec = get_unaligned_le16(&sector[MBR_Table + MBR_StartSector]);

	return filetype_mbr;
}

