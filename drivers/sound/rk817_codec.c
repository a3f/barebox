// SPDX-License-Identifier: GPL-2.0-only
//
// rk817 ALSA SoC Audio driver
//
// Copyright (c) 2018, Fuzhou Rockchip Electronics Co., Ltd All rights reserved.

#define pr_fmt(fmt) "rk817_codec: " fmt

#include <linux/clk.h>
#include <linux/printk.h>
#include <driver.h>
#include <init.h>
#include <sound/soc.h>
#include <clock.h>
#include <regmap.h>
#include <linux/mfd/rk808.h>
#include <of.h>
#include <gpiod.h>
#include <sound.h>

#include "rk817_codec.h"

#define RK817_PLAYBACK_RATES (SNDRV_PCM_RATE_8000 |\
			      SNDRV_PCM_RATE_16000 |	\
			      SNDRV_PCM_RATE_32000 |	\
			      SNDRV_PCM_RATE_44100 |	\
			      SNDRV_PCM_RATE_48000 |	\
			      SNDRV_PCM_RATE_96000)

#define RK817_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			SNDRV_PCM_FMTBIT_S20_3LE |\
			SNDRV_PCM_FMTBIT_S24_LE |\
			SNDRV_PCM_FMTBIT_S32_LE)

struct rk817_codec_priv {
	struct clk *mclk;
	unsigned int spk_volume;
	unsigned int hp_volume;
	bool use_ext_amplifier;
	long int playback_path;
};

/*
 * DDAC L/R volume setting
 * 0db~-95db,0.375db/step,for example:
 * 0: 0dB
 * 0x0a: -3.75dB
 * 0x7d: -46dB
 * 0xff: -95dB
 */
#define INITIAL_VOLUME		0x03

static struct rk817_reg_val_typ playback_power_up_list[] = {
	{ RK817_CODEC_AREF_RTCFG1, 0x40 },
	{ RK817_CODEC_DDAC_POPD_DACST, 0x02 },
	{ RK817_CODEC_DDAC_SR_LMT0, 0x02 },
/*	{ RK817_CODEC_DTOP_DIGEN_CLKE, 0x0f }, */
	/* APLL */
	{ RK817_CODEC_APLL_CFG0, 0x04 },
	{ RK817_CODEC_APLL_CFG1, 0x58 },
	{ RK817_CODEC_APLL_CFG2, 0x2d },
	{ RK817_CODEC_APLL_CFG3, 0x0c },
	{ RK817_CODEC_APLL_CFG4, 0xa5 },
	{ RK817_CODEC_APLL_CFG5, 0x00 },

	{ RK817_CODEC_DI2S_RXCMD_TSD, 0x00 },
	{ RK817_CODEC_DI2S_RSD, 0x00 },
/*	{ RK817_CODEC_DI2S_CKM, 0x00 }, */
	{ RK817_CODEC_DI2S_RXCR1, 0x00 },
	{ RK817_CODEC_DI2S_RXCMD_TSD, 0x20 },
	{ RK817_CODEC_DTOP_VUCTIME, 0xf4 },
	{ RK817_CODEC_DDAC_MUTE_MIXCTL, 0x00 },

	{ RK817_CODEC_DDAC_VOLL, 0x0a },
	{ RK817_CODEC_DDAC_VOLR, 0x0a },
};

#define RK817_CODEC_PLAYBACK_POWER_UP_LIST_LEN \
	ARRAY_SIZE(playback_power_up_list)

static struct rk817_reg_val_typ playback_power_down_list[] = {
	{ RK817_CODEC_DDAC_MUTE_MIXCTL, 0x01 },
	{ RK817_CODEC_ADAC_CFG1, 0x0f },
	/* HP */
	{ RK817_CODEC_AHP_CFG0, 0xe0 },
	{ RK817_CODEC_AHP_CP, 0x09 },
	/* SPK */
	{ RK817_CODEC_ACLASSD_CFG1, 0x69 },
};

