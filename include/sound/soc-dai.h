/* SPDX-License-Identifier: GPL-2.0
 *
 * linux/sound/soc-dai.h -- ALSA SoC Layer
 *
 * Copyright:	2005-2008 Wolfson Microelectronics. PLC.
 *
 * Digital Audio Interface (DAI) API.
 */

#ifndef __LINUX_SND_SOC_DAI_H
#define __LINUX_SND_SOC_DAI_H


#include <linux/list.h>
#include <linux/container_of.h>
#include <sound/asound.h>
#include <sound/pcm.h>
#include <driver.h>

struct regmap;
struct snd_soc_dai;

#define SND_SOC_DAI_FORMAT_I2S          1 /* I2S mode */

/*
 * DAI hardware audio formats.
 *
 * Describes the physical PCM data formating and clocking. Add new formats
 * to the end.
 */
#define SND_SOC_DAIFMT_I2S		SND_SOC_DAI_FORMAT_I2S

/* Describes the possible PCM format */
/*
 * use SND_SOC_DAI_FORMAT_xx as eash shift.
 * see
 *	snd_soc_runtime_get_dai_fmt()
 */
#define SND_SOC_POSSIBLE_DAIFMT_I2S		1

/*
 * DAI Clock gating.
 *
 * DAI bit clocks can be gated (disabled) when the DAI is not
 * sending or receiving PCM data in a frame. This can be used to save power.
 */
#define SND_SOC_DAIFMT_CONT		(1 << 4) /* continuous clock */
#define SND_SOC_DAIFMT_GATED		(0 << 4) /* clock is gated */

/* Describes the possible PCM format */
/*
 * define GATED -> CONT. GATED will be selected if both are selected.
 * see
 *	snd_soc_runtime_get_dai_fmt()
 */
#define SND_SOC_POSSIBLE_DAIFMT_CLOCK_SHIFT	16
#define SND_SOC_POSSIBLE_DAIFMT_CLOCK_MASK	(0xFFFF	<< SND_SOC_POSSIBLE_DAIFMT_CLOCK_SHIFT)
#define SND_SOC_POSSIBLE_DAIFMT_GATED		(0x1ULL	<< SND_SOC_POSSIBLE_DAIFMT_CLOCK_SHIFT)
#define SND_SOC_POSSIBLE_DAIFMT_CONT		(0x2ULL	<< SND_SOC_POSSIBLE_DAIFMT_CLOCK_SHIFT)

/*
 * DAI hardware signal polarity.
 *
 * Specifies whether the DAI can also support inverted clocks for the specified
 * format.
 *
 * BCLK:
 * - "normal" polarity means signal is available at rising edge of BCLK
 * - "inverted" polarity means signal is available at falling edge of BCLK
 *
 * FSYNC "normal" polarity depends on the frame format:
 * - I2S: frame consists of left then right channel data. Left channel starts
 *      with falling FSYNC edge, right channel starts with rising FSYNC edge.
 * - Left/Right Justified: frame consists of left then right channel data.
 *      Left channel starts with rising FSYNC edge, right channel starts with
 *      falling FSYNC edge.
 * - DSP A/B: Frame starts with rising FSYNC edge.
 * - AC97: Frame starts with rising FSYNC edge.
 *
 * "Negative" FSYNC polarity is the one opposite of "normal" polarity.
 */
#define SND_SOC_DAIFMT_NB_NF		(0 << 8) /* normal bit clock + frame */
#define SND_SOC_DAIFMT_NB_IF		(2 << 8) /* normal BCLK + inv FRM */
#define SND_SOC_DAIFMT_IB_NF		(3 << 8) /* invert BCLK + nor FRM */
#define SND_SOC_DAIFMT_IB_IF		(4 << 8) /* invert BCLK + FRM */

