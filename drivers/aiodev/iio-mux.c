// SPDX-License-Identifier: GPL-2.0
/*
 * IIO multiplexer driver
 *
 * Copyright (C) 2017 Axentia Technologies AB
 *
 * Author: Peter Rosin <peda@axentia.se>
 */

#include <linux/err.h>
#include <aiodev.h>
#include <linux/mutex.h>
#include <linux/overflow.h>
#include <linux/mux/consumer.h>
#include <linux/device.h>
#include <of.h>

struct mux {
	int cached_state;
	struct mux_control *control;
	struct aiochannel *parent;
	u32 delay_us;
	struct aiodevice indio_dev;
	u32 state[];
};

static int iio_mux_select(struct mux *mux, int idx)
{
	int ret;

	ret = mux_control_select_delay(mux->control, idx,
				       mux->delay_us);
	if (ret < 0) {
		mux->cached_state = -1;
		return ret;
	}

	if (mux->cached_state == idx)
		return 0;

	mux->cached_state = idx;

	return 0;
}

static void iio_mux_deselect(struct mux *mux)
{
	mux_control_deselect(mux->control);
}

static int mux_read_raw(struct aiochannel *chan,
			int *data)
{
	struct mux *mux = container_of(chan->aiodev, struct mux, indio_dev);
	int idx = chan->index;
	int ret;

	ret = iio_mux_select(mux, idx);
	if (ret < 0)
		return ret;

	ret = aiochannel_get_value(mux->parent, data);

	iio_mux_deselect(mux);

	return ret;
}

static int mux_configure_channel(struct device *dev, struct mux *mux,
				 u32 state, const char *label, int idx)
{
	struct aiodevice *indio_dev = &mux->indio_dev;

#if 0
	indio_dev->channels[idx] = &mux->channels[state];
	indio_dev->channels[idx].unit = "mV";
	indio_dev->channels[idx].name = label;
#endif

	if (state >= mux_control_states(mux->control)) {
		dev_err(dev, "too many channels\n");
		return -EINVAL;
	}

	return 0;
}

static int mux_probe(struct device *dev)
{
	struct device_node *np = dev_of_node(dev);
	struct aiodevice *indio_dev;
	struct aiochannel *parent;
	struct mux *mux;
	const char **labels;
	int all_children;
	int children;
	u32 state;
	int i;
	int ret;

	parent = aiochannel_get(dev, "parent");
	if (IS_ERR(parent))
		return dev_err_probe(dev, PTR_ERR(parent),
				     "failed to get parent channel\n");

	all_children = of_property_count_strings(np, "channels");
	if (all_children < 0)
		return all_children;

	labels = devm_kmalloc_array(dev, all_children, sizeof(*labels), GFP_KERNEL);
	if (!labels)
		return -ENOMEM;

	ret = of_property_read_string_array(np, "channels", labels, all_children);
	if (ret < 0)
		return ret;

	children = 0;
	for (state = 0; state < all_children; state++) {
		if (*labels[state])
			children++;
	}
	if (children <= 0) {
		dev_err(dev, "not even a single child\n");
		return -EINVAL;
	}

	mux = xzalloc(struct_size(mux, state, all_children));
	mux->parent = parent;
	mux->cached_state = -1;
	mux->delay_us = 0;

	of_property_read_u32(np, "settle-time-us", &mux->delay_us);

	indio_dev = &mux->indio_dev;

	ret = aiodevice_alloc_channels(indio_dev, children);
	if (ret)
		return ret;

	indio_dev->hwdev = dev;
	indio_dev->read = mux_read_raw;

	mux->control = mux_control_get(dev, NULL);
	if (IS_ERR(mux->control))
		return dev_err_probe(dev, PTR_ERR(mux->control),
				     "failed to get control-mux\n");

	i = 0;
	for (state = 0; state < all_children; state++) {
		if (!*labels[state])
			continue;

		ret = mux_configure_channel(dev, mux, state, labels[state], i++);
		if (ret < 0)
			return ret;
	}

	return aiodevice_register(indio_dev);
}

static const struct of_device_id mux_match[] = {
	{ .compatible = "io-channel-mux" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mux_match);

static struct driver mux_driver = {
	.probe = mux_probe,
	.name = "iio-mux",
	.of_match_table = mux_match,
};
device_platform_driver(mux_driver);

MODULE_DESCRIPTION("IIO multiplexer driver");
MODULE_AUTHOR("Peter Rosin <peda@axentia.se>");
MODULE_LICENSE("GPL v2");
