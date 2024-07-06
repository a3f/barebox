// SPDX-License-Identifier: GPL-2.0-only
/*
 * core.c - Code implementing core functionality of AIODEV susbsystem
 *
 * Copyright (c) 2015 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
 *
 * Copyright (c) 2015 Zodiac Inflight Innovation
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 */

#include <common.h>
#include <aiodev.h>
#include <malloc.h>

DEFINE_DEV_CLASS(aiodevice_class, "analogio");

const char *aiodevice_name(struct aiodevice *aiodev)
{
	return dev_name(&aiodev->dev);
}

struct aiochannel *aiochannel_by_name(const char *name)
{
	struct aiodevice *aiodev;
	size_t namelen = strlen(name);
	int i;

	for_each_aiodevice(aiodev) {
		if (strncmp(name, aiodevice_name(aiodev), namelen) ||
		    strcmp(name + namelen, "."))
			continue;

		for (i = 0; i < aiodev->num_channels; i++)
			if (!strcmp(name, aiodev->channels[i]->param_name))
				return aiodev->channels[i];
	}

	return ERR_PTR(-ENOENT);
}
EXPORT_SYMBOL(aiochannel_by_name);

struct aiochannel *aiochannel_get(struct device *dev, int index)
{
	struct of_phandle_args spec;
	struct aiodevice *aiodev;
	int ret, chnum = 0;

	if (!dev->of_node)
		return ERR_PTR(-EINVAL);

	ret = of_parse_phandle_with_args(dev->of_node,
					 "io-channels",
					 "#io-channel-cells",
					 index, &spec);
        if (ret)
                return ERR_PTR(ret);

	for_each_aiodevice(aiodev) {
		if (aiodev->hwdev->of_node == spec.np)
			goto found;
	}

	return ERR_PTR(-EPROBE_DEFER);

found:
	if (spec.args_count)
		chnum = spec.args[0];

	if (chnum >= aiodev->num_channels)
		return ERR_PTR(-EINVAL);

	return aiodev->channels[chnum];
}
EXPORT_SYMBOL(aiochannel_get);

int aiochannel_get_value(struct aiochannel *aiochan, int *value)
{
	struct aiodevice *aiodev = aiochan->aiodev;

	return aiodev->read(aiochan, value);
}
EXPORT_SYMBOL(aiochannel_get_value);

int aiochannel_name_get_value(const char *chname, int *value)
{
	struct aiochannel *aio;

	aio = aiochannel_by_name(chname);
	if (IS_ERR(aio))
		return PTR_ERR(aio);

	return aiochannel_get_value(aio, value);
}
EXPORT_SYMBOL(aiochannel_name_get_value);

int aiochannel_get_index(struct aiochannel *aiochan)
{
	return aiochan->index;
}
EXPORT_SYMBOL(aiochannel_get_index);

static int aiochannel_param_get_value(struct param_d *p, void *priv)
{
	struct aiochannel *aiochan = priv;

	return aiochannel_get_value(aiochan, &aiochan->value);
}

int aiodevice_register(struct aiodevice *aiodev)
{
	int i, ret;

	if (!aiodev->name && aiodev->hwdev &&
	    aiodev->hwdev->of_node) {
		aiodev->dev.id = DEVICE_ID_SINGLE;

		aiodev->name = of_alias_get(aiodev->hwdev->of_node);
	}

	if (!aiodev->name) {
		aiodev->name = "aiodev";
		aiodev->dev.id = DEVICE_ID_DYNAMIC;
	}

	dev_set_name(&aiodev->dev, aiodev->name);

	aiodev->dev.parent = aiodev->hwdev;

	ret = register_device(&aiodev->dev);
	if (ret)
		return ret;

	for (i = 0; i < aiodev->num_channels; i++) {
		struct aiochannel *aiochan = aiodev->channels[i];
		struct param_d *param;

		aiochan->index  = i;
		aiochan->aiodev = aiodev;

		if (!aiochan->param_name)
			aiochan->param_name = xasprintf("in_value%d_%s", i, aiochan->unit);

		param = dev_add_param_int(&aiodev->dev, aiochan->param_name,
					  NULL, aiochannel_param_get_value,
					  &aiochan->value, "%d", aiochan);
	}

	class_add_device(&aiodevice_class, &aiodev->dev);

	return 0;
}
EXPORT_SYMBOL(aiodevice_register);
