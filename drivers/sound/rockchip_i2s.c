// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google LLC
 * Copyright 2014 Rockchip Electronics Co., Ltd.
 * Taken from dc i2s/rockchip.c
 */

#include <common.h>
#include <driver.h>
#include <init.h>
#include <sound.h>
#include <pinctrl.h>
#include <regmap.h>
#include <linux/clk.h>
#include <sound/soc.h>
#include <io.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>

#define I2S_TXCR	0x00
#define		I2S_TXCR_VDW_MASK	GENMASK(8, 0)
#define		I2S_TXCR_VDW(x)		FIELD_PREP(I2S_TXCR_VDW_MASK, (x) - 1)
#define I2S_RXCR	0x04
#define		I2S_CSR_MASK		GENMASK(16, 15)
#define		I2S_CSR(chn)		FIELD_PREP(I2S_CSR_MASK, (chn) / 2 - 1)
#define I2S_CKR		0x08
#define		I2S_CKR_MDIV_MASK	GENMASK(23, 16)
#define		I2S_CKR_MDIV(x)		FIELD_PREP(I2S_CKR_MDIV_MASK, (x) - 1)
#define		I2S_CKR_RSD_MASK	GENMASK(15, 8)
#define		I2S_CKR_RSD(x)		FIELD_PREP(I2S_CKR_RSD_MASK, (x) - 1)
#define I2S_FIFOLR	0x0c
#define		I2S_FIFOLR_TFL0_MASK	GENMASK(5, 0)
#define I2S_DMACR	0x10
#define I2S_INTCR	0x14
#define I2S_INTSR	0x18
#define I2S_XFER	0x1c
#define		I2S_XFER_TXS_START	BIT(0)
#define		I2S_XFER_RXS_START	BIT(1)
#define I2S_CLR		0x20
#define I2S_TXDR	0x24
#define I2S_RXDR	0x28

struct rk_i2s_dev {
	struct regmap *regmap;
	struct clk *hclk;
	struct clk *mclk;
};

static inline struct rk_i2s_dev *to_info(struct snd_soc_dai *dai)
{
	return snd_soc_dai_get_drvdata(dai);
}

static int rockchip_i2s_hw_params(struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *cpu_dai)
{
	struct rk_i2s_dev *i2s = to_info(cpu_dai);
	u32 bps, chn;
	u32 lrf = 256;

	bps = params_width(params);

	chn = params_channels(params);
	if (chn != 2 && chn != 4 && chn != 6 && chn != 8) {
		dev_err(cpu_dai->dev, "invalid channel: %d\n", chn);
		return -EINVAL;
	}

	regmap_update_bits(i2s->regmap, I2S_TXCR,
			   I2S_TXCR_VDW_MASK | I2S_CSR_MASK,
			   I2S_TXCR_VDW(bps) | I2S_CSR(chn));

	regmap_update_bits(i2s->regmap, I2S_CKR,
			   I2S_CKR_MDIV_MASK | I2S_CKR_RSD_MASK,
			   I2S_CKR_MDIV(lrf / (bps * chn)) | I2S_CKR_RSD(bps * chn));

	return 0;
}

static int rockchip_i2s_set_fmt(struct snd_soc_dai *cpu_dai,
				unsigned int fmt)
{
	if ((fmt & SND_SOC_DAIFMT_CLOCK_PROVIDER_MASK) != SND_SOC_DAIFMT_BP_FP)
		return -ENOSYS;

	return 0;
}

static void rockchip_snd_stop(struct rk_i2s_dev *i2s)
{
	regmap_clear_bits(i2s->regmap, I2S_XFER, I2S_XFER_TXS_START);
}

