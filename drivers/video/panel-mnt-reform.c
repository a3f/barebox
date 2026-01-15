
// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2016 InforceComputing
 * Author: Vinay Simha BN <simhavcs@gmail.com>
 *
 * Copyright (C) 2016 Linaro Ltd
 * Author: Sumit Semwal <sumit.semwal@linaro.org>
 *
 * Copyright (C) 2024 MNT Research GmbH
 * Author: Lukas Hartmann <lukas@mntre.com>
 */

// #include <linux/backlight.h>
#include <clock.h>
// #include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
// #include <linux/of.h>
#include <of.h>
// #include <linux/regulator/consumer.h>
// #include <linux/panel-mnt-pocket-reform.h>

// #include <video/mipi_display.h>

// #include <drm/drm_crtc.h>
// #include <drm/drm_mipi_dsi.h>
// #include <drm/drm_modes.h>
// #include <drm/drm_panel.h>

#include <linux/mod_devicetable.h>
#include <of_device.h>
#include <regulator.h>
#include <video/media-bus-format.h>
#include <spi/spi.h>
#include <fb.h>
#include <linux/device.h>
#include <video/vpl.h>
#include <video/videomode.h>
#include <video/drm/drm_connector.h>
#include <video/mipi_display.h>
#include <video/mipi_dsi.h>
#include <video/backlight.h>



static const char * const regulator_names[] = {
	"vddp",
	"iovcc"
};

struct jdi_panel {
	// struct drm_panel base;
	struct mipi_dsi_device *dsi;

	struct device *dev;
	struct vpl vpl;

	struct regulator_bulk_data supplies[ARRAY_SIZE(regulator_names)];

	struct gpio_desc *enable_gpio;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *dcdc_en_gpio;
	struct backlight_device *backlight;

	bool prepared;
	bool enabled;

	int panel_version;
	int display_v1_init_count;
	int display_v2_init_count;
};

// static ssize_t panel_version_attr_show(struct device *dev, struct device_attribute *attr, char *buf) {
// 	struct jdi_panel *jdi = (struct jdi_panel *)dev_get_drvdata(dev);
// 	return sprintf(buf, "%d\n", jdi->panel_version);
// }
// static ssize_t panel_dcs_read_attr_show(struct device *dev, struct device_attribute *attr, char *buf) {
// 	return sprintf(buf, "%d\n", 0);
// }
// static ssize_t panel_dcs_read_attr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
// 	struct jdi_panel *jdi = (struct jdi_panel *)dev_get_drvdata(dev);
// 	long command;
// 	int ret = (int)kstrtol(buf, 10, &command);
// 	u8 readval = 0;
// 	if (ret == 0 && command > 0) {
// 		jdi->dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;
// 		mipi_dsi_dcs_read(jdi->dsi, (int)command, &readval, sizeof(readval));
// 		dev_err(dev, "[mnt pocket reform display] DCS command %d read result: 0x%x\n", (int)command, (int)readval);
// 	}
// 	return count;
// }
// static DEVICE_ATTR(mnt_pocket_reform_panel_version, 0444, panel_version_attr_show, NULL);
// static DEVICE_ATTR(mnt_pocket_reform_panel_dcs_read, 0644, panel_dcs_read_attr_show, panel_dcs_read_attr_store);
// static struct attribute *mnt_pocket_reform_panel_attrs[] = {
// 	&dev_attr_mnt_pocket_reform_panel_version.attr,
// 	&dev_attr_mnt_pocket_reform_panel_dcs_read.attr,
// 	NULL
// };
// static const struct attribute_group mnt_pocket_reform_panel_attr_group = {
// 	.attrs = mnt_pocket_reform_panel_attrs,
// };

// static int __global_mnt_pocket_reform_panel_version = 0;
// int mnt_pocket_reform_get_panel_version(void)
// {
// 	return __global_mnt_pocket_reform_panel_version;
// }
// EXPORT_SYMBOL_GPL(mnt_pocket_reform_get_panel_version);

// static inline struct jdi_panel *to_jdi_panel(struct drm_panel *panel)
// {
// 	return container_of(panel, struct jdi_panel, base);
// }

// static int dsi_dcs_bl_get_brightness(struct backlight_device *bl)
// {
// 	u16 brightness = bl->props.brightness;
// 	return (int)brightness;
// }

// static int dsi_dcs_bl_update_status(struct backlight_device *bl)
// {
// 	struct jdi_panel *jdi = bl_get_data(bl);
// 	struct mipi_dsi_device *dsi = jdi->dsi;

// 	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

// 	mipi_dsi_dcs_set_display_brightness(dsi, bl->props.brightness);

// 	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

// 	return 0;
// }

// static const struct backlight_ops dsi_bl_ops = {
// 	.update_status = dsi_dcs_bl_update_status,
// 	.get_brightness = dsi_dcs_bl_get_brightness,
// };