/* Describes the possible PCM format */
#define SND_SOC_POSSIBLE_DAIFMT_INV_SHIFT	32
#define SND_SOC_POSSIBLE_DAIFMT_INV_MASK	(0xFFFFULL << SND_SOC_POSSIBLE_DAIFMT_INV_SHIFT)
#define SND_SOC_POSSIBLE_DAIFMT_NB_NF		(0x1ULL    << SND_SOC_POSSIBLE_DAIFMT_INV_SHIFT)
#define SND_SOC_POSSIBLE_DAIFMT_NB_IF		(0x2ULL    << SND_SOC_POSSIBLE_DAIFMT_INV_SHIFT)
#define SND_SOC_POSSIBLE_DAIFMT_IB_NF		(0x4ULL    << SND_SOC_POSSIBLE_DAIFMT_INV_SHIFT)
#define SND_SOC_POSSIBLE_DAIFMT_IB_IF		(0x8ULL    << SND_SOC_POSSIBLE_DAIFMT_INV_SHIFT)

/*
 * DAI hardware clock providers/consumers
 *
 * This is wrt the codec, the inverse is true for the interface
 * i.e. if the codec is clk and FRM provider then the interface is
 * clk and frame consumer.
 */
#define SND_SOC_DAIFMT_CBP_CFP		(1 << 12) /* codec clk provider & frame provider */
#define SND_SOC_DAIFMT_CBC_CFP		(2 << 12) /* codec clk consumer & frame provider */
#define SND_SOC_DAIFMT_CBP_CFC		(3 << 12) /* codec clk provider & frame consumer */
#define SND_SOC_DAIFMT_CBC_CFC		(4 << 12) /* codec clk consumer & frame consumer */

/* when passed to set_fmt directly indicate if the device is provider or consumer */
#define SND_SOC_DAIFMT_BP_FP		SND_SOC_DAIFMT_CBP_CFP
#define SND_SOC_DAIFMT_BC_FP		SND_SOC_DAIFMT_CBC_CFP
#define SND_SOC_DAIFMT_BP_FC		SND_SOC_DAIFMT_CBP_CFC
#define SND_SOC_DAIFMT_BC_FC		SND_SOC_DAIFMT_CBC_CFC

/* Describes the possible PCM format */
#define SND_SOC_POSSIBLE_DAIFMT_CLOCK_PROVIDER_SHIFT	48
#define SND_SOC_POSSIBLE_DAIFMT_CLOCK_PROVIDER_MASK	(0xFFFFULL << SND_SOC_POSSIBLE_DAIFMT_CLOCK_PROVIDER_SHIFT)
#define SND_SOC_POSSIBLE_DAIFMT_CBP_CFP			(0x1ULL    << SND_SOC_POSSIBLE_DAIFMT_CLOCK_PROVIDER_SHIFT)
#define SND_SOC_POSSIBLE_DAIFMT_CBC_CFP			(0x2ULL    << SND_SOC_POSSIBLE_DAIFMT_CLOCK_PROVIDER_SHIFT)
#define SND_SOC_POSSIBLE_DAIFMT_CBP_CFC			(0x4ULL    << SND_SOC_POSSIBLE_DAIFMT_CLOCK_PROVIDER_SHIFT)
#define SND_SOC_POSSIBLE_DAIFMT_CBC_CFC			(0x8ULL    << SND_SOC_POSSIBLE_DAIFMT_CLOCK_PROVIDER_SHIFT)

#define SND_SOC_DAIFMT_FORMAT_MASK		0x000f
#define SND_SOC_DAIFMT_CLOCK_MASK		0x00f0
#define SND_SOC_DAIFMT_INV_MASK			0x0f00
#define SND_SOC_DAIFMT_CLOCK_PROVIDER_MASK	0xf000

#define SND_SOC_DAIFMT_MASTER_MASK	SND_SOC_DAIFMT_CLOCK_PROVIDER_MASK

/*
 * Master Clock Directions
 */
