// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * (C) Copyright 2010
 * Vipin Kumar, ST Micoelectronics, vipin.kumar@st.com.
 */

/*
 * Designware ethernet IP driver
 */

#include <common.h>
#include <dma.h>
#include <init.h>
#include <io.h>
#include <net.h>
#include <of_net.h>
#include <platform_data/eth-designware.h>
#include <linux/phy.h>
#include <linux/err.h>
#include <linux/iopoll.h>

#include "designware.h"
#include "dwmac.h"

/* Get PHY out of power saving mode.  If this is needed elsewhere then
 * consider making it part of phy-core and adding a resume method to
 * the phy device ops.  */
static int phy_resume(struct phy_device *phydev)
{
	int bmcr;

	bmcr = phy_read(phydev, MII_BMCR);
	if (bmcr < 0)
		return bmcr;
	if (bmcr & BMCR_PDOWN) {
		bmcr &= ~BMCR_PDOWN;
		return phy_write(phydev, MII_BMCR, bmcr);
	}
	return 0;
}

static int dwmac_start(struct eth_device *edev)
{
	struct dw_eth_dev *priv = edev->priv;
#if 0
	int ret;

	ret = phy_device_connect(edev, &priv->miibus, priv->phy_addr,
				 priv->ops->adjust_link, 0, priv->interface);
	if (ret)
		return ret;

	/* Before we reset the mac, we must insure the PHY is not powered down
	 * as the dw controller needs all clock domains to be running, including
	 * the PHY clock, to come out of a mac reset.  */
	ret = phy_resume(edev->phydev);
	if (ret)
		return ret;
#endif

	return priv->ops->start(edev);
}

static int dwmac_probe_dt(struct device_d *dev, struct dw_eth_dev *priv)
{
	struct device_node *child;

	if (!IS_ENABLED(CONFIG_OFTREE))
		return -ENODEV;

	priv->phy_addr = -1;
	priv->interface = of_get_phy_mode(dev->device_node);

	/* Set MDIO bus device node, if present. */
	for_each_child_of_node(dev->device_node, child) {
		if (of_device_is_compatible(child, "snps,dwmac-mdio")) {
			priv->miibus.dev.device_node = child;
			break;
		}
	}

	return 0;
}

struct dw_eth_dev *dwmac_drv_probe(struct device_d *dev, struct dw_eth_ops *ops)
{
	struct dwc_ether_platform_data *pdata = dev->platform_data;
	struct resource *iores;
	struct dw_eth_dev *priv;
	struct eth_device *edev;
	int ret;

	priv = xzalloc(sizeof(struct dw_eth_dev));

	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return ERR_CAST(iores);
	priv->regs = IOMEM(iores->start);

	priv->mac_regs_p = priv->regs;
	priv->dma_regs_p = priv->regs + DW_DMA_BASE_OFFSET;
	priv->ops = ops;

	if (pdata) {
		priv->phy_addr = pdata->phy_addr;
		priv->interface = pdata->interface;
		priv->fix_mac_speed = pdata->fix_mac_speed;
	} else {
		ret = dwmac_probe_dt(dev, priv);
		if (ret)
			return ERR_PTR(ret);
	}

	edev = &priv->netdev;
	edev->priv = priv;

	dev->priv = edev;
	edev->parent = dev;
	edev->open = dwmac_start;
	edev->send = ops->send;
	edev->recv = ops->rx;
	edev->halt = ops->halt;
	edev->get_ethaddr = ops->get_ethaddr;
	edev->set_ethaddr = ops->set_ethaddr;

	ret = ops->init(priv);
	if (ret)
		return ERR_PTR(ret);

	eth_register(edev);

	return priv;
}

void dwmac_drv_remove(struct device_d *dev) // FIXME export
{
}