// static struct backlight_device *
// drm_panel_create_dsi_backlight(struct jdi_panel *jdi)
// {
// 	struct device *dev = &jdi->dsi->dev;
// 	struct backlight_properties props;

// 	memset(&props, 0, sizeof(props));
// 	props.type = BACKLIGHT_RAW;
// 	props.brightness = 255;
// 	props.max_brightness = 255;

// 	return devm_backlight_device_register(dev, dev_name(dev), dev, jdi,
// 					      &dsi_bl_ops, &props);
// }

static ssize_t dsi_write_raw(struct mipi_dsi_device *dsi,
                             const void *data, size_t len)
{
	const struct mipi_dsi_msg msg = {
		.channel = dsi->channel,
		.tx_buf = data,
		.tx_len = len,
		.type = MIPI_DSI_DCS_LONG_WRITE,
		.flags = MIPI_DSI_MSG_USE_LPM
	};

	return dsi->host->ops->transfer(dsi->host, &msg);
}

static void pocket_display_v2_init(struct jdi_panel* jdi) {
	struct mipi_dsi_device *dsi = jdi->dsi;
	struct device *dev = &jdi->dsi->dev;
	int ret;

	mipi_dsi_dcs_soft_reset(dsi);
	// msleep(20);
    mdelay(20);

    // TODO: This doesn't get called multiple times so 
    // send the init sequence immediately
    jdi->display_v2_init_count++;

	// for unknown reasons, if we send all the data tables
	// on the first init, it takes very long (~14 seconds),
	// but it's quick the second time around.
	if (jdi->display_v2_init_count > 0) {
		dsi_write_raw(dsi, (u8[]) {0xE0,0xAB,0xBA}, 3);
		dsi_write_raw(dsi, (u8[]) {0xFF,0x01}, 2);
		dsi_write_raw(dsi, (u8[]) {0x83,0x16}, 2);
		dsi_write_raw(dsi, (u8[]) {0x90,0x81}, 2);

		dsi_write_raw(dsi, (u8[]) {0xFF,0x02}, 2);
		dsi_write_raw(dsi, (u8[]) {0x80,0xA0}, 2); // vcom
		dsi_write_raw(dsi, (u8[]) {0x81,0x50}, 2);
		dsi_write_raw(dsi, (u8[]) {0x82,0x50}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA2,0xB0}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA3,0x49}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA4,0x6B}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA5,0x34}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA6,0x22}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA7,0x34}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA8,0x22}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA9,0x51}, 2);
		dsi_write_raw(dsi, (u8[]) {0xAA,0xD4}, 2);
		dsi_write_raw(dsi, (u8[]) {0xAB,0xA6}, 2);
		dsi_write_raw(dsi, (u8[]) {0xAC,0x05}, 2);
		dsi_write_raw(dsi, (u8[]) {0xAD,0xAB}, 2);
		dsi_write_raw(dsi, (u8[]) {0xAE,0x3B}, 2);
		dsi_write_raw(dsi, (u8[]) {0xAF,0x1B}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB0,0x1E}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB1,0xAB}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB2,0xC1}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB3,0x22}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB4,0xFF}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB5,0xCC}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB6,0x23}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB7,0x01}, 2);
		dsi_write_raw(dsi, (u8[]) {0xBF,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC0,0x0E}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC1,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC2,0xFF}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC3,0x08}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC5,0xFF}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC7,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC8,0x05}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC9,0x12}, 2);
		dsi_write_raw(dsi, (u8[]) {0xCA,0x29}, 2);
		dsi_write_raw(dsi, (u8[]) {0xCB,0x41}, 2);
		dsi_write_raw(dsi, (u8[]) {0xCE,0x06}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD1,0x7F}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD2,0x6A}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD3,0x63}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD4,0x63}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD5,0x61}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD6,0x61}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD7,0x5D}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD8,0x5B}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD9,0x55}, 2);
		dsi_write_raw(dsi, (u8[]) {0xDA,0x4D}, 2);
		dsi_write_raw(dsi, (u8[]) {0xDB,0x4B}, 2);
		dsi_write_raw(dsi, (u8[]) {0xDC,0x49}, 2);
		dsi_write_raw(dsi, (u8[]) {0xDD,0x47}, 2);
		dsi_write_raw(dsi, (u8[]) {0xDE,0x43}, 2);
		dsi_write_raw(dsi, (u8[]) {0xDF,0x27}, 2);
		dsi_write_raw(dsi, (u8[]) {0xE0,0x14}, 2);
		dsi_write_raw(dsi, (u8[]) {0xE1,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xE2,0x7F}, 2);
		dsi_write_raw(dsi, (u8[]) {0xE3,0x6A}, 2);
		dsi_write_raw(dsi, (u8[]) {0xE4,0x63}, 2);
		dsi_write_raw(dsi, (u8[]) {0xE5,0x63}, 2);
		dsi_write_raw(dsi, (u8[]) {0xE6,0x61}, 2);
		dsi_write_raw(dsi, (u8[]) {0xE7,0x61}, 2);
		dsi_write_raw(dsi, (u8[]) {0xE8,0x5D}, 2);
		dsi_write_raw(dsi, (u8[]) {0xE9,0x5B}, 2);
		dsi_write_raw(dsi, (u8[]) {0xEA,0x55}, 2);
		dsi_write_raw(dsi, (u8[]) {0xEB,0x4D}, 2);
		dsi_write_raw(dsi, (u8[]) {0xEC,0x4B}, 2);
		dsi_write_raw(dsi, (u8[]) {0xED,0x49}, 2);
		dsi_write_raw(dsi, (u8[]) {0xEE,0x47}, 2);
		dsi_write_raw(dsi, (u8[]) {0xEF,0x43}, 2);
		dsi_write_raw(dsi, (u8[]) {0xF0,0x27}, 2);
		dsi_write_raw(dsi, (u8[]) {0xF1,0x14}, 2);
		dsi_write_raw(dsi, (u8[]) {0xF2,0x00}, 2);

		dsi_write_raw(dsi, (u8[]) {0xFF,0x03}, 2);
		dsi_write_raw(dsi, (u8[]) {0x9D,0x00}, 2);

		dsi_write_raw(dsi, (u8[]) {0xFF,0x04}, 2);
		dsi_write_raw(dsi, (u8[]) {0x80,0xCD}, 2);
		dsi_write_raw(dsi, (u8[]) {0x81,0xCB}, 2);
		dsi_write_raw(dsi, (u8[]) {0x82,0xA9}, 2);
		dsi_write_raw(dsi, (u8[]) {0x83,0x8C}, 2);
		dsi_write_raw(dsi, (u8[]) {0x84,0x46}, 2);
		dsi_write_raw(dsi, (u8[]) {0x85,0x66}, 2);
		dsi_write_raw(dsi, (u8[]) {0x86,0x66}, 2);
		dsi_write_raw(dsi, (u8[]) {0x87,0x64}, 2);
		dsi_write_raw(dsi, (u8[]) {0x88,0xA0}, 2);
		dsi_write_raw(dsi, (u8[]) {0x89,0x08}, 2);
		dsi_write_raw(dsi, (u8[]) {0x8A,0xA0}, 2);
		dsi_write_raw(dsi, (u8[]) {0x8B,0x08}, 2);
		dsi_write_raw(dsi, (u8[]) {0x8C,0x02}, 2);
		dsi_write_raw(dsi, (u8[]) {0x8D,0x81}, 2);
		dsi_write_raw(dsi, (u8[]) {0x8E,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0x8F,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0x90,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0x91,0x05}, 2);
		dsi_write_raw(dsi, (u8[]) {0x92,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0x93,0x0F}, 2);
		dsi_write_raw(dsi, (u8[]) {0x94,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0x95,0x08}, 2);
		dsi_write_raw(dsi, (u8[]) {0x96,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0x97,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0x98,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0x99,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0x9A,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0x9B,0x01}, 2);
		dsi_write_raw(dsi, (u8[]) {0x9C,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0x9D,0x74}, 2);
		dsi_write_raw(dsi, (u8[]) {0x9E,0x12}, 2);
		dsi_write_raw(dsi, (u8[]) {0x9F,0xB2}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA0,0x07}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA1,0xA0}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA2,0x08}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA3,0x60}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA4,0x08}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA5,0x02}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA6,0x54}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA7,0x80}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA8,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA9,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xAA,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xAB,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xAC,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xAD,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xAE,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xAF,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB0,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB1,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB2,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB3,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB4,0x09}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB5,0x01}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB6,0x13}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB7,0x08}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB8,0x12}, 2);
		dsi_write_raw(dsi, (u8[]) {0xB9,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xBA,0x03}, 2);
		dsi_write_raw(dsi, (u8[]) {0xBB,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xBC,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xBD,0x01}, 2);
		dsi_write_raw(dsi, (u8[]) {0xBE,0x70}, 2);
		dsi_write_raw(dsi, (u8[]) {0xBF,0xBA}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC0,0x98}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC1,0x76}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC2,0x54}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC3,0x32}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC4,0x10}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC5,0xCD}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC6,0xEF}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC7,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC8,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xC9,0x20}, 2);
		dsi_write_raw(dsi, (u8[]) {0xCB,0x21}, 2);
		dsi_write_raw(dsi, (u8[]) {0xCD,0x0B}, 2);
		dsi_write_raw(dsi, (u8[]) {0xCF,0x10}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD0,0x12}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD1,0x14}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD2,0x16}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD3,0x18}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD4,0x1A}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD5,0x05}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD6,0x07}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD7,0x09}, 2);
		dsi_write_raw(dsi, (u8[]) {0xD9,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xDA,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xDB,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xDC,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xDD,0x20}, 2);
		dsi_write_raw(dsi, (u8[]) {0xDF,0x21}, 2);
		dsi_write_raw(dsi, (u8[]) {0xE3,0x0B}, 2);
		dsi_write_raw(dsi, (u8[]) {0xE5,0x11}, 2);
		dsi_write_raw(dsi, (u8[]) {0xE6,0x13}, 2);
		dsi_write_raw(dsi, (u8[]) {0xE7,0x15}, 2);
		dsi_write_raw(dsi, (u8[]) {0xE8,0x17}, 2);
		dsi_write_raw(dsi, (u8[]) {0xE9,0x19}, 2);
		dsi_write_raw(dsi, (u8[]) {0xEA,0x1B}, 2);
		dsi_write_raw(dsi, (u8[]) {0xEB,0x06}, 2);
		dsi_write_raw(dsi, (u8[]) {0xEC,0x08}, 2);
		dsi_write_raw(dsi, (u8[]) {0xED,0x0A}, 2);
		dsi_write_raw(dsi, (u8[]) {0xEF,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xF0,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xF1,0xA2}, 2);
		dsi_write_raw(dsi, (u8[]) {0xF2,0x18}, 2);
		dsi_write_raw(dsi, (u8[]) {0xF3,0x11}, 2);
		dsi_write_raw(dsi, (u8[]) {0xF4,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xF5,0x11}, 2);
		dsi_write_raw(dsi, (u8[]) {0xF6,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xF7,0x11}, 2);
		dsi_write_raw(dsi, (u8[]) {0xF8,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xF9,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xFA,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xFB,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xFC,0x20}, 2);
		dsi_write_raw(dsi, (u8[]) {0xFD,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0xFE,0x00}, 2);

		dsi_write_raw(dsi, (u8[]) {0xFF,0x05}, 2);
		dsi_write_raw(dsi, (u8[]) {0x8F,0xE8}, 2);
		dsi_write_raw(dsi, (u8[]) {0x90,0x84}, 2);
		dsi_write_raw(dsi, (u8[]) {0x91,0x77}, 2);

		dsi_write_raw(dsi, (u8[]) {0xFF,0x06}, 2);
		dsi_write_raw(dsi, (u8[]) {0x80,0x10}, 2);
		dsi_write_raw(dsi, (u8[]) {0x81,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0x82,0x07}, 2);
		dsi_write_raw(dsi, (u8[]) {0x83,0x9F}, 2);

		dsi_write_raw(dsi, (u8[]) {0xFF,0x07}, 2);
		dsi_write_raw(dsi, (u8[]) {0x80,0x14}, 2);
		dsi_write_raw(dsi, (u8[]) {0x81,0x12}, 2);
		dsi_write_raw(dsi, (u8[]) {0x82,0x02}, 2);
		dsi_write_raw(dsi, (u8[]) {0x83,0x20}, 2);
		dsi_write_raw(dsi, (u8[]) {0x84,0x24}, 2);
		dsi_write_raw(dsi, (u8[]) {0x85,0x0C}, 2);
		dsi_write_raw(dsi, (u8[]) {0x86,0x39}, 2);
		dsi_write_raw(dsi, (u8[]) {0x87,0x77}, 2);
		dsi_write_raw(dsi, (u8[]) {0x88,0x80}, 2);
		dsi_write_raw(dsi, (u8[]) {0x89,0x33}, 2);
		dsi_write_raw(dsi, (u8[]) {0x8A,0x00}, 2);
		dsi_write_raw(dsi, (u8[]) {0x8B,0x04}, 2);
		dsi_write_raw(dsi, (u8[]) {0x8C,0x09}, 2);
		dsi_write_raw(dsi, (u8[]) {0x8D,0x40}, 2);
		dsi_write_raw(dsi, (u8[]) {0x8E,0x40}, 2);
		dsi_write_raw(dsi, (u8[]) {0x91,0x11}, 2);
		dsi_write_raw(dsi, (u8[]) {0x92,0x2C}, 2);
		dsi_write_raw(dsi, (u8[]) {0x93,0x2D}, 2);
		dsi_write_raw(dsi, (u8[]) {0x94,0x03}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA0,0x0A}, 2);
		dsi_write_raw(dsi, (u8[]) {0xA2,0x13}, 2);

		dsi_write_raw(dsi, (u8[]) {0xFF,0x00}, 2);
		dev_info(dev, "[display v2] sent tables OK\n");
	} else {
		dev_info(dev, "[display v2] not sending tables this time\n");
	}

	jdi->display_v2_init_count++;

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "[display v2] failed to set exit sleep mode: %d\n", ret);
	}
	mdelay(100);
	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "[display v2] failed to set display on: %d\n", ret);
	}
	dev_info(dev, "[display v2] OK\n");
}

