/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * core.c - Code implementing core functionality of AIODEV susbsystem
 *
 * Copyright (c) 2015 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
 * Copyright (c) 2015 Zodiac Inflight Innovation
 */

#ifndef __AIODEVICE_H
#define __AIODEVICE_H

#include <device.h>
#include <linux/errno.h>

struct aiodevice;
struct aiochannel {
	const char *unit;

	/* internal use */
	char *param_name;
	struct aiodevice *aiodev;
	int index;
	int value;
};

struct aiodevice {
	const char *name;
	int (*read)(struct aiochannel *, int *val);
	struct device dev;
	struct device *hwdev;
	struct aiochannel **channels;
	int num_channels;
};

int aiodevice_register(struct aiodevice *aiodev);

const char *aiodevice_name(struct aiodevice *aiodev);

struct aiochannel *aiochannel_get(struct device *dev, int index);
/* Find aiochannel by channel name, e.g. "aiodev0.in_value0_mV" */
struct aiochannel *aiochannel_by_name(const char *name);

int aiochannel_get_value(struct aiochannel *aiochan, int *value);
int aiochannel_get_index(struct aiochannel *aiochan);

static inline const char *aiochannel_get_unit(struct aiochannel *aiochan)
{
	return aiochan->unit;
}

extern struct class aiodevice_class;
#define for_each_aiodevice(aiodevice) \
	class_for_each_container_of_device(&aiodevice_class, aiodevice, dev)

#ifdef CONFIG_AIODEV
int aiochannel_name_get_value(const char *chname, int *value);
#else
static inline int aiochannel_name_get_value(const char *chname, int *value)
{
	return -EOPNOTSUPP;
}
#endif

#endif
