// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * pwm backlight support for barebox
 *
 * (C) Copyright 2014 Sascha Hauer, Pengutronix
 */
#include <common.h>
#include <malloc.h>
#include <init.h>
#include <video/backlight.h>
#include <pwm.h>
#include <linux/err.h>
#include <linux/device.h>
#include <of.h>
#include <regulator.h>
#include <linux/gpio/consumer.h>
#include <linux/math64.h>

struct pwm_backlight {
	struct backlight_device backlight;
	struct pwm_device *pwm;
	struct regulator *power;
	uint32_t period;
	unsigned int *levels;
	struct gpio_desc *enable_gpio;
	int enabled;
	unsigned int scale;
};

static int backlight_pwm_enable(struct pwm_backlight *pwm_backlight)
{
	int ret;

	if (pwm_backlight->enabled)
		return 0;

	ret = pwm_enable(pwm_backlight->pwm);
	if (ret)
		return ret;

	regulator_enable(pwm_backlight->power);

	gpiod_direction_output(pwm_backlight->enable_gpio, true);

	pwm_backlight->enabled = 1;

	return 0;
}

static int backlight_pwm_disable(struct pwm_backlight *pwm_backlight)
{
	int ret;

	if (!pwm_backlight->enabled)
		return 0;

	ret = gpiod_direction_output(pwm_backlight->enable_gpio, false);
	if (!ret) {
		regulator_disable(pwm_backlight->power);

		/*
		 * Only disable PWM when an enable gpio is present.
		 * The output of the PWM is undefined when the PWM
		 * is disabled.
		 */
		pwm_disable(pwm_backlight->pwm);
		pwm_backlight->enabled = 0;
	}

	return 0;
}

static int compute_duty_cycle(struct pwm_backlight *pwm_backlight, int brightness)
{
	int duty_cycle;

	if (pwm_backlight->levels)
		duty_cycle = pwm_backlight->levels[brightness];
	else
		duty_cycle = brightness;

	return duty_cycle * pwm_backlight->period / pwm_backlight->scale;
}

static int backlight_pwm_set(struct backlight_device *backlight,
		int brightness)
{
	struct pwm_backlight *pwm_backlight = container_of(backlight,
			struct pwm_backlight, backlight);

	pwm_config(pwm_backlight->pwm, compute_duty_cycle(pwm_backlight, brightness),
		   pwm_backlight->period);

	if (brightness)
		return backlight_pwm_enable(pwm_backlight);
	else
		return backlight_pwm_disable(pwm_backlight);
}

#define PWM_LUMINANCE_SHIFT	16
#define PWM_LUMINANCE_SCALE	(1 << PWM_LUMINANCE_SHIFT) /* luminance scale */

/*
 * CIE lightness to PWM conversion.
 *
 * The CIE 1931 lightness formula is what actually describes how we perceive
 * light:
 *          Y = (L* / 903.3)           if L* ≤ 8
 *          Y = ((L* + 16) / 116)^3    if L* > 8
 *
 * Where Y is the luminance, the amount of light coming out of the screen, and
 * is a number between 0.0 and 1.0; and L* is the lightness, how bright a human
 * perceives the screen to be, and is a number between 0 and 100.
 *
 * The following function does the fixed point maths needed to implement the
 * above formula.
 */
static u64 cie1931(unsigned int lightness)
{
	u64 retval;

	/*
	 * @lightness is given as a number between 0 and 1, expressed
	 * as a fixed-point number in scale
	 * PWM_LUMINANCE_SCALE. Convert to a percentage, still
	 * expressed as a fixed-point number, so the above formulas
	 * can be applied.
	 */
	lightness *= 100;
	if (lightness <= (8 * PWM_LUMINANCE_SCALE)) {
		retval = DIV_ROUND_CLOSEST(lightness * 10, 9033);
	} else {
		retval = (lightness + (16 * PWM_LUMINANCE_SCALE)) / 116;
		retval *= retval * retval;
		retval += 1ULL << (2*PWM_LUMINANCE_SHIFT - 1);
		retval >>= 2*PWM_LUMINANCE_SHIFT;
	}

	return retval;
}

/*
 * Create a default correction table for PWM values to create linear brightness
 * for LED based backlights using the CIE1931 algorithm.
 */
