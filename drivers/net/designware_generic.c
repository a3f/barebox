// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * (C) Copyright 2010
 * Vipin Kumar, ST Micoelectronics, vipin.kumar@st.com.
 */

/*
 * Designware ethernet IP driver for u-boot
 */

#include <common.h>
#include <init.h>
#include "designware_common.h"

static const struct dwc_common_ops dwmac_370a_ops = {
	.get_ethaddr = dwc_common_get_ethaddr,
	.set_ethaddr = dwc_common_set_ethaddr,
	.adjust_link = dwc_common_adjust_link,
	.enh_desc = 1,
};

static int dwc_ether_probe(struct device *dev)
{
	return dwc_common_probe(dev, device_get_match_data(dev), NULL);
}

static __maybe_unused struct of_device_id dwc_ether_compatible[] = {
	{
		.compatible = "snps,dwmac-3.70a",
		.data = &dwmac_370a_ops,
	}, {
		.compatible = "snps,dwmac-3.72a",
		.data = &dwmac_370a_ops,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, dwc_ether_compatible);

static struct driver dwc_ether_driver = {
	.name = "designware_eth",
	.probe = dwc_ether_probe,
	.remove	= dwc_common_remove,
	.of_compatible = DRV_OF_COMPAT(dwc_ether_compatible),
};
device_platform_driver(dwc_ether_driver);