static int rockchip_i2s_play(struct snd_soc_dai *cpu_dai,
			     const void *_data, unsigned nsamples)
{
	const u32 *data = _data;
	struct rk_i2s_dev *i2s = to_info(cpu_dai);
	u32 val;
	int ret, i;

	if (!data) {
		pinctrl_select_state(cpu_dai->dev, "bclk_off");
		rockchip_snd_stop(i2s);
		return 0;
	}

	pinctrl_select_state(cpu_dai->dev, "bclk_on");

	for (i = 0; i < min(32u, nsamples); i++)
		regmap_write(i2s->regmap, I2S_TXDR, *data++);

	nsamples -= min(32u, nsamples);

	regmap_set_bits(i2s->regmap, I2S_XFER,
			I2S_XFER_TXS_START | I2S_XFER_RXS_START);

	while (nsamples) {
		regmap_read(i2s->regmap, I2S_FIFOLR, &val);
		if (FIELD_GET(I2S_FIFOLR_TFL0_MASK, val) < 0x20) {
			regmap_write(i2s->regmap, I2S_TXDR, *data++);
			nsamples--;
		}
	}

	ret = regmap_read_poll_timeout(i2s->regmap, I2S_FIFOLR, val,
				       FIELD_GET(I2S_FIFOLR_TFL0_MASK, val) == 0,
				       USEC_PER_SEC);
	if (ret)
		return ret;

	regmap_clear_bits(i2s->regmap, I2S_XFER,
			  I2S_XFER_TXS_START | I2S_XFER_RXS_START);
	regmap_write(i2s->regmap, I2S_CLR, 0);

	return 0;
}

static int rockchip_i2s_set_sysclk(struct snd_soc_dai *cpu_dai, int clk_id,
				   unsigned int freq, int dir)
{
	struct rk_i2s_dev *i2s = to_info(cpu_dai);
	int ret;

	if (freq == 0)
		return 0;

	ret = clk_set_rate(i2s->mclk, freq);
	if (ret)
		dev_err(cpu_dai->dev, "Fail to set mclk %pe\n", ERR_PTR(ret));

	return ret;
}

static const struct snd_soc_dai_ops rockchip_i2s_dai_ops = {
	.hw_params = rockchip_i2s_hw_params,
	.set_sysclk = rockchip_i2s_set_sysclk,
	.set_fmt = rockchip_i2s_set_fmt,
	.play = rockchip_i2s_play,
};

static struct snd_soc_dai_driver rockchip_i2s_dai = {
	.name = "rockchip_i2s_dai",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S8 |
			SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S20_3LE |
			SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &rockchip_i2s_dai_ops,
};

static const struct regmap_config rockchip_i2s_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = I2S_RXDR,
};

static int rockchip_i2s_probe(struct device *dev)
{
	struct rk_i2s_dev *i2s;

	i2s = xzalloc(sizeof(*i2s));

	i2s->regmap = dev_request_regmap_mmio(dev, &rockchip_i2s_regmap_config, NULL);
	if (IS_ERR(i2s->regmap))
		return PTR_ERR(i2s->regmap);

	i2s->hclk = clk_get_enabled(dev, "i2s_hclk");
	if (IS_ERR(i2s->hclk))
		return PTR_ERR(i2s->hclk);

	i2s->mclk = clk_get(dev, "i2s_clk");
	if (IS_ERR(i2s->mclk))
		return PTR_ERR(i2s->mclk);

	pinctrl_select_state(dev, "bclk_off");

	rockchip_snd_stop(i2s);

	dev->priv = i2s;

	return snd_soc_register_component(dev, NULL, &rockchip_i2s_dai, 1);
}

static void rockchip_i2s_remove(struct device *dev)
{
	struct rk_i2s_dev *i2s = dev->priv;

	// TODO mclk
	clk_disable(i2s->hclk);
}

static __maybe_unused struct of_device_id rockchip_i2s_dt_ids[] = {
	{ .compatible = "rockchip,rk3066-i2s", },
	{ /* sentinel */ }
};

static struct driver rockchip_i2s_driver = {
	.name   = "rockchip-i2s",
	.probe  = rockchip_i2s_probe,
	.remove = rockchip_i2s_remove,
	.of_compatible = rockchip_i2s_dt_ids,
};
device_platform_driver(rockchip_i2s_driver);