// static void pocket_display_v1_init(struct jdi_panel* jdi) {
// 	struct mipi_dsi_device *dsi = jdi->dsi;
// 	struct device *dev = &jdi->dsi->dev;
// 	int ret;

// 	ret = mipi_dsi_dcs_write(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, (u8[]) {0x2c}, 1);
// 	ret = mipi_dsi_dcs_set_pixel_format(dsi, MIPI_DCS_PIXEL_FMT_24BIT << 4);
// 	if (ret < 0) {
// 		dev_err(dev, "[display v1] failed to set pixel format: %d\n", ret);
// 	}

// 	// write_memory_start
// 	ret = mipi_dsi_generic_write(dsi, (u8[]) {0x2c}, 1);
// 	ret = mipi_dsi_generic_write(dsi, (u8[]) {0x00}, 0);

// 	msleep(200);
// 	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
// 	if (ret < 0) {
// 		dev_err(dev, "[display v1] failed to set exit sleep mode: %d\n", ret);
// 	}

// 	// required delay
// 	msleep(800);

// 	// MCAP off
// 	ret = mipi_dsi_generic_write(dsi, (u8[]){0xB0, 0x00}, 2);
// 	if (ret < 0) {
// 		dev_err(dev, "[display v1] failed to set mcap: %d\n", ret);
// 	}

