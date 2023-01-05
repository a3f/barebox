// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <errno.h>
#include <driver.h>
#include <mach/linux.h>
#include <linux/time.h>
#include <linux/math64.h>
#include <of.h>
#include <sound.h>

struct sandbox_sound {
	struct sound_card card;
};

static int sandbox_sound_play(struct sound_card *card, const void *data, unsigned nsamples)
{
	if (!data) {
		sdl_sound_stop();
		return 0;
	}

	return sdl_sound_play(data, nsamples) ?: -EIO;
}

static int sandbox_sound_probe(struct device *dev)
{
	struct sandbox_sound *priv;
	struct sound_card *card;
	int ret;

	priv = xzalloc(sizeof(*priv));

	card = &priv->card;
	card->name = "SDL-Audio";
	card->play = sandbox_sound_play;

	/* Keep in-sync with arch/sandbox/os/sdl.c */
	card->params.samplingrate = 44000;
	card->params.bitspersample = 16;
	card->params.channels = 1;

	ret = sdl_sound_init(card->params.samplingrate);
	if (ret) {
		ret = -ENODEV;
		goto free_priv;
	}

	ret = sound_card_register(card);
	if (ret)
		goto sdl_sound_close;

	dev_info(dev, "probed\n");
	return 0;

sdl_sound_close:
	sdl_sound_close();
free_priv:
	free(priv);

	return ret;
}


static __maybe_unused struct of_device_id sandbox_sound_dt_ids[] = {
	{ .compatible = "barebox,sandbox-sound" },
	{ /* sentinel */ }
};

static struct driver sandbox_sound_drv = {
	.name  = "sandbox-sound",
	.of_compatible = sandbox_sound_dt_ids,
	.probe = sandbox_sound_probe,
};
device_platform_driver(sandbox_sound_drv);