static
int pwm_backlight_brightness_default(struct device *dev,
				     struct pwm_backlight *pwm_backlight,
				     unsigned int period)
{
	unsigned int i;
	u64 retval;

	/*
	 * Once we have 4096 levels there's little point going much higher...
	 * neither interactive sliders nor animation benefits from having
	 * more values in the table.
	 */
	pwm_backlight->backlight.brightness_max =
		min((int)DIV_ROUND_UP(period, fls(period)), 4096);

	pwm_backlight->levels = devm_kcalloc(dev, pwm_backlight->backlight.brightness_max,
				    sizeof(*pwm_backlight->levels), GFP_KERNEL);
	if (!pwm_backlight->levels)
		return -ENOMEM;

	/* Fill the table using the cie1931 algorithm */
	for (i = 0; i < pwm_backlight->backlight.brightness_max; i++) {
		retval = cie1931((i * PWM_LUMINANCE_SCALE) /
				 pwm_backlight->backlight.brightness_max) * period;
		retval = DIV_ROUND_CLOSEST_ULL(retval, PWM_LUMINANCE_SCALE);
		if (retval > UINT_MAX)
			return -EINVAL;
		pwm_backlight->levels[i] = (unsigned int)retval;
	}

	pwm_backlight->backlight.brightness_default
		= pwm_backlight->backlight.brightness_max / 2;
	pwm_backlight->backlight.brightness_max--;

	return 0;
}

static int pwm_backlight_parse_dt(struct device *dev,
				  struct pwm_backlight *pwm_backlight)
{
	struct device_node *node = dev->of_node;
	struct property *prop;
	int length;
	u32 value;
	int ret, i;

	if (!node)
		return -ENODEV;

	/* determine the number of brightness levels */
	prop = of_find_property(node, "brightness-levels", &length);
	if (!prop)
		return 0;

	ret = of_property_read_u32(node, "default-brightness-level",
					   &value);
	if (ret < 0)
		return ret;

	pwm_backlight->backlight.brightness_default = value;

	length /= sizeof(u32);

	/* read brightness levels from DT property */
	if (length > 0) {
		size_t size = sizeof(*pwm_backlight->levels) * length;

		pwm_backlight->levels = xzalloc(size);

		ret = of_property_read_u32_array(node, "brightness-levels",
						 pwm_backlight->levels,
						 length);
		if (ret < 0)
			return ret;

		pwm_backlight->backlight.brightness_max = length - 1;

		for (i = 0; i < length; i++)
			if (pwm_backlight->levels[i] > pwm_backlight->scale)
				pwm_backlight->scale = pwm_backlight->levels[i];

		if (pwm_backlight->scale == 0)
			return -EINVAL;
	} else {
		/* We implicitly assume here a linear levels array { 0, 1, 2, ... 100 } */
		pwm_backlight->scale = 100;
		pwm_backlight->backlight.brightness_max = pwm_backlight->scale;
	}

	return 0;
}

static int backlight_pwm_of_probe(struct device *dev)
{
	int ret;
	struct pwm_backlight *pwm_backlight;
	struct pwm_device *pwm;

	pwm = of_pwm_request(dev->of_node, NULL);
	if (IS_ERR(pwm)) {
		dev_err(dev, "Cannot find PWM device\n");
		return PTR_ERR(pwm);
	}

	pwm_backlight = xzalloc(sizeof(*pwm_backlight));
	pwm_backlight->pwm = pwm;
	pwm_backlight->period = pwm_get_period(pwm);

	ret = pwm_backlight_parse_dt(dev, pwm_backlight);
	if (ret)
		return ret;
	if (!pwm_backlight->backlight.brightness_max) {
		struct pwm_state state;
		int i;

		/*
		 * If no brightness levels are provided and max_brightness is
		 * not set, use the default brightness table. For the DT case,
		 * max_brightness is set to 0 when brightness levels is not
		 * specified. For the non-DT case, max_brightness is usually
		 * set to some value.
		 */

		/* Get the PWM period (in nanoseconds) */
		pwm_get_state(pwm_backlight->pwm, &state);

		ret = pwm_backlight_brightness_default(dev, pwm_backlight,
						       state.period);
		if (ret < 0)
			return dev_err_probe(dev, ret,
				      "failed to setup default brightness table\n");

		for (i = 0; i <= pwm_backlight->backlight.brightness_max; i++) {
			if (pwm_backlight->levels[i] > pwm_backlight->scale)
				pwm_backlight->scale = pwm_backlight->levels[i];
		}
	}

	pwm_backlight->enable_gpio = gpiod_get_optional(dev, "enable-gpios", 0);

	pwm_backlight->power = regulator_get(dev, "power");
	if (IS_ERR(pwm_backlight->power)) {
		dev_err(dev, "Cannot find regulator\n");
		return PTR_ERR(pwm_backlight->power);
	}

	pwm_backlight->backlight.slew_time_ms = 100;
	pwm_backlight->backlight.brightness_set = backlight_pwm_set;
	pwm_backlight->backlight.dev.parent = dev;
	pwm_backlight->backlight.node = dev->of_node;

	ret = backlight_register(&pwm_backlight->backlight);
	if (ret)
		return ret;

	return 0;
}

static struct of_device_id backlight_pwm_of_ids[] = {
	{
		.compatible = "pwm-backlight",
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, backlight_pwm_of_ids);

static struct driver backlight_pwm_of_driver = {
	.name  = "pwm-backlight",
	.probe = backlight_pwm_of_probe,
	.of_compatible = DRV_OF_COMPAT(backlight_pwm_of_ids),
};
device_platform_driver(backlight_pwm_of_driver);
