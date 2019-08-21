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
#include <linux/clk.h>
#include <linux/mii.h>

#include "dwmac.h"
#include "dwmac4.h"
#include "dwmac1000.h"

static int dwmac_mdio_wait_idle(struct dw_eth_dev *priv)
{
	u32 idle;
	return readl_poll_timeout(priv->regs + priv->ops->mii->addr, idle,
				  !(idle & MII_BUSY),
				  10000);
}

static int dwmac_mdio_read(struct mii_bus *bus, int mdio_addr,
			  int mdio_reg)
{
	struct dw_eth_dev *priv = bus->priv;
	u32 miiaddr = 0;
	int ret;

	ret = dwmac_mdio_wait_idle(priv);
	if (ret) {
		pr_err("MDIO not idle at entry\n");
		return ret;
	}

	if (priv->ops->has_gmac4) {
		miiaddr = readl(priv->regs + priv->ops->mii->addr);
		miiaddr &= EQOS_MAC_MDIO_ADDRESS_SKAP |
			EQOS_MAC_MDIO_ADDRESS_C45E;
		miiaddr |= EQOS_MAC_MDIO_ADDRESS_GOC_READ <<
			EQOS_MAC_MDIO_ADDRESS_GOC_SHIFT;
	}

	miiaddr |= (priv->ops->clk_csr << priv->ops->mii->clk_csr_shift) &
		priv->ops->mii->clk_csr_mask;

	miiaddr |= ((mdio_addr << priv->ops->mii->addr_shift) & priv->ops->mii->addr_mask) |
		((mdio_reg << priv->ops->mii->reg_shift) & priv->ops->mii->reg_mask);
	miiaddr |= MII_BUSY;

	writel(miiaddr, priv->regs + priv->ops->mii->addr);

	udelay(priv->ops->mdio_wait);

	ret = dwmac_mdio_wait_idle(priv);
	if (ret) {
		pr_err("MDIO read didn't complete\n");
		return ret;
	}

	return readl(priv->regs + priv->ops->mii->data) & 0xffff;
}

static int dwmac_mdio_write(struct mii_bus *bus, int mdio_addr,
			   int mdio_reg, u16 val)
{
	struct dw_eth_dev *priv = bus->priv;
	u32 miiaddr = 0;
	int ret;

	ret = dwmac_mdio_wait_idle(priv);
	if (ret) {
		pr_err("MDIO not idle at entry\n");
		return ret;
	}

	if (priv->ops->has_gmac4) {
		miiaddr = readl(priv->regs + priv->ops->mii->addr);
		miiaddr &= EQOS_MAC_MDIO_ADDRESS_SKAP |
			EQOS_MAC_MDIO_ADDRESS_C45E;
		miiaddr |= EQOS_MAC_MDIO_ADDRESS_GOC_WRITE <<
			 EQOS_MAC_MDIO_ADDRESS_GOC_SHIFT;
	} else {
		miiaddr = MII_WRITE;
	}

	miiaddr |= (priv->ops->clk_csr << priv->ops->mii->clk_csr_shift) &
		priv->ops->mii->clk_csr_mask;

	miiaddr |= ((mdio_addr << priv->ops->mii->addr_shift) & priv->ops->mii->addr_mask) |
		((mdio_reg << priv->ops->mii->reg_shift) & priv->ops->mii->reg_mask);
	miiaddr |= MII_BUSY;

	writel(val, priv->regs + priv->ops->mii->data);
	writel(miiaddr, priv->regs + priv->ops->mii->addr);

	udelay(priv->ops->mdio_wait);

	ret = dwmac_mdio_wait_idle(priv);
	if (ret) {
		pr_err("MDIO read didn't complete\n");
		return ret;
	}

	/* Needed as a fix for ST-Phy */
	dwmac_mdio_read(bus, mdio_addr, mdio_reg);
	return 0;
}

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
	int ret;

	ret = clk_bulk_enable(priv->num_clks, priv->clks);
	if (ret < 0) {
		pr_err("clk_bulk_enable() failed: %s\n", strerror(-ret));
		return ret;
	}

	if (priv->ops->clks_set_rate) {
		ret = priv->ops->clks_set_rate(priv);
		if (ret < 0) {
			pr_err("clks_set_rate() failed: %s\n", strerror(-ret));
			goto err;
		}
	}

	if (priv->ops->reset) {
		ret = priv->ops->reset(priv, 0);
		if (ret < 0) {
			pr_err("reset(0) failed: %s\n", strerror(-ret));
			goto err_stop_clks;
		}
	}

	udelay(10);

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

	ret = dwmac_reset(priv);
	if (ret)
		return ret;

	ret = priv->ops->start(edev);
	if (ret)
		goto err_stop_resets;

	return 0;

err_stop_resets:
	if (priv->ops->reset)
		priv->ops->reset(priv, 1);
err_stop_clks:
	clk_bulk_disable(priv->num_clks, priv->clks);
err:
	pr_err("%s: failed with %s\n", __func__, strerror(-ret));
	return ret;
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
	struct mii_bus *miibus;
	struct device_node *mdiobus;
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

	miibus = &priv->miibus;
	priv->phy_addr = -1;
	mdiobus = of_get_child_by_name(dev->device_node, "mdio");
	if (mdiobus)
		miibus->dev.device_node = mdiobus;

	miibus->parent = edev->parent;
	miibus->read = dwmac_mdio_read;
	miibus->write = dwmac_mdio_write;
	miibus->priv = priv;

	int ret = dwmac_reset(priv);
	if (ret)
		return ret;

	/* HW MAC address is lost during MAC reset */
	dev->set_ethaddr(dev, priv->macaddr);

	ret = ops->init(priv);
	if (ret)
		return ERR_PTR(ret);

	mdiobus_register(miibus);
	eth_register(edev);

	return priv;
}

int dwmac_reset(struct dw_eth_dev *priv)
{
	struct eth_mac_regs *mac_p = priv->mac_regs_p;
	struct eth_dma_regs *dma_p = priv->dma_regs_p;
	u64 start;

	writel(readl(&dma_p->busmode) | DMAMAC_SRST, &dma_p->busmode);

	if (priv->ops->has_gmac4 && priv->interface != PHY_INTERFACE_MODE_RGMII)
		writel(MII_PORTSELECT, &mac_p->conf);

	start = get_time_ns();
	while (readl(&dma_p->busmode) & DMAMAC_SRST) {
		if (is_timeout(start, 10 * MSECOND)) {
			dev_err(&priv->netdev.dev, "MAC reset timeout\n");
			return -EIO;
		}
	}
	return 0;
}


void dwmac_drv_remove(struct device_d *dev) // FIXME export
{
}
