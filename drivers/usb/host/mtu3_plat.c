// SPDX-License-Identifier: GPL-2.0-only

#include <driver.h>
#include <linux/clk.h>

static int mtu3_probe(struct device *dev)
{
	struct clk_bulk_data *clks;
	int ret;

	/* We assume PHY, power domain and Mediatek specific handling
	 * to be already done by Coreboot
	 *
	 * TODO: Is that assumption correct?
	 * TODO: Do we even need to enable the clocks ourselves?
	 */

	ret = clk_bulk_get_all_enabled(dev, &clks);
	if (ret < 0)
		return ret;

	return of_platform_populate(dev->of_node, NULL, dev);
}

static const struct of_device_id mtu3_dt_ids[] = {
	{ .compatible = "mediatek,mtu3" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mtu3_dt_ids);

static struct driver mtu3_driver = {
	.name = "mtu3",
	.probe = mtu3_probe,
	.of_compatible = mtu3_dt_ids,
};
device_platform_driver(mtu3_driver);