#define RK817_CODEC_PLAYBACK_POWER_DOWN_LIST_LEN \
	ARRAY_SIZE(playback_power_down_list)

static void rk817_codec_power_up(struct snd_soc_component *component)
{
	int i;

	snd_soc_component_update_bits(component, RK817_CODEC_DTOP_DIGEN_CLKE,
				      DAC_DIG_CLK_MASK, DAC_DIG_CLK_EN);
	for (i = 0; i < RK817_CODEC_PLAYBACK_POWER_UP_LIST_LEN; i++) {
		snd_soc_component_write(component, playback_power_up_list[i].reg,
					playback_power_up_list[i].value);
	}
}

static void rk817_codec_power_down(struct snd_soc_component *component)
{
	int i;

	/* mute output for pop noise */
	snd_soc_component_update_bits(component, RK817_CODEC_DDAC_MUTE_MIXCTL,
				      DACMT_ENABLE, DACMT_ENABLE);

	for (i = 0; i < RK817_CODEC_PLAYBACK_POWER_DOWN_LIST_LEN; i++) {
		snd_soc_component_write(component, playback_power_down_list[i].reg,
					playback_power_down_list[i].value);
	}

	snd_soc_component_update_bits(component, RK817_CODEC_DTOP_DIGEN_CLKE,
				      DAC_DIG_CLK_MASK, DAC_DIG_CLK_DIS);
}

