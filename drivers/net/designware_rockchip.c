// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <init.h>
#include <dma.h>
#include <net.h>
#include <regmap.h>
#include <of_net.h>
#include <mfd/syscon.h>
#include <linux/iopoll.h>
#include <linux/sizes.h>
#include <linux/time.h>
#include <linux/clk.h>

#include "designware_common.h"

struct eqos_rk_gmac;

struct rk_gmac_ops {
	void (*set_to_rgmii)(struct eqos_rk_gmac *priv,
			     int tx_delay, int rx_delay);
	void (*set_to_rmii)(struct eqos_rk_gmac *priv);
	void (*set_rmii_speed)(struct eqos_rk_gmac *priv, int speed);
	void (*set_rgmii_speed)(struct eqos_rk_gmac *priv, int speed);
	void (*integrated_phy_powerup)(struct eqos_rk_gmac *priv);
	const u32 *regs;
	int *required_clkids;
};

struct eqos_rk_gmac {
	struct clk_bulk_data *clks;
	int num_clks;
	bool clock_input;
	const struct rk_gmac_ops *ops;
	struct regmap *grf;
	int bus_id;
	u32 tx_delay;
	u32 rx_delay;
	struct device *dev;
};

enum {
	CLK_STMMACETH,
	CLK_MAC_RX,
	CLK_MAC_TX,
	CLK_MAC_REFOUT,
	CLK_MAC_ACLK,
	CLK_MAC_PCLK,
	CLK_MAC_SPEED,
	CLK_PTP_REF,
	CLK_MAC_REF,
};

static const struct clk_bulk_data rk_gmac_clks[] = {
	[CLK_STMMACETH]   = { .id = "stmmaceth" },
	[CLK_MAC_RX]      = { .id = "mac_clk_rx" },
	[CLK_MAC_TX]      = { .id = "mac_clk_tx" },
	[CLK_MAC_REFOUT]  = { .id = "clk_mac_refout" },
	[CLK_MAC_ACLK]    = { .id = "aclk_mac" },
	[CLK_MAC_PCLK]    = { .id = "pclk_mac" },
	[CLK_MAC_SPEED]   = { .id = "clk_mac_speed" },
	[CLK_PTP_REF]     = { .id = "ptp_ref" },
	[CLK_MAC_REF]     = { .id = "clk_mac_ref" },
};

static inline struct eqos_rk_gmac *to_rk_gmac(struct dwc_common *dwc)
{
	return dwc->priv;
}

#define HIWORD_UPDATE(val, mask, shift) \
		((val) << (shift) | (mask) << ((shift) + 16))

#define GRF_BIT(nr)	(BIT(nr) | BIT((nr) + 16))
#define GRF_CLR_BIT(nr)	(BIT((nr) + 16))

#define PX30_GRF_GMAC_CON1		0x0904

/* PX30_GRF_GMAC_CON1 */
#define PX30_GMAC_PHY_INTF_SEL_RMII	(GRF_CLR_BIT(4) | GRF_CLR_BIT(5) | \
					 GRF_BIT(6))
#define PX30_GMAC_SPEED_10M		GRF_CLR_BIT(2)
#define PX30_GMAC_SPEED_100M		GRF_BIT(2)

