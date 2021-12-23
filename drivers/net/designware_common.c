// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, Ahmad Fatoum, Pengutronix
 */

#include <common.h>
#include <net.h>
#include <of.h>

#include "designware_common.h"
#include "designware.h"
#include "designware_eqos.h"

int dwc_common_probe(struct device *dev, const struct dwc_common_ops *ops, void *priv)
{
	struct device_node *np = dev->of_node;
	bool is_eqos = false;

	if (of_device_is_compatible(np, "snps,dwmac-4.00") ||
	    of_device_is_compatible(np, "snps,dwmac-4.10a") ||
	    of_device_is_compatible(np, "snps,dwmac-4.20a") ||
	    of_device_is_compatible(np, "snps,dwmac-5.10a"))
		is_eqos = true;

	if (IS_ENABLED(CONFIG_DRIVER_NET_DESIGNWARE_EQOS) && is_eqos) {
		struct eqos *eqos;

		eqos = eqos_probe(dev, ops, priv);
		if (IS_ERR(eqos))
			return PTR_ERR(eqos);

		eqos->dwc.eqos = true;
		return 0;
	}

	if (IS_ENABLED(CONFIG_DRIVER_NET_DESIGNWARE) && !is_eqos)
		return PTR_ERR_OR_ZERO(dwc_drv_probe(dev, ops, priv));

	return -ENODEV;
}

int dwc_common_get_ethaddr(struct eth_device *edev, unsigned char *mac)
{
	return -EOPNOTSUPP;
}

static bool dwc_is_eqos(struct dwc_common *dwc)
{
	return IS_ENABLED(CONFIG_DRIVER_NET_DESIGNWARE_EQOS) && dwc->eqos;
}

static bool dwc_is_dwmac1000(struct dwc_common *dwc)
{
	return IS_ENABLED(CONFIG_DRIVER_NET_DESIGNWARE);
}

int dwc_common_set_ethaddr(struct eth_device *edev, const unsigned char *mac)
{
	struct dwc_common *dwc = edev->priv;

	if (dwc_is_eqos(dwc))
		return eqos_set_ethaddr(dwc, mac);
	else if (dwc_is_dwmac1000(dwc))
		return dwc_ether_set_ethaddr(dwc, mac);

	return -ENOSYS;
}

void dwc_common_adjust_link(struct eth_device *edev)
{
	struct dwc_common *dwc = edev->priv;

	if (dwc_is_eqos(dwc))
		eqos_adjust_link(dwc, edev->phydev);
	else if (dwc_is_dwmac1000(dwc))
		dwc_adjust_link(dwc, edev->phydev);
}

void dwc_common_remove(struct device *dev)
{
	struct dwc_common *dwc = dev->priv;

	if (dwc_is_eqos(dwc))
		eqos_remove(dwc);
	else if (dwc_is_dwmac1000(dwc))
		dwc_drv_remove(dwc);
}
