// SPDX-License-Identifier: GPL-2.0-only
#include <bootsource.h>
#include <common.h>
#include <deep-probe.h>
#include <init.h>
#include <mach/rockchip/bbu.h>
#include <gpio.h>

struct reform2_model {
	const char *name;
	const char *shortname;
};

/* GPIO GPIO4_B3 = E11 = (4*32 + 8 + 3) = 139 */
#define USB_HUB_RESET 139

static int reform2_rk3588_probe(struct device *dev)
{
	enum bootsource bootsource = bootsource_get();
	int instance = bootsource_get_instance();
	const struct reform2_model *model;
	int bbu_flags_emmc = 0;
	int bbu_flags_sd = 0;

	model = device_get_match_data(dev);

	if (bootsource == BOOTSOURCE_MMC && instance == 1) {
		of_device_enable_path("/chosen/environment-sd");
		bbu_flags_sd |= BBU_HANDLER_FLAG_DEFAULT;
	} else {
		of_device_enable_path("/chosen/environment-emmc");
		bbu_flags_emmc |= BBU_HANDLER_FLAG_DEFAULT;
	}

	rockchip_bbu_mmc_register("emmc", bbu_flags_emmc, "/dev/mmc0");
	rockchip_bbu_mmc_register("sd", bbu_flags_sd, "/dev/mmc1");

	printf("[mnt-reform-series-rk3588] setup_usb()\n");
	gpio_request(USB_HUB_RESET, "usb_hub_rst");
	gpio_direction_output(USB_HUB_RESET, 0);
	mdelay(10);
	gpio_set_value(USB_HUB_RESET, 1);

	return 0;
}

static const struct reform2_model reform2_rk3588 = {
	.name = "MNT Reform 2 with RCORE-RK3588 Module",
	.shortname = "reform2_rk3588",
};

static const struct reform2_model reform2_dsi_rk3588 = {
	.name = "MNT Reform 2 with RCORE-DSI RK3588 Module",
	.shortname = "reform2_dsi_rk3588",
};

static const struct reform2_model pocket_reform_rk3588 = {
	.name = "MNT Pocket Reform with RCORE RK3588 Module",
	.shortname = "pocket_reform_rk3588",
};

static const struct reform2_model reform_next_rk3588 = {
	.name = "MNT Reform Next with RCORE RK3588 Module",
	.shortname = "reform_next_rk3588",
};

static const struct of_device_id reform2_rk3588_of_match[] = {
	{
		.compatible = "mntre,reform2-rcore",
		.data = &reform2_rk3588,
	},
	{
		.compatible = "mntre,reform2-rcore-dsi",
		.data = &reform2_dsi_rk3588,
	},
	{
		.compatible = "mntre,pocket-reform-rcore",
		.data = &pocket_reform_rk3588,
	},
	{
		.compatible = "mntre,reform-next-rcore",
		.data = &reform_next_rk3588,
	},
	{ /* sentinel */ },
};

static struct driver reform2_rk3588_board_driver = {
	.name = "board-reform2-rk3588",
	.probe = reform2_rk3588_probe,
	.of_compatible = reform2_rk3588_of_match,
};
coredevice_platform_driver(reform2_rk3588_board_driver);

BAREBOX_DEEP_PROBE_ENABLE(reform2_rk3588_of_match);
