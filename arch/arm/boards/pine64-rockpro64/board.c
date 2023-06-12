// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <init.h>
#include <mach/rockchip/bbu.h>
#include <bootsource.h>
#include <environment.h>
#include <deep-probe.h>

static int rockpro64_probe(struct device *dev)
{
	enum bootsource bootsource = bootsource_get();
	int instance = bootsource_get_instance();
	int mmc_bbu_flags = 0, sd_bbu_flags = 0;

	if (bootsource == BOOTSOURCE_MMC && instance == 1) {
		of_device_enable_path("/chosen/environment-sd");
		sd_bbu_flags = BBU_HANDLER_FLAG_DEFAULT;
	} else {
		of_device_enable_path("/chosen/environment-emmc");
		mmc_bbu_flags = BBU_HANDLER_FLAG_DEFAULT;
	}

	rockchip_bbu_mmc_register("sd", sd_bbu_flags, "/dev/mmc1");
	rockchip_bbu_mmc_register("emmc", mmc_bbu_flags, "/dev/mmc0");

	return 0;
}

static const struct of_device_id rockpro64_of_match[] = {
	{ .compatible = "pine64,rockpro64" },
	{ /* Sentinel */},
};
BAREBOX_DEEP_PROBE_ENABLE(rockpro64_of_match);

static struct driver rockpro64_board_driver = {
	.name = "board-rockpro64",
	.probe = rockpro64_probe,
	.of_compatible = rockpro64_of_match,
};
coredevice_platform_driver(rockpro64_board_driver);