static int rk817_playback_path_put(struct snd_soc_component *component, int path)
{
	struct rk817_codec_priv *rk817_codec_data = snd_soc_component_get_drvdata(component);
	long int pre_path;

	if (rk817_codec_data->playback_path == path) {
		pr_debug("%s: playback_path is not changed!\n", __func__);
		return 0;
	}

	pre_path = rk817_codec_data->playback_path;
	rk817_codec_data->playback_path = path;

	pr_debug("%s : set playback_path %ld, pre_path %ld\n",
		 __func__, rk817_codec_data->playback_path, pre_path);

	switch (rk817_codec_data->playback_path) {
	case OFF:
		rk817_codec_power_down(component);
		break;
	case RCV:
	case SPK_PATH:
	case RING_SPK:
		if (pre_path == OFF)
			rk817_codec_power_up(component);
		if (!rk817_codec_data->use_ext_amplifier) {
			/* power on dac ibias/l/r */
			snd_soc_component_write(component, RK817_CODEC_ADAC_CFG1,
						PWD_DACBIAS_ON | PWD_DACD_ON |
						PWD_DACL_ON | PWD_DACR_ON);
			/* CLASS D mode */
			snd_soc_component_write(component, RK817_CODEC_DDAC_MUTE_MIXCTL, 0x10);
			/* CLASS D enable */
			snd_soc_component_write(component, RK817_CODEC_ACLASSD_CFG1, 0xa5);
			/* restart CLASS D, OCPP/N */
			snd_soc_component_write(component, RK817_CODEC_ACLASSD_CFG2, 0xc4);
		} else {
			/* HP_CP_EN , CP 2.3V */
			snd_soc_component_write(component, RK817_CODEC_AHP_CP, 0x11);
			/* power on HP two stage opamp ,HP amplitude 0db */
			snd_soc_component_write(component, RK817_CODEC_AHP_CFG0, 0x80);
			/* power on dac ibias/l/r */
			snd_soc_component_write(component, RK817_CODEC_ADAC_CFG1,
				     PWD_DACBIAS_ON | PWD_DACD_DOWN |
				     PWD_DACL_ON | PWD_DACR_ON);
			snd_soc_component_update_bits(component, RK817_CODEC_DDAC_MUTE_MIXCTL,
					   DACMT_ENABLE, DACMT_DISABLE);
		}
		snd_soc_component_write(component, RK817_CODEC_DDAC_VOLL,
					rk817_codec_data->spk_volume);
		snd_soc_component_write(component, RK817_CODEC_DDAC_VOLR,
					rk817_codec_data->spk_volume);
		break;
	case HP_PATH:
	case HP_NO_MIC:
	case RING_HP:
	case RING_HP_NO_MIC:
		if (pre_path == OFF)
			rk817_codec_power_up(component);
		/* HP_CP_EN , CP 2.3V */
		snd_soc_component_write(component, RK817_CODEC_AHP_CP, 0x11);
		/* power on HP two stage opamp ,HP amplitude 0db */
		snd_soc_component_write(component, RK817_CODEC_AHP_CFG0, 0x80);
		/* power on dac ibias/l/r */
		snd_soc_component_write(component, RK817_CODEC_ADAC_CFG1,
			     PWD_DACBIAS_ON | PWD_DACD_DOWN |
			     PWD_DACL_ON | PWD_DACR_ON);
		snd_soc_component_update_bits(component, RK817_CODEC_DDAC_MUTE_MIXCTL,
				   DACMT_ENABLE, DACMT_DISABLE);

		snd_soc_component_write(component, RK817_CODEC_DDAC_VOLL,
					rk817_codec_data->hp_volume);
		snd_soc_component_write(component, RK817_CODEC_DDAC_VOLR,
					rk817_codec_data->hp_volume);
		break;
	case BT:
		break;
	case SPK_HP:
	case RING_SPK_HP:
		if (pre_path == OFF)
			rk817_codec_power_up(component);

		/* HP_CP_EN , CP 2.3V  */
		snd_soc_component_write(component, RK817_CODEC_AHP_CP, 0x11);
		/* power on HP two stage opamp ,HP amplitude 0db */
		snd_soc_component_write(component, RK817_CODEC_AHP_CFG0, 0x80);

		/* power on dac ibias/l/r */
		snd_soc_component_write(component, RK817_CODEC_ADAC_CFG1,
			     PWD_DACBIAS_ON | PWD_DACD_ON |
			     PWD_DACL_ON | PWD_DACR_ON);

		if (!rk817_codec_data->use_ext_amplifier) {
			/* CLASS D mode */
			snd_soc_component_write(component, RK817_CODEC_DDAC_MUTE_MIXCTL, 0x10);
			/* CLASS D enable */
			snd_soc_component_write(component, RK817_CODEC_ACLASSD_CFG1, 0xa5);
			/* restart CLASS D, OCPP/N */
			snd_soc_component_write(component, RK817_CODEC_ACLASSD_CFG2, 0xc4);
		}

		snd_soc_component_write(component, RK817_CODEC_DDAC_VOLL,
					rk817_codec_data->hp_volume);
		snd_soc_component_write(component, RK817_CODEC_DDAC_VOLR,
					rk817_codec_data->hp_volume);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int rk817_set_dai_fmt(struct snd_soc_dai *codec_dai,
			     unsigned int fmt)
{
	struct snd_soc_component *component = codec_dai->component;
	unsigned int i2s_mst = 0;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBC_CFC:
		i2s_mst |= RK817_I2S_MODE_SLV;
		break;
	case SND_SOC_DAIFMT_CBP_CFP:
		i2s_mst |= RK817_I2S_MODE_MST;
		break;
	default:
		dev_err(component->dev, "%s : set master mask failed!\n", __func__);
		return -EINVAL;
	}

	snd_soc_component_update_bits(component, RK817_CODEC_DI2S_CKM,
				      RK817_I2S_MODE_MASK, i2s_mst);

	return 0;
}

static int rk817_hw_params(struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		snd_soc_component_write(component, RK817_CODEC_DI2S_RXCR2,
					VDW_RX_16BITS);
		snd_soc_component_write(component, RK817_CODEC_DI2S_TXCR2,
					VDW_TX_16BITS);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S32_LE:
		snd_soc_component_write(component, RK817_CODEC_DI2S_RXCR2,
					VDW_RX_24BITS);
		snd_soc_component_write(component, RK817_CODEC_DI2S_TXCR2,
					VDW_TX_24BITS);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int rk817_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_component *component = dai->component;

	snd_soc_component_update_bits(component,
				      RK817_CODEC_DDAC_MUTE_MIXCTL,
				      DACMT_MASK,
				      mute ? DACMT_ENABLE : DACMT_DISABLE);

	return 0;
}

static int rk817_init(struct snd_soc_component *component)
{
	snd_soc_component_write(component, RK817_CODEC_DDAC_POPD_DACST, 0x02);

	if (0) {
		snd_soc_component_write(component, RK817_CODEC_DDAC_SR_LMT0, 0x02);
		snd_soc_component_write(component, RK817_CODEC_DADC_SR_ACL0, 0x02);
		snd_soc_component_write(component, RK817_CODEC_DTOP_VUCTIME, 0xf4);
	} else {
		rk817_playback_path_put(component, SPK_HP);
	}

	return 0;
}

static int rk817_probe(struct snd_soc_component *component)
{
	struct regmap *regmap = dev_get_regmap(component->dev->parent, NULL);

	snd_soc_component_init_regmap(component, regmap);

	snd_soc_component_write(component, RK817_CODEC_DTOP_LPT_SRST, 0x40);

	return rk817_init(component);
}

static int rk817_set_component_pll(struct snd_soc_component *component)
{
	/* Set resistor value and charge pump current for PLL. */
	snd_soc_component_write(component, RK817_CODEC_APLL_CFG1, 0x58);
	/* Set the PLL feedback clock divide value (values not documented). */
	snd_soc_component_write(component, RK817_CODEC_APLL_CFG2, 0x2d);
	/* Set the PLL pre-divide value (values not documented). */
	snd_soc_component_write(component, RK817_CODEC_APLL_CFG3, 0x0c);
	/* Set the PLL VCO output clock divide and PLL divided ratio of PLL High Clk (values not
	 * documented).
	 */
	snd_soc_component_write(component, RK817_CODEC_APLL_CFG4, 0xa5);

	return 0;
}

static const struct snd_soc_component_driver soc_codec_dev_rk817 = {
	.probe = rk817_probe,
	.set_pll = rk817_set_component_pll,
};

static const struct snd_soc_dai_ops rk817_dai_ops = {
	.hw_params	= rk817_hw_params,
	.set_fmt	= rk817_set_dai_fmt,
	.mute_stream	= rk817_digital_mute,
};

static struct snd_soc_dai_driver rk817_dai[] = {
	{
		.name = "rk817-hifi",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 2,
			.channels_max = 8,
			.rates = RK817_PLAYBACK_RATES,
			.formats = RK817_FORMATS,
		},
		.ops = &rk817_dai_ops,
	},
};