static void px30_set_to_rmii(struct eqos_rk_gmac *priv)
{
	struct device *dev = priv->dev;

	if (IS_ERR(priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	regmap_write(priv->grf, PX30_GRF_GMAC_CON1,
		     PX30_GMAC_PHY_INTF_SEL_RMII);
}

static void px30_set_rmii_speed(struct eqos_rk_gmac *priv, int speed)
{
	struct device *dev = priv->dev;
	struct clk *clk_mac_speed = priv->clks[CLK_MAC_SPEED].clk;
	int ret;

	if (IS_ERR(clk_mac_speed)) {
		dev_err(dev, "%s: Missing clk_mac_speed clock\n", __func__);
		return;
	}

	if (speed == SPEED_10) {
		regmap_write(priv->grf, PX30_GRF_GMAC_CON1,
			     PX30_GMAC_SPEED_10M);

		ret = clk_set_rate(clk_mac_speed, 2500000);
		if (ret)
			dev_err(dev, "%s: set clk_mac_speed rate 2500000 failed: %d\n",
				__func__, ret);
	} else if (speed == SPEED_100) {
		regmap_write(priv->grf, PX30_GRF_GMAC_CON1,
			     PX30_GMAC_SPEED_100M);

		ret = clk_set_rate(clk_mac_speed, 25000000);
		if (ret)
			dev_err(dev, "%s: set clk_mac_speed rate 25000000 failed: %d\n",
				__func__, ret);

	} else {
		dev_err(dev, "unknown speed value for RMII! speed=%d", speed);
	}
}

static const struct rk_gmac_ops px30_ops = {
	.set_to_rmii = px30_set_to_rmii,
	.set_rmii_speed = px30_set_rmii_speed,
	.required_clkids = (int []) {
		CLK_STMMACETH, CLK_MAC_RX, CLK_MAC_TX,
		CLK_MAC_ACLK, CLK_MAC_PCLK, CLK_MAC_SPEED,
		-1
	}
};

#define DELAY_ENABLE(soc, tx, rx) \
	(((tx) ? soc##_GMAC_TXCLK_DLY_ENABLE : soc##_GMAC_TXCLK_DLY_DISABLE) | \
	 ((rx) ? soc##_GMAC_RXCLK_DLY_ENABLE : soc##_GMAC_RXCLK_DLY_DISABLE))

#define RK3399_GRF_SOC_CON5	0xc214
#define RK3399_GRF_SOC_CON6	0xc218

/* RK3399_GRF_SOC_CON5 */
#define RK3399_GMAC_PHY_INTF_SEL_RGMII	(GRF_BIT(9) | GRF_CLR_BIT(10) | \
					 GRF_CLR_BIT(11))
#define RK3399_GMAC_PHY_INTF_SEL_RMII	(GRF_CLR_BIT(9) | GRF_CLR_BIT(10) | \
					 GRF_BIT(11))
#define RK3399_GMAC_FLOW_CTRL		GRF_BIT(8)
#define RK3399_GMAC_FLOW_CTRL_CLR	GRF_CLR_BIT(8)
#define RK3399_GMAC_SPEED_10M		GRF_CLR_BIT(7)
#define RK3399_GMAC_SPEED_100M		GRF_BIT(7)
#define RK3399_GMAC_RMII_CLK_25M	GRF_BIT(3)
#define RK3399_GMAC_RMII_CLK_2_5M	GRF_CLR_BIT(3)
#define RK3399_GMAC_CLK_125M		(GRF_CLR_BIT(4) | GRF_CLR_BIT(5))
#define RK3399_GMAC_CLK_25M		(GRF_BIT(4) | GRF_BIT(5))
#define RK3399_GMAC_CLK_2_5M		(GRF_CLR_BIT(4) | GRF_BIT(5))
#define RK3399_GMAC_RMII_MODE		GRF_BIT(6)
#define RK3399_GMAC_RMII_MODE_CLR	GRF_CLR_BIT(6)

/* RK3399_GRF_SOC_CON6 */
#define RK3399_GMAC_TXCLK_DLY_ENABLE	GRF_BIT(7)
#define RK3399_GMAC_TXCLK_DLY_DISABLE	GRF_CLR_BIT(7)
#define RK3399_GMAC_RXCLK_DLY_ENABLE	GRF_BIT(15)
#define RK3399_GMAC_RXCLK_DLY_DISABLE	GRF_CLR_BIT(15)
#define RK3399_GMAC_CLK_RX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 8)
#define RK3399_GMAC_CLK_TX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 0)

static void rk3399_set_to_rgmii(struct eqos_rk_gmac *priv,
				int tx_delay, int rx_delay)
{
	struct device *dev = priv->dev;

	if (IS_ERR(priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	regmap_write(priv->grf, RK3399_GRF_SOC_CON5,
		     RK3399_GMAC_PHY_INTF_SEL_RGMII |
		     RK3399_GMAC_RMII_MODE_CLR);
	regmap_write(priv->grf, RK3399_GRF_SOC_CON6,
		     DELAY_ENABLE(RK3399, tx_delay, rx_delay) |
		     RK3399_GMAC_CLK_RX_DL_CFG(rx_delay) |
		     RK3399_GMAC_CLK_TX_DL_CFG(tx_delay));
}

static void rk3399_set_to_rmii(struct eqos_rk_gmac *priv)
{
	struct device *dev = priv->dev;

	if (IS_ERR(priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	regmap_write(priv->grf, RK3399_GRF_SOC_CON5,
		     RK3399_GMAC_PHY_INTF_SEL_RMII | RK3399_GMAC_RMII_MODE);
}

static void rk3399_set_rgmii_speed(struct eqos_rk_gmac *priv, int speed)
{
	struct device *dev = priv->dev;

	if (IS_ERR(priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	if (speed == SPEED_10)
		regmap_write(priv->grf, RK3399_GRF_SOC_CON5,
			     RK3399_GMAC_CLK_2_5M);
	else if (speed == SPEED_100)
		regmap_write(priv->grf, RK3399_GRF_SOC_CON5,
			     RK3399_GMAC_CLK_25M);
	else if (speed == SPEED_1000)
		regmap_write(priv->grf, RK3399_GRF_SOC_CON5,
			     RK3399_GMAC_CLK_125M);
	else
		dev_err(dev, "unknown speed value for RGMII! speed=%d", speed);
}

static void rk3399_set_rmii_speed(struct eqos_rk_gmac *priv, int speed)
{
	struct device *dev = priv->dev;

	if (IS_ERR(priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	if (speed == SPEED_10) {
		regmap_write(priv->grf, RK3399_GRF_SOC_CON5,
			     RK3399_GMAC_RMII_CLK_2_5M |
			     RK3399_GMAC_SPEED_10M);
	} else if (speed == SPEED_100) {
		regmap_write(priv->grf, RK3399_GRF_SOC_CON5,
			     RK3399_GMAC_RMII_CLK_25M |
			     RK3399_GMAC_SPEED_100M);
	} else {
		dev_err(dev, "unknown speed value for RMII! speed=%d", speed);
	}
}

static __maybe_unused const struct rk_gmac_ops rk3399_ops = {
	.set_to_rgmii = rk3399_set_to_rgmii,
	.set_to_rmii = rk3399_set_to_rmii,
	.set_rmii_speed = rk3399_set_rmii_speed,
	.set_rgmii_speed = rk3399_set_rgmii_speed,
	.required_clkids = (int []) {
		CLK_STMMACETH, CLK_MAC_RX, CLK_MAC_TX, CLK_MAC_REFOUT,
		CLK_MAC_ACLK, CLK_MAC_PCLK, CLK_MAC_REF, -1
	}
};

#define RK3568_GRF_GMAC0_CON0		0x0380
#define RK3568_GRF_GMAC0_CON1		0x0384
#define RK3568_GRF_GMAC1_CON0		0x0388
#define RK3568_GRF_GMAC1_CON1		0x038c

/* RK3568_GRF_GMAC0_CON1 && RK3568_GRF_GMAC1_CON1 */
#define RK3568_GMAC_PHY_INTF_SEL_RGMII	\
		(GRF_BIT(4) | GRF_CLR_BIT(5) | GRF_CLR_BIT(6))
#define RK3568_GMAC_PHY_INTF_SEL_RMII	\
		(GRF_CLR_BIT(4) | GRF_CLR_BIT(5) | GRF_BIT(6))
#define RK3568_GMAC_FLOW_CTRL			GRF_BIT(3)
#define RK3568_GMAC_FLOW_CTRL_CLR		GRF_CLR_BIT(3)
#define RK3568_GMAC_RXCLK_DLY_ENABLE		GRF_BIT(1)
#define RK3568_GMAC_RXCLK_DLY_DISABLE		GRF_CLR_BIT(1)
#define RK3568_GMAC_TXCLK_DLY_ENABLE		GRF_BIT(0)
#define RK3568_GMAC_TXCLK_DLY_DISABLE		GRF_CLR_BIT(0)

/* RK3568_GRF_GMAC0_CON0 && RK3568_GRF_GMAC1_CON0 */
#define RK3568_GMAC_CLK_RX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 8)
#define RK3568_GMAC_CLK_TX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 0)

static unsigned long eqos_get_csr_clk_rate_rk_gmac(struct dwc_common *dwc)
{
	struct eqos_rk_gmac *priv = to_rk_gmac(dwc);

	return clk_get_rate(priv->clks[CLK_STMMACETH].clk);
}

static void rk3568_set_to_rgmii(struct eqos_rk_gmac *priv,
				int tx_delay, int rx_delay)
{
	struct device *dev = priv->dev;
	u32 offset_con0, offset_con1;

	if (IS_ERR(priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	offset_con0 = (priv->bus_id == 1)
		      ? RK3568_GRF_GMAC1_CON0 : RK3568_GRF_GMAC0_CON0;
	offset_con1 = (priv->bus_id == 1)
		      ? RK3568_GRF_GMAC1_CON1 : RK3568_GRF_GMAC0_CON1;

	regmap_write(priv->grf, offset_con1,
		     RK3568_GMAC_PHY_INTF_SEL_RGMII |
		     RK3568_GMAC_RXCLK_DLY_ENABLE |
		     RK3568_GMAC_TXCLK_DLY_ENABLE);

	regmap_write(priv->grf, offset_con0,
		     RK3568_GMAC_CLK_RX_DL_CFG(rx_delay) |
		     RK3568_GMAC_CLK_TX_DL_CFG(tx_delay));
}

static void rk3568_set_to_rmii(struct eqos_rk_gmac *priv)
{
	struct device *dev = priv->dev;
	u32 offset_con1;

	if (IS_ERR(priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	offset_con1 = (priv->bus_id == 1)
		      ? RK3568_GRF_GMAC1_CON1 : RK3568_GRF_GMAC0_CON1;

	regmap_write(priv->grf, offset_con1,
		     RK3568_GMAC_PHY_INTF_SEL_RMII);
}

static void rk3568_set_gmac_speed(struct eqos_rk_gmac *priv, int speed)
{
	struct device *dev = priv->dev;
	unsigned long rate;
	int ret;

	switch (speed) {
	case SPEED_10:
		rate = 2500000;
		break;
	case SPEED_100:
		rate = 25000000;
		break;
	case SPEED_1000:
		rate = 125000000;
		break;
	default:
		dev_err(dev, "unknown speed value for GMAC speed=%d", speed);
		return;
	}

	ret = clk_set_rate(priv->clks[CLK_MAC_SPEED].clk, rate);
	if (ret)
		dev_err(dev, "%s: set clk_mac_speed rate %ld failed %d\n",
			__func__, rate, ret);
}

static const struct rk_gmac_ops rk3568_ops = {
	.set_to_rgmii = rk3568_set_to_rgmii,
	.set_to_rmii = rk3568_set_to_rmii,
	.set_rmii_speed = rk3568_set_gmac_speed,
	.set_rgmii_speed = rk3568_set_gmac_speed,
	.regs = (u32 []) {
		0xfe2a0000, /* gmac0 */
		0xfe010000, /* gmac1 */
		0x0, /* sentinel */
	},
	.required_clkids = (int []) {
		CLK_STMMACETH, CLK_MAC_RX, CLK_MAC_TX, CLK_MAC_REFOUT,
		CLK_MAC_ACLK, CLK_MAC_PCLK, CLK_MAC_SPEED, CLK_PTP_REF,
		-1
	}
};

static int rk_gmac_powerup(struct dwc_common *dwc)
{
	struct eqos_rk_gmac *priv = to_rk_gmac(dwc);
	struct device *dev = priv->dev;

	/*rmii or rgmii*/
	switch (dwc->interface) {
	case PHY_INTERFACE_MODE_RGMII:
		dev_dbg(dev, "init for RGMII\n");
		priv->ops->set_to_rgmii(priv, priv->tx_delay,
					    priv->rx_delay);
		break;
	case PHY_INTERFACE_MODE_RGMII_ID:
		dev_dbg(dev, "init for RGMII_ID\n");
		priv->ops->set_to_rgmii(priv, 0, 0);
		break;
	case PHY_INTERFACE_MODE_RGMII_RXID:
		dev_dbg(dev, "init for RGMII_RXID\n");
		priv->ops->set_to_rgmii(priv, priv->tx_delay, 0);
		break;
	case PHY_INTERFACE_MODE_RGMII_TXID:
		dev_dbg(dev, "init for RGMII_TXID\n");
		priv->ops->set_to_rgmii(priv, 0, priv->rx_delay);
		break;
	case PHY_INTERFACE_MODE_RMII:
		dev_dbg(dev, "init for RMII\n");
		priv->ops->set_to_rmii(priv);
		break;
	default:
		dev_err(dev, "NO interface defined!\n");
	}

	return 0;
}

static void eqos_rk_adjust_link(struct eth_device *edev)
{
	struct dwc_common *dwc = edev->priv;
	struct eqos_rk_gmac *priv = to_rk_gmac(dwc);

	if (phy_interface_mode_is_rgmii(dwc->interface))
		priv->ops->set_rgmii_speed(priv, edev->phydev->speed);
	else
		priv->ops->set_rmii_speed(priv, edev->phydev->speed);

	dwc_common_adjust_link(edev);
}

static int eqos_init_rk_gmac(struct device *dev, struct dwc_common *dwc)
{
	struct device_node *np = dev->of_node;
	struct eqos_rk_gmac *priv = to_rk_gmac(dwc);
	int i = 0, ret, *clkid;
	const char *strings;

	priv->dev = dev;

	ret = of_property_read_string(np, "clock_in_out", &strings);
	if (ret) {
                dev_err(dev, "Can not read property: clock_in_out.\n");
                priv->clock_input = true;
	} else {
		dev_dbg(dev, "clock is %s\n", strings);
		if (!strcmp(strings, "input"))
			priv->clock_input = true;
		else
			priv->clock_input = false;
	}

	priv->ops = device_get_match_data(dev);

	if (dev->num_resources > 0 && priv->ops->regs) {
		while (priv->ops->regs[i]) {
			if (priv->ops->regs[i] == dev->resource[0].start) {
				priv->bus_id = i;
				break;
			}
			i++;
		}
	}

	priv->grf = syscon_regmap_lookup_by_phandle(np, "rockchip,grf");
	if (IS_ERR(priv->grf)) {
		dev_err(dev, "unable to get grf");
		return PTR_ERR(priv->grf);
	}

	priv->tx_delay = 0x30;
	of_property_read_u32(np, "tx_delay", &priv->tx_delay);
	priv->rx_delay = 0x10;
	of_property_read_u32(np, "rx_delay", &priv->rx_delay);

	priv->num_clks = ARRAY_SIZE(rk_gmac_clks);
	priv->clks = xmalloc(priv->num_clks * sizeof(*priv->clks));
	memcpy(priv->clks, rk_gmac_clks, sizeof rk_gmac_clks);

	ret = clk_bulk_get_optional(dev, priv->num_clks, priv->clks);
	if (ret)
		return ret;

	for (clkid = priv->ops->required_clkids; *clkid >= 0; clkid++) {
		if (!priv->clks[*clkid].clk)
			return dev_err_probe(dev, -ENOENT, "Failed to get clks\n");
	}

	ret = clk_bulk_enable(priv->num_clks, priv->clks);
	if (ret) {
		dev_err(dev, "Failed to enable clks: %s\n", strerror(-ret));
		return ret;
	}

	rk_gmac_powerup(dwc);

	return 0;
}

static struct dwc_common_ops rk_gmac_ops = {
	.init = eqos_init_rk_gmac,
	.get_ethaddr = dwc_common_get_ethaddr,
	.set_ethaddr = dwc_common_set_ethaddr,
	.adjust_link = eqos_rk_adjust_link,
	.get_csr_clk_rate = eqos_get_csr_clk_rate_rk_gmac,

	.clk_csr = EQOS_MDIO_ADDR_CR_250_300,
	.config_mac = EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_AV,
};

static int rk_gmac_probe(struct device *dev)
{
	return dwc_common_probe(dev, &rk_gmac_ops, xzalloc(sizeof(struct eqos_rk_gmac)));
}

static __maybe_unused struct of_device_id rk_gmac_compatible[] = {
#ifdef CONFIG_DRIVER_NET_DESIGNWARE
	{ .compatible = "rockchip,px30-gmac", .data = &px30_ops },
	{ .compatible = "rockchip,rk3399-gmac", .data = &rk3399_ops },
#endif
#ifdef CONFIG_DRIVER_NET_DESIGNWARE_EQOS
	{ .compatible = "rockchip,rk3568-gmac", .data = &rk3568_ops },
#endif
	{ /* sentinel */ }
};

static struct driver rk_gmac_driver = {
	.name = "eqos-rockchip",
	.probe = rk_gmac_probe,
	.remove = dwc_common_remove,
	.of_compatible = DRV_OF_COMPAT(rk_gmac_compatible),
};
device_platform_driver(rk_gmac_driver);