// 	// required delay
// 	mdelay(200);

// 	// Interface setting, video mode
// 	ret = mipi_dsi_generic_write(dsi, (u8[])
// 				     {0xB3, 0x14, 0x08, 0x00, 0x22, 0x00}, 6);
// 	if (ret < 0) {
// 		dev_err(dev, "[display v1] failed to set display interface setting: %d\n"
// 			, ret);
// 	}

// 	// interface ID setting
// 	mdelay(20);
// 	ret = mipi_dsi_generic_write(dsi, (u8[]) {0xb4, 0x0c}, 2);

// 	// DSI control
// 	ret = mipi_dsi_generic_write(dsi, (u8[]) {0xb6, 0x3a, 0xd3}, 3);
// 	ret = mipi_dsi_dcs_set_display_on(dsi);

// 	mipi_dsi_dcs_set_display_brightness(dsi, 127);
// }

static int jdi_panel_init(struct jdi_panel *jdi)
{
	struct mipi_dsi_device *dsi = jdi->dsi;
	struct device *dev = &jdi->dsi->dev;
	int ret;
	u8 readval = 0;

	if (of_property_present(dsi->dev.of_node, "probe-mode-lpm")) {
		// imx8mplus, a311d
		dsi->mode_flags |= MIPI_DSI_MODE_LPM;
	} else {
		// rk3588
		dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;
	}

	// msleep(20);
    mdelay(20);

	if (!jdi->panel_version) {
		mipi_dsi_dcs_read(dsi, 12, &readval, sizeof(readval));
		dev_err(dev, "[mnt pocket reform display] read register 12a: 0x%x\n", (int)readval);

		if (readval == 0x77 || readval == 0x70) {
			jdi->panel_version = 1;
            pr_err("%s got v1\n", __func__);
			// __global_mnt_pocket_reform_panel_version = 1;
		} else {
			jdi->panel_version = 2;
            pr_err("%s got v2\n", __func__);
			// __global_mnt_pocket_reform_panel_version = 2;
		}
	}
    jdi->panel_version = 2;
    // __global_mnt_pocket_reform_panel_version = 2;

	if (jdi->panel_version == 2) {
		dsi->mode_flags |= MIPI_DSI_MODE_LPM;
        // dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_BURST;
		// panel v2 doesn't like HSE
		dsi->mode_flags &= ~MIPI_DSI_MODE_VIDEO_HSE;
		// dsi->mode_flags &= ~(MIPI_DSI_MODE_NO_EOT_PACKET | MIPI_DSI_MODE_VIDEO_NO_HFP | MIPI_DSI_MODE_VIDEO_NO_HBP | MIPI_DSI_MODE_VIDEO_NO_HSA);
		dsi->mode_flags &= ~(MIPI_DSI_MODE_NO_EOT_PACKET | MIPI_DSI_MODE_VIDEO_HFP | MIPI_DSI_MODE_VIDEO_HBP | MIPI_DSI_MODE_VIDEO_HSA);
        pr_err("%s mode flags: %lu\n", __func__, dsi->mode_flags);

		pocket_display_v2_init(jdi);
	} else {
		// pocket_display_v1_init(jdi);
		// if (!jdi->backlight) {
		// 	jdi->backlight = drm_panel_create_dsi_backlight(jdi);
		// 	if (IS_ERR(jdi->backlight))
		// 		return dev_err_probe(dev, PTR_ERR(jdi->backlight),
		// 				     "failed to register backlight %d\n", ret);
		// 	backlight_enable(jdi->backlight);
		// }
	}

	return 0;
}

