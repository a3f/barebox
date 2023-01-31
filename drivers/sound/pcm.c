// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (C) 2010, Lars-Peter Clausen <lars@metafoo.de>
 *  Copyright (C) 2021, Ahmad Fatoum
 */

#include <common.h>
#include <regulator.h>
#include <sound.h>
#include <of.h>
#include <pwm.h>

struct pcm_sound_card {
	struct sound_card card;
};

#define HZ_TO_NANOSECONDS(x) (1000000000UL/(x))

static int pcm_beep(struct sound_card *card, unsigned freq, unsigned duration)
{
	struct pcm_sound_card *pcm = container_of(card, struct pcm_sound_card, card);
	struct pwm_state state;
	int error = 0;

	sound_create_square_wave(i2s_uc_priv->samplingrate, data, data_size,
				 frequency_hz, i2s_uc_priv->channels);

	apl_ioctl

	if (!freq) {
		regulator_disable(beeper->amplifier);
		goto pwm_disable;
	}

	pwm_get_state(beeper->pwm, &state);

	state.p_enable = true;
	state.period_ns = HZ_TO_NANOSECONDS(freq);
	pwm_set_relative_duty_cycle(&state, 50, 100);

	error = pwm_apply_state(beeper->pwm, &state);
	if (error)
		return error;

	error = regulator_enable(beeper->amplifier);
	if (error)
		goto pwm_disable;

	return 0;
pwm_disable:
	pwm_disable(beeper->pwm);
	return error;
}

static int pwm_beeper_probe(struct device *dev)
{
	struct pwm_beeper *beeper;
	struct sound_card *card;
	struct pwm_state state;
	u32 bell_frequency;
	int error;

	beeper = xzalloc(sizeof(*beeper));
	dev->priv = beeper;

	beeper->pwm = of_pwm_request(dev->of_node, NULL);
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

	error = of_property_read_u32(dev->of_node, "beeper-hz",
				     &bell_frequency);
	if (error) {
		bell_frequency = 1000;
		dev_dbg(dev, "failed to parse 'beeper-hz' property, using default: %uHz\n",
			bell_frequency);
	}

	card = &beeper->card;
	card->name = dev->of_node->full_name;
	card->bell_frequency = bell_frequency;
	card->beep = pwm_beeper_beep;

	return sound_card_register(card);
}

static void pcm_sound_card_exit(struct pcm_sound_card *pcm)
{
}
