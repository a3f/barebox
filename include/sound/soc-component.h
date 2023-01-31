/* SPDX-License-Identifier: GPL-2.0
 *
 * soc-component.h
 *
 * Copyright (C) 2019 Renesas Electronics Corp.
 * Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
 */
#ifndef __SOC_COMPONENT_H
#define __SOC_COMPONENT_H

#include <sound/soc.h>

struct snd_soc_component;
struct device_node;

struct snd_soc_component_driver {
	const char *name;
	int (*probe)(struct snd_soc_component *component);
	int (*set_pll)(struct snd_soc_component *component);
};

unsigned int snd_soc_component_read(struct snd_soc_component *component,
				    unsigned int reg);
int snd_soc_component_write(struct snd_soc_component *component,
			    unsigned int reg, unsigned int val);
int snd_soc_component_update_bits(struct snd_soc_component *component,
				  unsigned int reg, unsigned int mask,
				  unsigned int val);

struct snd_soc_component *of_snd_soc_component_get(struct device_node *np);

static inline void snd_soc_component_init_regmap(struct snd_soc_component *component,
						 struct regmap *regmap)
{
	component->regmap = regmap;
}

static inline void snd_soc_component_exit_regmap(struct snd_soc_component *component)
{
}

static inline void snd_soc_component_set_drvdata(struct snd_soc_component *component, void *data)
{
	component->dev->priv = data;
}

static inline void *snd_soc_component_get_drvdata(struct snd_soc_component *component)
{
	return component->dev->priv;
}

#endif