static int jdi_panel_disable(struct jdi_panel *jdi)
{
	// struct jdi_panel *jdi = to_jdi_panel(panel);

	if (!jdi->enabled) return 0;

	jdi->enabled = false;
	mipi_dsi_dcs_set_display_off(jdi->dsi);
	if (jdi->panel_version == 2) return 0;
	
	mipi_dsi_dcs_enter_sleep_mode(jdi->dsi);
	// msleep(100);
    mdelay(100);
	// backlight_disable(jdi->backlight);

	return 0;
}

static int jdi_panel_unprepare(struct jdi_panel *jdi)
{
	// struct jdi_panel *jdi = to_jdi_panel(panel);
	struct device *dev = &jdi->dsi->dev;
	int ret;

	if (!jdi->prepared)
		return 0;

	ret = regulator_bulk_disable(ARRAY_SIZE(jdi->supplies), jdi->supplies);
	if (ret < 0)
		dev_err(dev, "regulator disable failed, %d\n", ret);

	if (!IS_ERR(jdi->enable_gpio)) gpiod_set_value(jdi->enable_gpio, 0);

	if (!IS_ERR(jdi->reset_gpio)) gpiod_set_value(jdi->reset_gpio, 1);

	if (!IS_ERR(jdi->dcdc_en_gpio)) gpiod_set_value(jdi->dcdc_en_gpio, 0);

	jdi->prepared = false;

	return 0;
}

