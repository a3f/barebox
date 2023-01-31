// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google, LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <audio_codec.h>
#include <clk.h>
#include <dm.h>
#include <i2s.h>
#include <log.h>
#include <misc.h>
#include <sound.h>
#include <asm/arch-rockchip/periph.h>
#include <dm/pinctrl.h>

static int rockchip_sound_setup(struct udevice *dev)
{
	struct sound_uc_priv *uc_priv = dev_get_uclass_priv(dev);
	struct i2s_uc_priv *i2c_priv = dev_get_uclass_priv(uc_priv->i2s);
	int ret;

	if (uc_priv->setup_done)
		return -EALREADY;
	ret = audio_codec_set_params(uc_priv->codec, i2c_priv->id,
				     i2c_priv->samplingrate,
				     i2c_priv->samplingrate * i2c_priv->rfs,
				     i2c_priv->bitspersample,
				     i2c_priv->channels);
	if (ret)
		return ret;
	uc_priv->setup_done = true;

	return 0;
}

static int rockchip_sound_play(struct udevice *dev, void *data, uint data_size)
{
	struct sound_uc_priv *uc_priv = dev_get_uclass_priv(dev);

	return i2s_tx_data(uc_priv->i2s, data, data_size);
}

static const struct sound_ops rockchip_sound_ops = {
	.setup	= rockchip_sound_setup,
	.play	= rockchip_sound_play,
};

struct device *of_get_sound_dai(struct device_node *parent, const char *name)
{
	struct device_node *node;
	struct device *dev;

	node = of_get_child_by_name(parent, name);
	if (!node) {
		ret = dev_err_probe(dev, -EINVAL,
				    "Failed to find /%s subnode\n", name);
		return ERR_PTR(ret);
	}

	ret = of_parse_phandle_with_args(node, "sound-dai",
					 "#sound-dai-cells", 0, &args);
	if (ret) {
		dev_err_probe(dev, ret, "Cannot find i2s phandle\n");
		return ERR_PTR(ret);
	}

	dev = of_find_device_by_node(args.np);
	if (!dev) {
		ret = dev_err_probe(dev, -EINVAL, "unable to find device of node %s\n",
				    args.np->full_name);
		return ERR_PTR(ret);
	}

	return dev;

static int rockchip_sound_probe(struct udevice *dev)
{
	struct sound_uc_priv *uc_priv = dev_get_uclass_priv(dev);
	struct ofnode_phandle_args args;
	struct udevice *pinctrl;
	struct clk *clk;
	struct device_node *node;
	struct device *dev;
	int ret;

	dev = of_get_sound_dai(dev->device_node, "cpu");
	if (IS_ERR(dev))
		return PTR_ERR(dev);

	ret = uclass_get_device_by_ofnode(UCLASS_I2S, args.node, &uc_priv->i2s);
	if (ret)
		return dev_err_probe(dev, ret, "Cannot find i2s\n");

	dev = of_get_sound_dai(dev->device_node, "codec");
	if (IS_ERR(dev))
		return PTR_ERR(dev);

	ret = uclass_get_device_by_ofnode(UCLASS_AUDIO_CODEC, args.node,
					  &uc_priv->codec);
	if (ret)
		return dev_err_probe(dev, ret, "Cannot find audio codec\n");

	clk = clk_get(uc_priv->i2s, 1);
	if (IS_ERR(clk))
		return dev_err_probe(dev, PTR_ERR(clk), "Cannot find clock\n");

	ret = clk_set_rate(clk, 12288000);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Cannot find clock\n");

	dev_dbg(dev, "Probed sound with codec '%s' and i2s '%s'\n",
		uc_priv->codec->name, uc_priv->i2s->name);

	return 0;
}

static const struct udevice_id rockchip_sound_ids[] = {
	{ .compatible = "rockchip,audio-rk817" },
	{ /* sentinel */ }
};

static struct driver rockchip_sound_driver = {
	.name = "rockchip_sound",
	.probe = rockchip_sound_probe,
	.remove = rockchip_sound_remove,
};
device_platform_driver(rockchip_sound_driver);
