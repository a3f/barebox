// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  PCM Interface - misc routines
 *  Copyright (c) 1998 by Jaroslav Kysela <perex@perex.cz>
 */

#include <linux/types.h>
#include <sound/asound.h>
#include <sound/pcm.h>
#include <errno.h>
#include <module.h>

#define SND_PCM_FORMAT_UNKNOWN (-1)

/* NOTE: "signed" prefix must be given below since the default char is
 *       unsigned on some architectures!
 */
struct pcm_format_data {
	unsigned char width;	/* bit width */
	unsigned char phys;	/* physical bit width */
	signed char le;	/* 0 = big-endian, 1 = little-endian, -1 = others */
	signed char signd;	/* 0 = unsigned, 1 = signed, -1 = others */
	unsigned char silence[8];	/* silence data to fill */
};

/* we do lots of calculations on snd_pcm_format_t; shut up sparse */
#define INT	__force int

static bool valid_format(snd_pcm_format_t format)
{
	return (INT)format >= 0 && (INT)format <= (INT)SNDRV_PCM_FORMAT_LAST;
}

static const struct pcm_format_data pcm_formats[(INT)SNDRV_PCM_FORMAT_LAST+1] = {
	[SNDRV_PCM_FORMAT_S8] = {
		.width = 8, .phys = 8, .le = -1, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_U8] = {
		.width = 8, .phys = 8, .le = -1, .signd = 0,
		.silence = { 0x80 },
	},
	[SNDRV_PCM_FORMAT_S16_LE] = {
		.width = 16, .phys = 16, .le = 1, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_S16_BE] = {
		.width = 16, .phys = 16, .le = 0, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_U16_LE] = {
		.width = 16, .phys = 16, .le = 1, .signd = 0,
		.silence = { 0x00, 0x80 },
	},
	[SNDRV_PCM_FORMAT_U16_BE] = {
		.width = 16, .phys = 16, .le = 0, .signd = 0,
		.silence = { 0x80, 0x00 },
	},
	[SNDRV_PCM_FORMAT_S24_LE] = {
		.width = 24, .phys = 32, .le = 1, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_S24_BE] = {
		.width = 24, .phys = 32, .le = 0, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_U24_LE] = {
		.width = 24, .phys = 32, .le = 1, .signd = 0,
		.silence = { 0x00, 0x00, 0x80 },
	},
	[SNDRV_PCM_FORMAT_U24_BE] = {
		.width = 24, .phys = 32, .le = 0, .signd = 0,
		.silence = { 0x00, 0x80, 0x00, 0x00 },
	},
	[SNDRV_PCM_FORMAT_S32_LE] = {
		.width = 32, .phys = 32, .le = 1, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_S32_BE] = {
		.width = 32, .phys = 32, .le = 0, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_U32_LE] = {
		.width = 32, .phys = 32, .le = 1, .signd = 0,
		.silence = { 0x00, 0x00, 0x00, 0x80 },
	},
	[SNDRV_PCM_FORMAT_U32_BE] = {
		.width = 32, .phys = 32, .le = 0, .signd = 0,
		.silence = { 0x80, 0x00, 0x00, 0x00 },
	},
	[SNDRV_PCM_FORMAT_FLOAT_LE] = {
		.width = 32, .phys = 32, .le = 1, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_FLOAT_BE] = {
		.width = 32, .phys = 32, .le = 0, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_FLOAT64_LE] = {
		.width = 64, .phys = 64, .le = 1, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_FLOAT64_BE] = {
		.width = 64, .phys = 64, .le = 0, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE] = {
		.width = 32, .phys = 32, .le = 1, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE] = {
		.width = 32, .phys = 32, .le = 0, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_MU_LAW] = {
		.width = 8, .phys = 8, .le = -1, .signd = -1,
		.silence = { 0x7f },
	},
	[SNDRV_PCM_FORMAT_A_LAW] = {
		.width = 8, .phys = 8, .le = -1, .signd = -1,
		.silence = { 0x55 },
	},
	[SNDRV_PCM_FORMAT_IMA_ADPCM] = {
		.width = 4, .phys = 4, .le = -1, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_G723_24] = {
		.width = 3, .phys = 3, .le = -1, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_G723_40] = {
		.width = 5, .phys = 5, .le = -1, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_DSD_U8] = {
		.width = 8, .phys = 8, .le = 1, .signd = 0,
		.silence = { 0x69 },
	},
	[SNDRV_PCM_FORMAT_DSD_U16_LE] = {
		.width = 16, .phys = 16, .le = 1, .signd = 0,
		.silence = { 0x69, 0x69 },
	},
	[SNDRV_PCM_FORMAT_DSD_U32_LE] = {
		.width = 32, .phys = 32, .le = 1, .signd = 0,
		.silence = { 0x69, 0x69, 0x69, 0x69 },
	},
	[SNDRV_PCM_FORMAT_DSD_U16_BE] = {
		.width = 16, .phys = 16, .le = 0, .signd = 0,
		.silence = { 0x69, 0x69 },
	},
	[SNDRV_PCM_FORMAT_DSD_U32_BE] = {
		.width = 32, .phys = 32, .le = 0, .signd = 0,
		.silence = { 0x69, 0x69, 0x69, 0x69 },
	},
	/* FIXME: the following two formats are not defined properly yet */
	[SNDRV_PCM_FORMAT_MPEG] = {
		.le = -1, .signd = -1,
	},
	[SNDRV_PCM_FORMAT_GSM] = {
		.le = -1, .signd = -1,
	},
	[SNDRV_PCM_FORMAT_S20_LE] = {
		.width = 20, .phys = 32, .le = 1, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_S20_BE] = {
		.width = 20, .phys = 32, .le = 0, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_U20_LE] = {
		.width = 20, .phys = 32, .le = 1, .signd = 0,
		.silence = { 0x00, 0x00, 0x08, 0x00 },
	},
	[SNDRV_PCM_FORMAT_U20_BE] = {
		.width = 20, .phys = 32, .le = 0, .signd = 0,
		.silence = { 0x00, 0x08, 0x00, 0x00 },
	},
	/* FIXME: the following format is not defined properly yet */
	[SNDRV_PCM_FORMAT_SPECIAL] = {
		.le = -1, .signd = -1,
	},
	[SNDRV_PCM_FORMAT_S24_3LE] = {
		.width = 24, .phys = 24, .le = 1, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_S24_3BE] = {
		.width = 24, .phys = 24, .le = 0, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_U24_3LE] = {
		.width = 24, .phys = 24, .le = 1, .signd = 0,
		.silence = { 0x00, 0x00, 0x80 },
	},
	[SNDRV_PCM_FORMAT_U24_3BE] = {
		.width = 24, .phys = 24, .le = 0, .signd = 0,
		.silence = { 0x80, 0x00, 0x00 },
	},
	[SNDRV_PCM_FORMAT_S20_3LE] = {
		.width = 20, .phys = 24, .le = 1, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_S20_3BE] = {
		.width = 20, .phys = 24, .le = 0, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_U20_3LE] = {
		.width = 20, .phys = 24, .le = 1, .signd = 0,
		.silence = { 0x00, 0x00, 0x08 },
	},
	[SNDRV_PCM_FORMAT_U20_3BE] = {
		.width = 20, .phys = 24, .le = 0, .signd = 0,
		.silence = { 0x08, 0x00, 0x00 },
	},
	[SNDRV_PCM_FORMAT_S18_3LE] = {
		.width = 18, .phys = 24, .le = 1, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_S18_3BE] = {
		.width = 18, .phys = 24, .le = 0, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_U18_3LE] = {
		.width = 18, .phys = 24, .le = 1, .signd = 0,
		.silence = { 0x00, 0x00, 0x02 },
	},
	[SNDRV_PCM_FORMAT_U18_3BE] = {
		.width = 18, .phys = 24, .le = 0, .signd = 0,
		.silence = { 0x02, 0x00, 0x00 },
	},
	[SNDRV_PCM_FORMAT_G723_24_1B] = {
		.width = 3, .phys = 8, .le = -1, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_G723_40_1B] = {
		.width = 5, .phys = 8, .le = -1, .signd = -1,
		.silence = {},
	},
};

/**
 * snd_pcm_format_width - return the bit-width of the format
 * @format: the format to check
 *
 * Return: The bit-width of the format, or a negative error code
 * if unknown format.
 */
int snd_pcm_format_width(snd_pcm_format_t format)
{
	int val;
	if (!valid_format(format))
		return -EINVAL;
	val = pcm_formats[(INT)format].width;
	if (!val)
		return -EINVAL;
	return val;
}
EXPORT_SYMBOL(snd_pcm_format_width);
