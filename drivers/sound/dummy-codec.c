// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt) "dummy-codec: " fmt

#include <linux/clk.h>
#include <linux/printk.h>
#include <driver.h>
#include <init.h>
#include <sound/soc.h>
#include <of.h>

#include "dummy_codec.h"

#define DUMMY_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
		       SNDRV_PCM_FMTBIT_S20_3LE |\
		       SNDRV_PCM_FMTBIT_S24_LE |\
		       SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_component_driver soc_codec_dev_dummy;
static const struct snd_soc_dai_ops dummy_dai_ops;

static struct snd_soc_dai_driver dummy_dai[] = {
	{
		.name = "dummy-hifi",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 2,
			.channels_max = 8,
			.rates = DUMMY_PLAYBACK_RATES,
			.formats = DUMMY_FORMATS,
		},
		.ops = &dummy_dai_ops,
	},
};

static int dummy_platform_probe(struct device *dev)
{
	struct dummy_codec_priv *dummy_codec_data;
	struct clk *mclk;
	int ret;

	dummy_codec_data = xzalloc(sizeof(*dummy_codec_data));

	dev->priv = dummy_codec_data;

	ret = snd_soc_register_component(dev, &soc_codec_dev_dummy,
					 dummy_dai, ARRAY_SIZE(dummy_dai));
	if (ret < 0)
		return dev_err_probe(dev, ret, "register codec error\n");

	return 0;
}

static struct driver dummy_codec_driver = {
	.name = "dummy-codec",
	.probe = dummy_platform_probe,
};
device_platform_driver(dummy_codec_driver);
