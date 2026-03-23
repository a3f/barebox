// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Amlogic Meson Reset Auxiliary driver
 *
 * Copyright (c) 2024 BayLibre, SAS.
 * Author: Jerome Brunet <jbrunet@baylibre.com>
 */

/*
 * SMA REVIEW: THIS ENTIRE DRIVER CANNOT WORK AS-IS IN BAREBOX.
 *
 * In Linux, this driver binds to auxiliary devices created by the parent
 * audio clock controller (axg-audio.c) via the auxiliary_bus. The auxiliary
 * bus creates virtual sub-devices that share the parent's regmap. Barebox
 * does NOT have an auxiliary bus.
 *
 * The meson_reset_aux_ids[] table below uses auxiliary device names
 * (e.g. "a1-audio-clkc.rst-a1") as .compatible strings, but these are
 * NOT device tree compatible strings — they will NEVER match any DT node.
 *
 * RECOMMENDED FIX: Remove this driver entirely and instead have the parent
 * audio clock driver (axg-audio.c) call meson_reset_controller_register()
 * directly. This is the same pattern already used by meson8b.c and
 * meson-aoclk.c clock drivers in barebox, which register reset controllers
 * from within the clock driver probe.
 *
 * In axg-audio.c, replace the auxiliary device creation block:
 *
 *   // Instead of:
 *   //   if (data->rst_drvname) {
 *   //       auxdev = __devm_auxiliary_device_create(...);
 *   //   }
 *
 *   // Do this:
 *   //   #include "../../reset/amlogic/reset-meson.h"  (or move params to a shared header)
 *   //   if (data->rst_param) {
 *   //       ret = meson_reset_controller_register(dev, map, data->rst_param);
 *   //       if (ret)
 *   //           return ret;
 *   //   }
 *
 * And change struct audioclk_data to hold a pointer to meson_reset_param
 * instead of rst_drvname string. E.g.:
 *
 *   // struct audioclk_data {
 *   //     struct meson_clk_hw_data hw_clks;
 *   //     const struct meson_reset_param *rst_param;  // replaces rst_drvname
 *   //     unsigned int max_register;
 *   // };
 *
 * Then for g12a_audioclk_data:
 *   //  .rst_param = &meson_g12a_audio_param,
 * And for sm1_audioclk_data:
 *   //  .rst_param = &meson_sm1_audio_param,
 *
 * The meson_reset_param structs below can be moved to a shared header
 * or kept here and exported. See Kconfig note too.
 *
 * ALTERNATIVE (if you want to keep this as a separate driver):
 * You could make the parent clock driver register a platform_device child
 * with platform_data pointing to the correct meson_reset_param. Then this
 * driver would be a normal platform driver matching by name. But this adds
 * complexity with no benefit in a bootloader context.
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/device.h>
#include <of.h>
/* SMA REVIEW: auxiliary_bus.h does not exist in barebox - correctly removed */
//#include <linux/auxiliary_bus.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>

#include "reset-meson.h"

/*
 * SMA REVIEW: These param structs are correct and can be reused.
 * If you go with the recommended fix (register from axg-audio.c directly),
 * either move these to reset-meson.h or export them from here.
 */
static const struct meson_reset_param meson_a1_audio_param = {
	.reset_ops	= &meson_reset_toggle_ops,
	.reset_num	= 32,
	.level_offset	= 0x28,
};

static const struct meson_reset_param meson_a1_audio_vad_param = {
	.reset_ops	= &meson_reset_toggle_ops,
	.reset_num	= 6,
	.level_offset	= 0x8,
};

static const struct meson_reset_param meson_g12a_audio_param = {
	.reset_ops	= &meson_reset_toggle_ops,
	.reset_num	= 26,
	.level_offset	= 0x24,
};

static const struct meson_reset_param meson_sm1_audio_param = {
	.reset_ops	= &meson_reset_toggle_ops,
	.reset_num	= 39,
	.level_offset	= 0x28,
};

/*
 * SMA REVIEW BUG: This of_device_id table uses Linux auxiliary_device names
 * as .compatible strings. These are NOT valid DT compatible strings and will
 * never match any device tree node. This table is fundamentally wrong for
 * barebox. See the top-of-file comment for the recommended fix.
 */
static const struct of_device_id meson_reset_aux_ids[] = {
	{
		.compatible = "a1-audio-clkc.rst-a1",
		.data = &meson_a1_audio_param,
	}, {
		.compatible = "a1-audio-clkc.rst-a1-vad",
		.data = &meson_a1_audio_vad_param,
	}, {
		.compatible = "axg-audio-clkc.rst-g12a",
		.data = &meson_g12a_audio_param,
	}, {
		.compatible = "axg-audio-clkc.rst-sm1",
		.data = &meson_sm1_audio_param,
	}, {}
};
MODULE_DEVICE_TABLE(of, meson_reset_aux_ids);

static int meson_reset_aux_probe(struct device *dev)
{
	/*
	 * SMA REVIEW BUG: This probe function is broken in two ways:
	 *
	 * 1) param is never retrieved. In Linux, the auxiliary_device_id
	 *    passed the param via id->driver_data. In barebox with
	 *    of_device_id, you would use device_get_match_data(dev).
	 *    But the match table itself is wrong (see above), so this
	 *    will never be called anyway.
	 *
	 * 2) meson_reset_controller_register() is never called, so
	 *    even if this probe ran, no reset controller would be
	 *    registered.
	 *
	 * IF you wanted to fix just this function (without fixing the
	 * fundamental auxiliary bus problem), it would look like:
	 */
	// const struct meson_reset_param *param;
	struct regmap *map;

	// param = device_get_match_data(dev);
	// if (!param)
	//	return -ENODEV;

	map = dev_get_regmap(dev->parent, NULL);
	if (!map)
		return -EINVAL;

	// return meson_reset_controller_register(dev, map, param);
	return 0;
}

static struct driver meson_reset_aux_driver = {
	.probe		= meson_reset_aux_probe,
	.of_match_table	= meson_reset_aux_ids,
};
core_platform_driver(meson_reset_aux_driver);

MODULE_DESCRIPTION("Amlogic Meson Reset Auxiliary driver");
MODULE_AUTHOR("Jerome Brunet <jbrunet@baylibre.com>");
MODULE_LICENSE("Dual BSD/GPL");