#define SND_SOC_CLOCK_IN		0
#define SND_SOC_CLOCK_OUT		1

unsigned int snd_soc_daifmt_clock_provider_from_bitmap(unsigned int bit_frame);

unsigned int snd_soc_daifmt_parse_clock_provider_raw(struct device_node *np,
						     const char *prefix,
						     struct device_node **bitclkmaster,
						     struct device_node **framemaster);
#define snd_soc_daifmt_parse_clock_provider_as_bitmap(np, prefix)	\
	snd_soc_daifmt_parse_clock_provider_raw(np, prefix, NULL, NULL)
#define snd_soc_daifmt_parse_clock_provider_as_phandle			\
	snd_soc_daifmt_parse_clock_provider_raw
#define snd_soc_daifmt_parse_clock_provider_as_flag(np, prefix)		\
	snd_soc_daifmt_clock_provider_from_bitmap(			\
		snd_soc_daifmt_parse_clock_provider_as_bitmap(np, prefix))

int snd_soc_dai_play(struct snd_soc_dai *dai,
		     const void *data, unsigned nsamples);
int snd_soc_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt);
int snd_soc_dai_digital_mute(struct snd_soc_dai *dai, int mute);
int snd_soc_dai_set_sysclk(struct snd_soc_dai *dai, int clk_id,
			   unsigned int freq, int dir);

/*
 * Digital Audio Interface Driver.
 *
 * Describes the Digital Audio Interface in terms of its ALSA, DAI and AC97
 * operations and capabilities. Codec and platform drivers will register this
 * structure for every DAI they have.
 *
 * This structure covers the clocking, formating and ALSA operations for each
 * interface.
 */
struct snd_soc_dai_driver {
	/* DAI description */
	const char *name;

	/* ops */
	const struct snd_soc_dai_ops *ops;

	/* DAI capabilities */
	struct snd_soc_pcm_stream playback;
};

struct snd_soc_component;

/*
 * Digital Audio Interface runtime data.
 *
 * Holds runtime data for a DAI.
 */
struct snd_soc_dai {
	const char *name;
	struct snd_soc_dai_driver *driver;
	struct device *dev;
	struct snd_soc_component *component;
};

struct snd_soc_component {
	struct regmap *regmap;
	struct device *dev;
	const struct snd_soc_component_driver *driver;
	struct list_head list;
	int ndais;
	struct snd_soc_dai dais[];
};

struct snd_soc_dai_ops {
	/*
	 * DAI clocking configuration, all optional.
	 * Called by soc_card drivers, normally in their hw_params.
	 */
	int (*set_sysclk)(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir);
	int (*set_bclk_ratio)(struct snd_soc_dai *dai, unsigned int ratio);

	/*
	 * DAI format configuration
	 * Called by soc_card drivers, normally in their hw_params.
	 */
	int (*set_fmt)(struct snd_soc_dai *dai, unsigned int fmt);

	/*
	 * DAI digital mute - optional.
	 * Called by soc-core to minimise any pops.
	 */
	int (*mute_stream)(struct snd_soc_dai *dai, int mute);

	/*
	 * ALSA PCM audio operations - all optional.
	 * Called by soc-core during audio PCM operations.
	 */
	int (*startup)(struct snd_soc_dai *);
	void (*shutdown)(struct snd_soc_dai *);
	int (*hw_params)(struct snd_pcm_hw_params *, struct snd_soc_dai *);

	int (*play)(struct snd_soc_dai *cpu_dai, const void *data, unsigned nsamples);
};

static inline void snd_soc_dai_set_drvdata(struct snd_soc_dai *dai, void *data)
{
	dai->dev->priv = data;
}

static inline void *snd_soc_dai_get_drvdata(struct snd_soc_dai *dai)
{
	return dai->dev->priv;
}

struct snd_soc_dai *of_snd_soc_dai_get_by_phandle(struct device_node *np,
						  const char *property);


#endif