static int jdi_panel_prepare(struct jdi_panel *jdi)
{
	// struct jdi_panel *jdi = to_jdi_panel(panel);
	struct device *dev = &jdi->dsi->dev;
	int ret;

	if (!of_property_present(dev->of_node, "init-in-enable")) {
		if (jdi->prepared)
			return 0;
	}

	ret = regulator_bulk_enable(ARRAY_SIZE(jdi->supplies), jdi->supplies);
	if (ret < 0) {
		dev_err(dev, "regulator enable failed, %d\n", ret);
		return ret;
	}

	// msleep(20);
    mdelay(20);

	if (!IS_ERR(jdi->dcdc_en_gpio)) gpiod_set_value(jdi->dcdc_en_gpio, 1);
	// usleep_range(10, 20);
    udelay(10);

	if (!IS_ERR(jdi->reset_gpio)) gpiod_set_value(jdi->reset_gpio, 0);
	// usleep_range(10, 20);
    udelay(10);

	if (!IS_ERR(jdi->enable_gpio)) gpiod_set_value(jdi->enable_gpio, 1);
	// usleep_range(10, 20);
    udelay(10);

	if (!of_property_present(dev->of_node, "init-in-enable")) {
		// on a311d the panels only work
		// if init is done in here, not in enable
		dev_info(dev, "[display] init in prepare...\n");
		ret = jdi_panel_init(jdi);
		if (ret < 0) {
			dev_err(dev, "failed jdi_panel_init: %d\n", ret);
			return ret;
		}
	}

	jdi->prepared = true;

	return 0;
}

static int jdi_panel_enable(struct jdi_panel *jdi)
{
	// struct jdi_panel *jdi = to_jdi_panel(panel);
	struct device *dev = &jdi->dsi->dev;
	int ret;

	if (of_property_present(dev->of_node, "init-in-enable")) {
		// on imx8mp the panels only work
		// if init is done here, not in prepare
		dev_info(dev, "[display] init in enable...\n");
		ret = jdi_panel_init(jdi);
		if (ret < 0) {
			dev_err(dev, "failed jdi_panel_init: %d\n", ret);
			return ret;
		}
	} else {
		jdi->dsi->mode_flags |= MIPI_DSI_MODE_LPM;
		ret = mipi_dsi_dcs_set_display_on(jdi->dsi);
		if (ret < 0) {
			pr_err("%s error setting display on: %pe\n", __func__, ERR_PTR(ret));
		}

	}
	jdi->enabled = true;

	return 0;
}

static const struct drm_display_mode panel_v1_mode = {
	.clock = 140000,
	.hdisplay = 1200,
	.hsync_start = 1200 + 48,
	.hsync_end = 1200 + 48 + 32,
	.htotal = 1200 + 48 + 32 + 60,
	.vdisplay = 1920,
	.vsync_start = 1920 + 3,
	.vsync_end = 1920 + 3 + 5,
	.vtotal = 1920 + 3 + 5 + 6,
	.flags = 0,
};

static const struct drm_display_mode panel_v2_mode = {
	.clock = 140000,
	.hdisplay = 1200,
	.hsync_start = 1200 + 40,
	.hsync_end = 1200 + 40 + 20,
	.htotal = 1200 + 40 + 20 + 40,
	.vdisplay = 1920,
	.vsync_start = 1920 + 18,
	.vsync_end = 1920 + 18 + 2,
	.vtotal = 1920 + 18 + 2 + 20,
	.flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
};

