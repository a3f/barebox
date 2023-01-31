// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google, LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define pr_fmt(fmt) "simple-audio-card: " fmt

#include <common.h>
#include <sound.h>
#include <sound/soc.h>
#include <of.h>
#include <sound/simple_card_utils.h>

struct dai_link {
	struct sound_card card;
	struct device_node *of_start;
	unsigned int dai_fmt;
	struct snd_soc_dai *cpu_dai;
	struct snd_soc_dai *codec_dai;
};

struct simple_audio_card {
	int nlinks;
	struct dai_link links[];
};

static void dai_link_stop(struct dai_link *link)
{
	snd_soc_dai_digital_mute(link->codec_dai, true);
	snd_soc_dai_digital_mute(link->cpu_dai, true);

	snd_soc_dai_play(link->cpu_dai, NULL, 0);
}

static int dai_link_play(struct sound_card *card, const void *data, unsigned nsamples)
{
	struct dai_link *link = container_of(card, struct dai_link, card);

	return snd_soc_dai_play(link->cpu_dai, data, nsamples);
}

static int set_dai_link(struct device *dev, struct device_node **outnp,
			struct device_node *dai, const char *prefix)
{
	if (!strstarts(dai->name, prefix))
		return 0;

	if (*outnp) {
		dev_err(dev, "only one codec and one CPU DAI supported\n");
		return -ENOSYS;
	}

	*outnp = dai;
	return 0;
}

#define DAI_PROP(prop, subnode) (subnode ? prop : "simple-audio-card," prop)

static int parse_dai_link(struct device *dev, struct dai_link *link,
			  struct device_node *linknp, bool sub)
{
	struct device_node *dai, *codec = NULL, *cpu = NULL;
	unsigned fmt = 0;
	int ret;

	for_each_available_child_of_node(linknp, dai) {
		ret = set_dai_link(dev, &cpu, dai, DAI_PROP("cpu", sub));
		if (ret)
			return ret;

		ret = set_dai_link(dev, &codec, dai, DAI_PROP("codec", sub));
		if (ret)
			return ret;
	}

	if (!link->cpu_dai || !link->codec_dai)
		return -ENOENT;

	asoc_simple_parse_daifmt(dev, linknp, codec, DAI_PROP("", sub), &fmt);
	if (!fmt)
		return dev_err_probe(dev, -ENOSYS, "failed to read DAI fmt\n");

	link->dai_fmt = fmt;
	link->card.play = dai_link_play;

	link->cpu_dai = of_snd_soc_dai_get_by_phandle(cpu, "sound-dai");
	link->codec_dai = of_snd_soc_dai_get_by_phandle(codec, "sound-dai");

	if (!codec || !cpu)
		return -EILSEQ;

	return sound_card_register(&link->card);
}

static int simple_audio_card_probe(struct device *dev)
{
	struct simple_audio_card *priv;
	struct device_node *child, *np = dev_of_node(dev);
	const char *label;
	int i = 0, nlinks = 0, ret;
	bool subnodes = true;
	struct dai_link *link;

	for_each_available_child_of_node(np, child) {
		if (strstarts(child->name, "simple-audio-card,dai-link"))
			nlinks++;
	}

	if (!nlinks) {
		nlinks = 1;
		subnodes = false;
	}

	priv = kzalloc(struct_size(priv, links, nlinks), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	label = np->full_name;
	of_property_read_string(np, "simple-audio-card,name", &label);

	priv->nlinks = nlinks;
	dev->priv = priv;

	link = priv->links;

	if (!subnodes)
		return parse_dai_link(dev, link, np, false);

	for_each_available_child_of_node(np, child) {
		link->card.name = xasprintf("%s%d", label, i++);

		ret = parse_dai_link(dev, link, child, true);
		if (ret)
			return ret;

		link++;
	}

	return 0;
}

static void simple_audio_card_remove(struct device *dev)
{
	struct simple_audio_card *priv = dev->priv;
	int i;

	for (i = 0; i < priv->nlinks; i++)
		dai_link_stop(&priv->links[i]);
}

static const struct of_device_id simple_audio_card_ids[] = {
	{ .compatible = "simple-audio-card" },
	{ /* sentinel */ }
};

static struct driver simple_audio_card_driver = {
	.name = "simple_audio_card",
	.probe = simple_audio_card_probe,
	.remove = simple_audio_card_remove,
	.of_compatible = simple_audio_card_ids,
};
device_platform_driver(simple_audio_card_driver);
