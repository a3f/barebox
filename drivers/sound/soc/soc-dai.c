// SPDX-License-Identifier: GPL-2.0
//
// soc-dai.c
//
// Copyright (C) 2019 Renesas Electronics Corp.
// Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
//

#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <linux/printk.h>
#include <module.h>

#define soc_dai_ret(dai, ret) _soc_dai_ret(dai, __func__, ret)
static inline int _soc_dai_ret(struct snd_soc_dai *dai,
			       const char *func, int ret)
{
	/* Positive, Zero values are not errors */
	if (ret >= 0)
		return ret;

	/* Negative values might be errors */
	switch (ret) {
	case -EPROBE_DEFER:
	case -ENOTSUPP:
		break;
	default:
		dev_err(dai->dev,
			"ASoC: error at %s on %s: %d\n",
			func, dai->name, ret);
	}

	return ret;
}

/**
 * snd_soc_dai_play - play PCM audio
 * @dai: DAI
 * @data: PCM data to play
 * @nsamples: number of samples
 *
 * Playback a PCM audio buffer via DAI
 */
int snd_soc_dai_play(struct snd_soc_dai *dai,
		     const void *data, unsigned nsamples)
{
	int ret = -ENOTSUPP;

	if (dai->driver->ops &&
	    dai->driver->ops->play)
		ret = dai->driver->ops->play(dai, data, nsamples);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_play);

/**
 * snd_soc_dai_set_fmt - configure DAI hardware audio format.
 * @dai: DAI
 * @fmt: SND_SOC_DAIFMT_* format value.
 *
 * Configures the DAI hardware format and clocking.
 */
int snd_soc_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	int ret = -ENOTSUPP;

	if (dai->driver->ops && dai->driver->ops->set_fmt)
		ret = dai->driver->ops->set_fmt(dai, fmt);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_set_fmt);

/**
 * snd_soc_dai_digital_mute - configure DAI system or master clock.
 * @dai: DAI
 * @mute: mute enable
 * @direction: stream to mute
 *
 * Mutes the DAI DAC.
 */
int snd_soc_dai_digital_mute(struct snd_soc_dai *dai, int mute)
{
	int ret = -ENOTSUPP;

	if (dai->driver->ops &&
	    dai->driver->ops->mute_stream)
		ret = dai->driver->ops->mute_stream(dai, mute);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_digital_mute);

/**
 * snd_soc_dai_set_sysclk - configure DAI system or master clock.
 * @dai: DAI
 * @clk_id: DAI specific clock ID
 * @freq: new clock frequency in Hz
 * @dir: new clock direction - input/output.
 *
 * Configures the DAI master (MCLK) or system (SYSCLK) clocking.
 */
int snd_soc_dai_set_sysclk(struct snd_soc_dai *dai, int clk_id,
			   unsigned int freq, int dir)
{
	int ret = -ENOTSUPP;

	if (dai->driver->ops &&
	    dai->driver->ops->set_sysclk)
		ret = dai->driver->ops->set_sysclk(dai, clk_id, freq, dir);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_set_sysclk);

static struct snd_soc_dai *snd_soc_dai_of_xlate(struct of_phandle_args *daispec)
{
	struct snd_soc_component *component;
	int index = 0;

	/* We don't yet need to support custom of_xlate callbacks */
	if (WARN_ON(daispec->args_count > 1))
		return NULL;
	else if (daispec->args_count == 1)
		index = daispec->args[0];

	component = of_snd_soc_component_get(daispec->np);
	if (!component)
		return NULL;

	return &component->dais[0];
}

/**
 * of_snd_soc_dai_get_by_phandle - lookup DAI using OF node
 * @np: device node to lookup DAI for
 *
 * Returns found OF node or NULL if none found
 */
struct snd_soc_dai *of_snd_soc_dai_get_by_phandle(struct device_node *np,
						  const char *property)
{
	struct of_phandle_args daispec;
	int ret;

	ret = of_parse_phandle_with_args(np, property, "#sound-dai-cells",
					 0, &daispec);
	if (ret < 0)
		return NULL;

	return snd_soc_dai_of_xlate(&daispec);
}
EXPORT_SYMBOL_GPL(of_snd_soc_dai_get_by_phandle);
