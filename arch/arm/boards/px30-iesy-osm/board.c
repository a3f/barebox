// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <init.h>
#include <bootsource.h>
#include <environment.h>
#include <deep-probe.h>

static int iesy_px30_osm_probe(struct device *dev)
{
	enum bootsource bootsource = bootsource_get();
	int instance = bootsource_get_instance();

	if (bootsource == BOOTSOURCE_MMC && instance == 0)
		of_device_enable_path("/chosen/environment-sd");
	else
		of_device_enable_path("/chosen/environment-emmc");

	return 0;
}

static const struct of_device_id iesy_px30_osm_of_match[] = {
	{ .compatible = "iesy,rpx30-osm" },
	{ /* Sentinel */},
};
BAREBOX_DEEP_PROBE_ENABLE(iesy_px30_osm_of_match);

static struct driver iesy_px30_osm_board_driver = {
	.name = "board-iesy-px30-osm",
	.probe = iesy_px30_osm_probe,
	.of_compatible = iesy_px30_osm_of_match,
};
coredevice_platform_driver(iesy_px30_osm_board_driver);
