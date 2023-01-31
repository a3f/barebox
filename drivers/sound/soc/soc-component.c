// SPDX-License-Identifier: GPL-2.0
//
// soc-component.c
//
// Copyright 2009-2011 Wolfson Microelectronics PLC.
// Copyright (C) 2019 Renesas Electronics Corp.
//
// Mark Brown <broonie@opensource.wolfsonmicro.com>
// Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>

#include <sound/soc-component.h>
#include <linux/err.h>
#include <linux/printk.h>
#include <regmap.h>
#include <module.h>
#include <of.h>

#define for_each_component(component)			\
	list_for_each_entry(component, &component_list, list)

#define soc_component_ret_reg_rw(dai, ret, reg) _soc_component_ret(dai, __func__, ret, reg)

static inline int _soc_component_ret(struct snd_soc_component *component,
				     const char *func, int ret, int reg)
{
	/* Positive/Zero values are not errors */
	if (ret >= 0)
		return ret;

	/* Negative values might be errors */
	switch (ret) {
	case -EPROBE_DEFER:
	case -ENOTSUPP:
		break;
	default:
		dev_err(component->dev,
		       "ASoC: error at %s on %s for register: [0x%08x] %d\n",
		       func, regmap_name(component->regmap) ?: "regmap", reg, ret);
	}

	return ret;
}

unsigned int snd_soc_component_read(struct snd_soc_component *component,
				    unsigned int reg)
{
	unsigned int val;
	int ret;

	ret = regmap_read(component->regmap, reg, &val);
	if (ret < 0)
		return soc_component_ret_reg_rw(component, ret, reg);

	return val;
}
EXPORT_SYMBOL_GPL(snd_soc_component_read);

int snd_soc_component_write(struct snd_soc_component *component,
			    unsigned int reg, unsigned int val)
{
	int ret;

	ret = regmap_write(component->regmap, reg, val);

	return soc_component_ret_reg_rw(component, ret, reg);
}
EXPORT_SYMBOL_GPL(snd_soc_component_write);

int snd_soc_component_update_bits(struct snd_soc_component *component,
				  unsigned int reg, unsigned int mask,
				  unsigned int val)
{
	int ret;

	ret = regmap_update_bits(component->regmap, reg, mask, val);

	return soc_component_ret_reg_rw(component, ret, reg);
}
EXPORT_SYMBOL_GPL(snd_soc_component_update_bits);

static LIST_HEAD(component_list);

static void snd_soc_component_initialize(struct snd_soc_component *component,
					 const struct snd_soc_component_driver *driver,
					 struct device *dev)
{
	component->dev		= dev;
	component->driver	= driver;
}

static void snd_soc_add_component(struct snd_soc_component *component)
{
	if (!component->regmap)
		component->regmap = dev_get_regmap(component->dev,
						   NULL);

	/* see for_each_component */
	list_add(&component->list, &component_list);
}

int snd_soc_register_component(struct device *dev,
			       const struct snd_soc_component_driver *component_driver,
			       struct snd_soc_dai_driver *dai_drv,
			       int num_dai)
{
	struct snd_soc_dai *dai;

	if (num_dai != 1)
		return -ENOSYS;

	dai = kzalloc(sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	snd_soc_component_initialize(dai->component, component_driver, dev);

	snd_soc_add_component(dai->component);
	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_register_component);

struct snd_soc_component *of_snd_soc_component_get(struct device_node *np)
{
	struct snd_soc_component *component;

	of_device_ensure_probed(np);

	for_each_component(component) {
		if (dev_of_node(component->dev) == np)
			return component;
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(of_snd_soc_component_get);