static int rk817_platform_probe(struct device *dev)
{
	struct rk817_codec_priv *rk817_codec_data;
	struct clk *mclk;
	int ret;

	rk817_codec_data = xzalloc(sizeof(*rk817_codec_data));

	dev->priv = rk817_codec_data;

	mclk = clk_get_enabled(dev->parent, "mclk");
	if (IS_ERR(mclk))
		return dev_err_probe(dev, PTR_ERR(mclk), "unable to get enabled mclk\n");

	rk817_codec_data->mclk = mclk;

	rk817_codec_data->hp_volume = INITIAL_VOLUME;
	rk817_codec_data->spk_volume = INITIAL_VOLUME;
	rk817_codec_data->playback_path = OFF;

	ret = snd_soc_register_component(dev, &soc_codec_dev_rk817,
					 rk817_dai, ARRAY_SIZE(rk817_dai));
	if (ret < 0)
		return dev_err_probe(dev, ret, "register codec error\n");

	return 0;
}

static void rk817_platform_remove(struct device *dev)
{
	struct rk817_codec_priv *rk817_codec_data = dev->priv;
	clk_disable(rk817_codec_data->mclk);
}

static struct driver rk817_codec_driver = {
	.name = "rk817-codec",
	.probe = rk817_platform_probe,
	.remove = rk817_platform_remove,
};
device_platform_driver(rk817_codec_driver);

MODULE_DESCRIPTION("ASoC RK817 codec driver");
MODULE_AUTHOR("binyuan <kevan.lan@rock-chips.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:rk817-codec");
