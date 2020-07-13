// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (C) 2010, Lars-Peter Clausen <lars@metafoo.de>
 *  PWM beeper driver
 */

#include <common.h>
#include <regulator.h>
#include <of.h>
#include <pwm.h>
#include <poller.h>

struct pwm_beeper {
	struct cdev cdev;
	struct pwm_device *pwm;
	struct regulator *amplifier;
	struct poller_async work;
	unsigned long period;
	unsigned int bell_frequency;
	bool suspended;
	bool amplifier_on;
};

enum snd { SND_BELL, SND_TONE };

#define HZ_TO_NANOSECONDS(x) (1000000000UL/(x))

static int pwm_beeper_on(struct pwm_beeper *beeper, unsigned long period)
{
	struct pwm_state state;
	int error;

	pwm_get_state(beeper->pwm, &state);

	state.p_enable = true;
	state.period_ns = period;
	pwm_set_relative_duty_cycle(&state, 50, 100);

	error = pwm_apply_state(beeper->pwm, &state);
	if (error)
		return error;

	if (!beeper->amplifier_on) {
		error = regulator_enable(beeper->amplifier);
		if (error) {
			pwm_disable(beeper->pwm);
			return error;
		}

		beeper->amplifier_on = true;
	}

	return 0;
}

static void pwm_beeper_off(struct pwm_beeper *beeper)
{
	if (beeper->amplifier_on) {
		regulator_disable(beeper->amplifier);
		beeper->amplifier_on = false;
	}

	pwm_disable(beeper->pwm);
}

static void pwm_beeper_work(void *priv)
{
	struct pwm_beeper *beeper = priv;
	unsigned long period = beeper->period;

	if (period)
		pwm_beeper_on(beeper, period);
	else
		pwm_beeper_off(beeper);
}

static int pwm_beeper_event(struct pwm_beeper *beeper, unsigned int code, unsigned int value)
{
	switch (code) {
	case SND_BELL:
		value = value ? beeper->bell_frequency : 0;
		break;
	case SND_TONE:
		break;
	default:
		return -EINVAL;
	}

	if (value == 0)
		beeper->period = 0;
	else
		beeper->period = HZ_TO_NANOSECONDS(value);

	if (!beeper->suspended)
		poller_call_async(&beeper->work, 0, pwm_beeper_work, beeper);

	return 0;
}

static int pwm_beeper_cdev_write(struct cdev *cdev, const void *buf, size_t count,
				 loff_t offset, ulong flags)
{
	struct pwm_beeper *beeper = cdev->priv;
	u32 value;
	int error = -EINVAL;

	switch (count) {
	case 4:
		memcpy(&value, buf, sizeof value);
		error = pwm_beeper_event(beeper, SND_TONE, value);
		break;
	case 1:
		value = *(u8 *)buf;
		error = pwm_beeper_event(beeper, SND_BELL, value);
		break;
	}

	return error == 0 ? count : error;
}

static void pwm_beeper_stop(struct pwm_beeper *beeper)
{
	beeper->suspended = true;
	pwm_beeper_off(beeper);
}

static int pwm_beeper_cdev_close(struct cdev *cdev)
{
	struct pwm_beeper *beeper = cdev->priv;

	pwm_beeper_stop(beeper);
	return 0;
}

static struct cdev_operations beeperops = {
	.write = pwm_beeper_cdev_write,
	.close = pwm_beeper_cdev_close,
};

static int pwm_beeper_probe(struct device_d *dev)
{
	struct pwm_beeper *beeper;
	struct pwm_state state;
	u32 bell_frequency;
	int error;

	beeper = xzalloc(sizeof(*beeper));
	dev->priv = beeper;

	beeper->pwm = of_pwm_request(dev->device_node, NULL);
	if (IS_ERR(beeper->pwm)) {
		error = PTR_ERR(beeper->pwm);
		if (error != -EPROBE_DEFER)
			dev_err(dev, "Failed to request PWM device: %d\n",
				error);
		return error;
	}

	/* Sync up PWM state and ensure it is off. */
	pwm_init_state(beeper->pwm, &state);
	state.p_enable = false;
	error = pwm_apply_state(beeper->pwm, &state);
	if (error) {
		dev_err(dev, "failed to apply initial PWM state: %d\n",
			error);
		return error;
	}

	beeper->amplifier = regulator_get(dev, "amp");
	if (IS_ERR(beeper->amplifier)) {
		error = PTR_ERR(beeper->amplifier);
		if (error != -EPROBE_DEFER)
			dev_err(dev, "Failed to get 'amp' regulator: %d\n",
				error);
		return error;
	}

	error = poller_async_register(&beeper->work, dev_name(dev));
	if (error)
		return error;

	error = of_property_read_u32(dev->device_node, "beeper-hz", &bell_frequency);
	if (error) {
		bell_frequency = 1000;
		dev_dbg(dev,
			"failed to parse 'beeper-hz' property, using default: %uHz\n",
			bell_frequency);
	}

	beeper->bell_frequency = bell_frequency;

	beeper->cdev.name = "pwm-beeper";
	beeper->cdev.flags = DEVFS_IS_CHARACTER_DEV;
	beeper->cdev.ops = &beeperops;

	return devfs_create(&beeper->cdev);
}

static void pwm_beeper_suspend(struct device_d *dev)
{
	struct pwm_beeper *beeper = dev->priv;

	beeper->suspended = true;

	pwm_beeper_stop(beeper);
}

static const struct of_device_id pwm_beeper_match[] = {
	{ .compatible = "pwm-beeper", },
	{ },
};

static struct driver_d pwm_beeper_driver = {
	.name		= "pwm-beeper",
	.probe		= pwm_beeper_probe,
	.remove		= pwm_beeper_suspend,
	.of_compatible	= pwm_beeper_match,
};
device_platform_driver(pwm_beeper_driver);
