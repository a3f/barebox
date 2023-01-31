/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 *  Advanced Linux Sound Architecture - ALSA - Driver
 *  Copyright (c) 1994-2003 by Jaroslav Kysela <perex@perex.cz>,
 *                             Abramo Bagnara <abramo@alsa-project.org>
 */

#ifndef _UAPI__SOUND_ASOUND_H
#define _UAPI__SOUND_ASOUND_H

#include <linux/types.h>
#include <asm/byteorder.h>

typedef int __bitwise snd_pcm_format_t;
#define	SNDRV_PCM_FORMAT_S8	((__force snd_pcm_format_t) 0)
#define	SNDRV_PCM_FORMAT_U8	((__force snd_pcm_format_t) 1)
#define	SNDRV_PCM_FORMAT_S16_LE	((__force snd_pcm_format_t) 2)
#define	SNDRV_PCM_FORMAT_S16_BE	((__force snd_pcm_format_t) 3)
#define	SNDRV_PCM_FORMAT_U16_LE	((__force snd_pcm_format_t) 4)
#define	SNDRV_PCM_FORMAT_U16_BE	((__force snd_pcm_format_t) 5)
#define	SNDRV_PCM_FORMAT_S24_LE	((__force snd_pcm_format_t) 6) /* low three bytes */
#define	SNDRV_PCM_FORMAT_S24_BE	((__force snd_pcm_format_t) 7) /* low three bytes */
#define	SNDRV_PCM_FORMAT_U24_LE	((__force snd_pcm_format_t) 8) /* low three bytes */
#define	SNDRV_PCM_FORMAT_U24_BE	((__force snd_pcm_format_t) 9) /* low three bytes */
/*
 * For S32/U32 formats, 'msbits' hardware parameter is often used to deliver information about the
 * available bit count in most significant bit. It's for the case of so-called 'left-justified' or
 * `right-padding` sample which has less width than 32 bit.
 */
#define	SNDRV_PCM_FORMAT_S32_LE	((__force snd_pcm_format_t) 10)
#define	SNDRV_PCM_FORMAT_S32_BE	((__force snd_pcm_format_t) 11)
#define	SNDRV_PCM_FORMAT_U32_LE	((__force snd_pcm_format_t) 12)
#define	SNDRV_PCM_FORMAT_U32_BE	((__force snd_pcm_format_t) 13)
#define	SNDRV_PCM_FORMAT_FLOAT_LE	((__force snd_pcm_format_t) 14) /* 4-byte float, IEEE-754 32-bit, range -1.0 to 1.0 */
#define	SNDRV_PCM_FORMAT_FLOAT_BE	((__force snd_pcm_format_t) 15) /* 4-byte float, IEEE-754 32-bit, range -1.0 to 1.0 */
#define	SNDRV_PCM_FORMAT_FLOAT64_LE	((__force snd_pcm_format_t) 16) /* 8-byte float, IEEE-754 64-bit, range -1.0 to 1.0 */
#define	SNDRV_PCM_FORMAT_FLOAT64_BE	((__force snd_pcm_format_t) 17) /* 8-byte float, IEEE-754 64-bit, range -1.0 to 1.0 */
#define	SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE ((__force snd_pcm_format_t) 18) /* IEC-958 subframe, Little Endian */
#define	SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE ((__force snd_pcm_format_t) 19) /* IEC-958 subframe, Big Endian */
#define	SNDRV_PCM_FORMAT_MU_LAW		((__force snd_pcm_format_t) 20)
#define	SNDRV_PCM_FORMAT_A_LAW		((__force snd_pcm_format_t) 21)
#define	SNDRV_PCM_FORMAT_IMA_ADPCM	((__force snd_pcm_format_t) 22)
#define	SNDRV_PCM_FORMAT_MPEG		((__force snd_pcm_format_t) 23)
#define	SNDRV_PCM_FORMAT_GSM		((__force snd_pcm_format_t) 24)
#define	SNDRV_PCM_FORMAT_S20_LE	((__force snd_pcm_format_t) 25) /* in four bytes, LSB justified */
#define	SNDRV_PCM_FORMAT_S20_BE	((__force snd_pcm_format_t) 26) /* in four bytes, LSB justified */
#define	SNDRV_PCM_FORMAT_U20_LE	((__force snd_pcm_format_t) 27) /* in four bytes, LSB justified */
#define	SNDRV_PCM_FORMAT_U20_BE	((__force snd_pcm_format_t) 28) /* in four bytes, LSB justified */
/* gap in the numbering for a future standard linear format */
#define	SNDRV_PCM_FORMAT_SPECIAL	((__force snd_pcm_format_t) 31)
#define	SNDRV_PCM_FORMAT_S24_3LE	((__force snd_pcm_format_t) 32)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_S24_3BE	((__force snd_pcm_format_t) 33)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_U24_3LE	((__force snd_pcm_format_t) 34)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_U24_3BE	((__force snd_pcm_format_t) 35)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_S20_3LE	((__force snd_pcm_format_t) 36)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_S20_3BE	((__force snd_pcm_format_t) 37)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_U20_3LE	((__force snd_pcm_format_t) 38)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_U20_3BE	((__force snd_pcm_format_t) 39)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_S18_3LE	((__force snd_pcm_format_t) 40)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_S18_3BE	((__force snd_pcm_format_t) 41)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_U18_3LE	((__force snd_pcm_format_t) 42)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_U18_3BE	((__force snd_pcm_format_t) 43)	/* in three bytes */
#define	SNDRV_PCM_FORMAT_G723_24	((__force snd_pcm_format_t) 44) /* 8 samples in 3 bytes */
#define	SNDRV_PCM_FORMAT_G723_24_1B	((__force snd_pcm_format_t) 45) /* 1 sample in 1 byte */
#define	SNDRV_PCM_FORMAT_G723_40	((__force snd_pcm_format_t) 46) /* 8 Samples in 5 bytes */
#define	SNDRV_PCM_FORMAT_G723_40_1B	((__force snd_pcm_format_t) 47) /* 1 sample in 1 byte */
#define	SNDRV_PCM_FORMAT_DSD_U8		((__force snd_pcm_format_t) 48) /* DSD, 1-byte samples DSD (x8) */
#define	SNDRV_PCM_FORMAT_DSD_U16_LE	((__force snd_pcm_format_t) 49) /* DSD, 2-byte samples DSD (x16), little endian */
#define	SNDRV_PCM_FORMAT_DSD_U32_LE	((__force snd_pcm_format_t) 50) /* DSD, 4-byte samples DSD (x32), little endian */
#define	SNDRV_PCM_FORMAT_DSD_U16_BE	((__force snd_pcm_format_t) 51) /* DSD, 2-byte samples DSD (x16), big endian */
#define	SNDRV_PCM_FORMAT_DSD_U32_BE	((__force snd_pcm_format_t) 52) /* DSD, 4-byte samples DSD (x32), big endian */
#define	SNDRV_PCM_FORMAT_LAST		SNDRV_PCM_FORMAT_DSD_U32_BE
#define	SNDRV_PCM_FORMAT_FIRST		SNDRV_PCM_FORMAT_S8

struct snd_pcm_hw_params {
	unsigned int channels;
	unsigned int rate_min;
	snd_pcm_format_t format;
};

#endif /* _UAPI__SOUND_ASOUND_H */