// static int jdi_panel_get_modes(struct jdi_panel *jdi,
// 			       struct drm_connector *connector)
static int jdi_panel_get_modes(struct jdi_panel *jdi,
			     struct display_timings *timings)
{
    pr_err("%s start\n", __func__);
	// struct drm_display_mode *mode;
	// struct jdi_panel *jdi = to_jdi_panel(panel);
	struct device *dev = &jdi->dsi->dev;
	struct fb_videomode *mode;

	mode = xzalloc(sizeof(*mode));

	drm_display_mode_to_fb_videomode(&panel_v2_mode, mode);

	timings->modes = mode;
	timings->num_modes = 1;

	return 0;

	// if (jdi->panel_version == 2) {
	// 	mode = drm_mode_duplicate(connector->dev, &panel_v2_mode);
	// } else {
	// 	mode = drm_mode_duplicate(connector->dev, &panel_v1_mode);

	// 	if (of_property_present(dev->of_node, "init-in-enable")) {
	// 		// a hack to force display pipeline reinit on
	// 		// imx8mp, otherwise the version probing read to dcs register 12
	// 		// causes wrapping and color shift of display v1
	// 		if (jdi->display_v1_init_count == 0) {
	// 			mode->clock = 145000;
	// 			jdi->display_v1_init_count++;
	// 		}
	// 	}
	// }

	// if (!mode) {
	// 	dev_err(dev, "failed to add mode\n");
	// 	return -ENOMEM;
	// }

	// // on A311D, we shift the vsync by one line to counteract VIU_OSD_HOLD_FIFO_LINES
	// if (of_property_present(dev->of_node, "vsync-shift")) {
	// 	uint32_t vsync_shift = 0;
	// 	of_property_read_u32(dev->of_node, "vsync-shift", &vsync_shift);
	// 	dev_warn(dev, "vsync-shift from device tree: %d\n", vsync_shift);
	// 	mode->vsync_start += vsync_shift;
	// 	mode->vsync_end += vsync_shift;
	// }

	// drm_mode_set_name(mode);

	// drm_mode_probed_add(connector, mode);

	// connector->display_info.width_mm = 95;
	// connector->display_info.height_mm = 151;

	// return 1;
}

static int jdi_panel_get_display_info(struct jdi_panel *jdi,
			     struct drm_display_info *display_info)
{
    // TODO: Not sure what the bus flags should be here
	display_info->bus_flags = 0;
	display_info->panel_orientation = DRM_MODE_PANEL_ORIENTATION_LEFT_UP;
    display_info->width_mm = 95;
    display_info->height_mm = 151;

	return 0;
}

// static const struct drm_panel_funcs jdi_panel_funcs = {
// 	.disable = jdi_panel_disable,
// 	.unprepare = jdi_panel_unprepare,
// 	.prepare = jdi_panel_prepare,
// 	.enable = jdi_panel_enable,
// 	.get_modes = jdi_panel_get_modes,
// };

static const struct of_device_id jdi_of_match[] = {
	{ .compatible = "jdi,lt070me05000"},
	{ }
};
MODULE_DEVICE_TABLE(of, jdi_of_match);

static int jdi_panel_add(struct jdi_panel *jdi)
{
	struct device *dev = &jdi->dsi->dev;
	int ret;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(jdi->supplies); i++)
		jdi->supplies[i].supply = regulator_names[i];

	// ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(jdi->supplies),
	// 			      jdi->supplies);
	ret = regulator_bulk_get(dev, ARRAY_SIZE(jdi->supplies),
				      jdi->supplies);
	if (ret < 0)
		return dev_err_probe(dev, ret,
				     "failed to init regulator, ret=%d\n", ret);

	jdi->enable_gpio = gpiod_get(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(jdi->enable_gpio))
		dev_err_probe(dev, PTR_ERR(jdi->enable_gpio),
			      "cannot get enable-gpio %d\n", ret);

	jdi->reset_gpio = gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(jdi->reset_gpio))
		dev_err_probe(dev, PTR_ERR(jdi->reset_gpio),
			      "cannot get reset-gpios %d\n", ret);

	jdi->dcdc_en_gpio = gpiod_get(dev, "dcdc-en", GPIOD_OUT_LOW);
	if (IS_ERR(jdi->dcdc_en_gpio))
		dev_err_probe(dev, PTR_ERR(jdi->dcdc_en_gpio),
			      "cannot get dcdc-en-gpio %d\n", ret);

	// drm_panel_init(&jdi->base, &jdi->dsi->dev, &jdi_panel_funcs,
	// 	       DRM_MODE_CONNECTOR_DSI);

	// drm_panel_add(&jdi->base);

	return 0;
}

// static void jdi_panel_del(struct jdi_panel *jdi)
// {
// 	if (jdi->base.dev)
// 		drm_panel_remove(&jdi->base);
// }

