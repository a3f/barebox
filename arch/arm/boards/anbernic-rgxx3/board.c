// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <init.h>
#include <envfs.h>
#include <bootsource.h>
#include <mach/rockchip/bbu.h>

struct rgxx3_drvdata {
};

static bool of_emmc_is_available(void)
{
	struct device_node *np;

	np = of_find_node_by_alias(NULL, "mmc0");
	return np && of_device_is_available(np);
}

static int anbernic_rgxx3_probe(struct device *dev)
{
	int sd_flags = BBU_HANDLER_FLAG_DEFAULT;

	if (of_emmc_is_available()) {
		int emmc_flags = 0;

		/* While we have an eMMC, we are booting from SD right now */
		if (bootsource_get() == BOOTSOURCE_MMC &&
		    bootsource_get_instance() == 1) {
			emmc_flags = BBU_HANDLER_FLAG_DEFAULT;
			sd_flags = 0;
		}

		rockchip_bbu_mmc_register("emmc", emmc_flags, "/dev/mmc0");
	}

	rockchip_bbu_mmc_register("sd", sd_flags, "/dev/mmc1");

	/*
	 * Not all boards have a serial port, so enable the
	 * USB ACM serial gadget unconditionally
	 */
	defaultenv_append_directory(defaultenv_anbernic_rgxx3);

	return 0;
}

static const struct of_device_id anbernic_rgxx3_of_match[] = {
	{ .compatible = "anbernic,rg353p" },
	{ .compatible = "anbernic,rg353ps" },
	{ .compatible = "anbernic,rg353v" },
	{ .compatible = "anbernic,rg353vs" },
	{ .compatible = "anbernic,rg503" },
	{ .compatible = "anbernic,rg-arc-d" },
	{ .compatible = "anbernic,rg-arc-s" },
	{ .compatible = "anbernic,rgb10max3" },
	{ .compatible = "powkiddy,rgb30" },
	{ .compatible = "powkiddy,rk2023" },
	{ .compatible = "powkiddy,x55" },
	{ /* sentinel */ },
};
BAREBOX_DEEP_PROBE_ENABLE(anbernic_rgxx3_of_match);

static struct driver anbernic_rgxx3_board_driver = {
	.name = "board-anbernic-rgxx3",
	.probe = anbernic_rgxx3_probe,
	.of_compatible = anbernic_rgxx3_of_match,
};
coredevice_platform_driver(anbernic_rgxx3_board_driver);
