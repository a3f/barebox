/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __MACH_RKIMAGE_RK30_H_
#define __MACH_RKIMAGE_RK30_H_

#include <linux/types.h>
#ifdef __BAREBOX__
#include <crypto/rc4.h>
#endif

#define RK_BLK_SIZE		512
#define RK_SIZE_ALIGN		2048
#define RK_INIT_OFFSET		4
#define RK_MAX_BOOT_SIZE	(512 << 10)
#define RK_SPL_HDR_START	(RK_INIT_OFFSET * RK_BLK_SIZE)
#define RK_SPL_HDR_SIZE		4

#define RK_SIGNATURE		0x0ff0aa55
#define RK_SIGNATURE_RC4	0xfcdc8c3b

#define RK_BOOTROM_RC4_KEY	(unsigned char[16]) { \
	124, 78, 3, 4, 85, 5, 9, 7, 45, 44, 123, 56, 23, 13, 23, 17 }


/**
 * struct rkheader0_info - header block for boot ROM
 *
 * This is stored at SD card block 64 (where each block is 512 bytes, or at
 * the start of SPI flash. It is encoded with RC4.
 *
 * @signature:		Signature (must be RK_SIGNATURE)
 * @disable_rc4:	0 to use rc4 for boot image,  1 to use plain binary
 * @init_offset:	Offset in blocks of the SPL code from this header
 *			block. E.g. 4 means 2KB after the start of this header.
 * @init_size:		Size in blocks of the SPL code
 * @init_boot_size:	Size in blocks of the SPL code + the second stage loader
 *
 * Other fields are not used by barebox
 */
struct rkheader0_info {
	uint32_t signature;
	uint8_t reserved[4];
	uint32_t disable_rc4;
	uint16_t init_offset;
	uint8_t reserved1[492];
	uint16_t init_size;
	uint16_t init_boot_size;
	uint8_t reserved2[2];
};

/**
 * struct header1_info
 */
struct rkheader1_info {
	uint32_t magic;
};

static inline void *rk_bootrom_rc4_encode(void *buf, unsigned size)
{
	unsigned int remaining = size;

	while (remaining > 0) {
		int step = (remaining > RK_BLK_SIZE) ? RK_BLK_SIZE : remaining;

		rc4_encode(buf, step, RK_BOOTROM_RC4_KEY);
		buf += step;
		remaining -= step;
	}

	return buf;
}

#endif