static int jdi_ioctl(struct vpl *vpl, unsigned int port,
			 unsigned int cmd, void *ptr)
{
	struct jdi_panel *ctx = container_of(vpl, struct jdi_panel, vpl);

	switch (cmd) {
	case VPL_PREPARE:
        pr_err("%s VPL_PREPARE\n", __func__);
		return jdi_panel_prepare(ctx);
	case VPL_ENABLE:
        pr_err("%s VPL_ENABLE\n", __func__);
		return jdi_panel_enable(ctx);
	case VPL_DISABLE:
        pr_err("%s VPL_DISABLE\n", __func__);
		return jdi_panel_disable(ctx);
	case VPL_UNPREPARE:
        pr_err("%s VPL_UNPREPARE\n", __func__);
		return jdi_panel_unprepare(ctx);
	case VPL_GET_VIDEOMODES:
        pr_err("%s VPL_GET_VIDEOMODES\n", __func__);
		return jdi_panel_get_modes(ctx, ptr);
	case VPL_GET_DISPLAY_INFO:
        pr_err("%s VPL_GET_DISPLAY_INFO\n", __func__);
		return jdi_panel_get_display_info(ctx, ptr);
	case VPL_GET_BUS_FORMAT:
        pr_err("%s VPL_GET_BUS_FORMAT\n", __func__);
		*(u32 *)ptr = MEDIA_BUS_FMT_RGB888_1X24;
		return 0;
	default:
		return 0;
	}
}

static int jdi_panel_probe(struct mipi_dsi_device *dsi)
{
    pr_err("%s start\n", __func__);
	struct device *dev = &dsi->dev;
	struct jdi_panel *jdi;
	int ret;
	int err;

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_HSE;

	// // on a311d it works only without burst, but imx8mplus needs burst mode
	// if (of_property_present(dsi->dev.of_node, "burst-mode")) {
	// 	dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_BURST;
	// 	dev_warn(&dsi->dev, "DSI burst mode enabled via device tree\n");
	// }
	if (of_property_present(dsi->dev.of_node, "no-eot-hfp-hbp-hsa")) {
		dsi->mode_flags |= MIPI_DSI_MODE_NO_EOT_PACKET | MIPI_DSI_MODE_VIDEO_HFP | MIPI_DSI_MODE_VIDEO_HBP | MIPI_DSI_MODE_VIDEO_HSA;
		dev_warn(&dsi->dev, "DSI eot/hfp/hbp/hsa disabled via device tree\n");
	}

	jdi = devm_kzalloc(&dsi->dev, sizeof(*jdi), GFP_KERNEL);
	if (!jdi)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, jdi);

    jdi->dev = dev;
	jdi->dsi = dsi;

	jdi->vpl.node = dev->of_node;
	jdi->vpl.ioctl = jdi_ioctl;

	ret = vpl_register(&jdi->vpl);
	if (ret) {
        pr_err("%s register failed\n", __func__);
		return ret;
    }

    pr_err("%s about to jdi_panel_add\n", __func__);
	ret = jdi_panel_add(jdi);
	if (ret < 0)
		return ret;

    pr_err("%s about to mipi_dsi_attach\n", __func__);
	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		// jdi_panel_del(jdi);
		return ret;
	}

	// err = sysfs_create_group(&dsi->dev.kobj, &mnt_pocket_reform_panel_attr_group);
	// if (err) {
	// 	pr_err("failed to register pocket reform panel attr group\n");
	// }


	return 0;
}

// static void jdi_panel_remove(struct mipi_dsi_device *dsi)
// {
// 	struct jdi_panel *jdi = mipi_dsi_get_drvdata(dsi);
// 	int ret;

// 	ret = jdi_panel_disable(&jdi->base);
// 	if (ret < 0)
// 		dev_err(&dsi->dev, "failed to disable panel: %d\n", ret);

// 	ret = mipi_dsi_detach(dsi);
// 	if (ret < 0)
// 		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n",
// 			ret);

// 	jdi_panel_del(jdi);
// }

// static void jdi_panel_shutdown(struct mipi_dsi_device *dsi)
// {
// 	struct jdi_panel *jdi = mipi_dsi_get_drvdata(dsi);

// 	jdi_panel_disable(&jdi->base);
// }

static struct mipi_dsi_driver jdi_panel_driver = {
	.driver = {
		.name = "panel-mnt-pocket-reform",
		.of_match_table = jdi_of_match,
	},
	.probe = jdi_panel_probe,
	// .remove = jdi_panel_remove,
	// .shutdown = jdi_panel_shutdown,
};
module_mipi_dsi_driver(jdi_panel_driver);


MODULE_AUTHOR("Sumit Semwal <sumit.semwal@linaro.org>");
MODULE_AUTHOR("Vinay Simha BN <simhavcs@gmail.com>");
MODULE_AUTHOR("Lukas Hartmann <lukas@mntre.com>");
MODULE_DESCRIPTION("MNT Pocket Reform Display WUXGA");
MODULE_LICENSE("GPL v2");
