// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <init.h>
#include <mach/rockchip/bbu.h>
#include <bootsource.h>
#include <environment.h>
#include <deep-probe.h>

static int rockpi_n10_probe(struct device *dev)
{
	enum bootsource bootsource = bootsource_get();
	int instance = bootsource_get_instance();

	if (bootsource == BOOTSOURCE_MMC && instance == 1)
		of_device_enable_path("/chosen/environment-sd");
	else
		of_device_enable_path("/chosen/environment-emmc");

	rockchip_bbu_mmc_register("emmc", BBU_HANDLER_FLAG_DEFAULT, "/dev/mmc0");
	rockchip_bbu_mmc_register("sd", 0, "/dev/mmc1");

	return 0;
}

static const struct of_device_id rockpi_n10_of_match[] = {
	{ .compatible = "radxa,rockpi-n10" },
	{ /* Sentinel */},
};
BAREBOX_DEEP_PROBE_ENABLE(rockpi_n10_of_match);

static struct driver rockpi_n10_board_driver = {
	.name = "board-rockpi-n10",
	.probe = rockpi_n10_probe,
	.of_compatible = rockpi_n10_of_match,
};
coredevice_platform_driver(rockpi_n10_board_driver);
