// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 Ahmad Fatoum, Pengutronix, <a.fatoum@pengutronix.de>
 * Copyright (c) 2017 Alexandre Torgue, ST Microelectronics, <alexandre.torgue@st.com>
 * Copyright (c) 2010 Vipin Kumar, ST Micoelectronics, <vipin.kumar@st.com>
 */

#define DEBUG 1

#include <common.h>
#include <init.h>
#include <io.h>
#include <net.h>
#include <linux/clk.h>
#include <of_net.h>
#include <linux/reset.h>
#include <mfd/syscon.h>
#include "designware.h"

#define SYSCFG_MCU_ETH_MASK		BIT(23)
#define SYSCFG_MP1_ETH_MASK		GENMASK(23, 16)
#define SYSCFG_PMCCLRR_OFFSET		0x40

#define SYSCFG_PMCR_ETH_CLK_SEL		BIT(16)
#define SYSCFG_PMCR_ETH_REF_CLK_SEL	BIT(17)

/*  Ethernet PHY interface selection in register SYSCFG Configuration
 *------------------------------------------
 * src	 |BIT(23)| BIT(22)| BIT(21)|BIT(20)|
 *------------------------------------------
 * MII   |   0	 |   0	  |   0    |   1   |
 *------------------------------------------
 * GMII  |   0	 |   0	  |   0    |   0   |
 *------------------------------------------
 * RGMII |   0	 |   0	  |   1	   |  n/a  |
 *------------------------------------------
 * RMII  |   1	 |   0	  |   0	   |  n/a  |
 *------------------------------------------
 */
#define SYSCFG_PMCR_ETH_SEL_MII		BIT(20)
#define SYSCFG_PMCR_ETH_SEL_RGMII	BIT(21)
#define SYSCFG_PMCR_ETH_SEL_RMII	BIT(23)
#define SYSCFG_PMCR_ETH_SEL_GMII	0
#define SYSCFG_MCU_ETH_SEL_MII		0
#define SYSCFG_MCU_ETH_SEL_RMII		1

/* STM32MP1 register definitions
 *
 * Below table summarizes the clock requirement and clock sources for
 * supported phy interface modes.
 * __________________________________________________________________________
 *|PHY_MODE | Normal | PHY wo crystal|   PHY wo crystal   |No 125Mhz from PHY|
 *|         |        |      25MHz    |        50MHz       |                  |
 * ---------------------------------------------------------------------------
 *|  MII    |	 -   |     eth-ck    |	      n/a	  |	  n/a        |
 *|         |        |		     |                    |		     |
 * ---------------------------------------------------------------------------
 *|  GMII   |	 -   |     eth-ck    |	      n/a	  |	  n/a        |
 *|         |        |               |                    |		     |
 * ---------------------------------------------------------------------------
 *| RGMII   |	 -   |     eth-ck    |	      n/a	  |  eth-ck (no pin) |
 *|         |        |               |                    |  st,eth-clk-sel  |
 * ---------------------------------------------------------------------------
 *| RMII    |	 -   |     eth-ck    |	    eth-ck        |	  n/a        |
 *|         |        |		     | st,eth-ref-clk-sel |		     |
 * ---------------------------------------------------------------------------
 *
 * BIT(17) : set this bit in RMII mode when you have PHY without crystal 50MHz
 * BIT(16) : set this bit in GMII/RGMII PHY when you do not want use 125Mhz
 * from PHY
 *-----------------------------------------------------
 * src	 |         BIT(17)       |       BIT(16)      |
 *-----------------------------------------------------
 * MII   |           n/a	 |         n/a        |
 *-----------------------------------------------------
 * GMII  |           n/a         |   st,eth-clk-sel   |
 *-----------------------------------------------------
 * RGMII |           n/a         |   st,eth-clk-sel   |
 *-----------------------------------------------------
 * RMII  |   st,eth-ref-clk-sel	 |         n/a        |
 *-----------------------------------------------------
 *
 */

struct stm32_dwc_dev {
	struct dw_eth_dev *priv;
	struct regmap *regmap;
	u32 mode_reg; /* MAC glue-logic mode register */
	int eth_clk_sel_reg;
	int eth_ref_clk_sel_reg;
	struct stm32_ops *ops;
	unsigned extra_clks;
	struct clk_bulk_data *clks;
};

static struct stm32_dwc_dev *g_dev; // FIXME!!

extern unsigned long eqos_get_tick_clk_rate_stm32(void)
{
	struct clk *clk = clk_get(g_dev, "stmmaceth");
	if (IS_ERR(clk)) {
		dev_warn(g_dev, "FIXME can't get stmmaceth clock\n");
		hang();
	}
	return clk_get_rate(clk);
}

struct stm32_ops {
	int (*set_mode)(struct stm32_dwc_dev *);
	int (*probe_dt)(struct stm32_dwc_dev *, struct device_d *);
	u32 syscfg_eth_mask;
	unsigned max_extra_clks;
};

static int stm32mp1_dwc_set_mode(struct stm32_dwc_dev *dwmac)
{
	struct dw_eth_dev *eth_dev = dwmac->priv;
	u32 val, reg = dwmac->mode_reg;
	int ret;

	switch (eth_dev->interface) {
	case PHY_INTERFACE_MODE_MII:
		val = SYSCFG_PMCR_ETH_SEL_MII;
		pr_debug("SYSCFG init : PHY_INTERFACE_MODE_MII\n");
		break;
	case PHY_INTERFACE_MODE_GMII:
		val = SYSCFG_PMCR_ETH_SEL_GMII;
		if (dwmac->eth_clk_sel_reg)
			val |= SYSCFG_PMCR_ETH_CLK_SEL;
		pr_debug("SYSCFG init : PHY_INTERFACE_MODE_GMII\n");
		break;
	case PHY_INTERFACE_MODE_RMII:
		val = SYSCFG_PMCR_ETH_SEL_RMII;
		if (dwmac->eth_ref_clk_sel_reg)
			val |= SYSCFG_PMCR_ETH_REF_CLK_SEL;
		pr_debug("SYSCFG init : PHY_INTERFACE_MODE_RMII\n");
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
		val = SYSCFG_PMCR_ETH_SEL_RGMII;
		if (dwmac->eth_clk_sel_reg)
			val |= SYSCFG_PMCR_ETH_CLK_SEL;
		pr_debug("SYSCFG init : PHY_INTERFACE_MODE_RGMII\n");
		break;
	default:
		dev_err(&eth_dev->netdev.dev, "bad phy mode %d\n",
			eth_dev->interface);
		return -EINVAL;
	}

	/* Need to update PMCCLRR (clear register) */
	ret = regmap_write(dwmac->regmap, reg + SYSCFG_PMCCLRR_OFFSET,
			   dwmac->ops->syscfg_eth_mask);

	/* Update PMCSETR (set register) */
	return regmap_update_bits(dwmac->regmap, reg,
				  dwmac->ops->syscfg_eth_mask, val);
}

static int stm32mcu_dwc_set_mode(struct stm32_dwc_dev *dwmac)
{
	struct dw_eth_dev *eth_dev = dwmac->priv;
	u32 reg = dwmac->mode_reg;
	int val;

	switch (eth_dev->interface) {
	case PHY_INTERFACE_MODE_MII:
		val = SYSCFG_MCU_ETH_SEL_MII;
		pr_debug("SYSCFG init : PHY_INTERFACE_MODE_MII\n");
		break;
	case PHY_INTERFACE_MODE_RMII:
		val = SYSCFG_MCU_ETH_SEL_RMII;
		pr_debug("SYSCFG init : PHY_INTERFACE_MODE_RMII\n");
		break;
	default:
		pr_debug("SYSCFG init :  Do not manage %d interface\n",
			 eth_dev->interface);
		/* Do not manage others interfaces */
		return -EINVAL;
	}

	return regmap_update_bits(dwmac->regmap, reg,
				 dwmac->ops->syscfg_eth_mask, val << 23);
}

static const struct clk_bulk_data stm32_clks[] = {
	{ .id = "stmmaceth" },
	{ .id = "mac-clk-rx" },
	{ .id = "mac-clk-tx" },
	{ .id = "syscfg-clk" },
};

static int stm32mp1_dwc_probe_dt(struct stm32_dwc_dev *dwmac, struct device_d *dev)
{
	struct device_node *np = dev->device_node;
	struct clk_bulk_data *clk;

	if (!IS_ENABLED(CONFIG_OFTREE))
		return -ENODEV;

	g_dev = dev;
	/* Gigabit Ethernet 125MHz clock selection. */
	dwmac->eth_clk_sel_reg = of_property_read_bool(np, "st,eth-clk-sel");

	/* Ethernet 50Mhz RMII clock selection */
	dwmac->eth_ref_clk_sel_reg =
		of_property_read_bool(np, "st,eth-ref-clk-sel");

	clk = &dwmac->clks[ARRAY_SIZE(stm32_clks)];
	dwmac->extra_clks = 2;

	clk->id = "syscfg-clk";
	clk->clk = clk_get(dev, clk->id);
	if (IS_ERR(clk->clk)) {
		dev_warn(dev, "No syscfg clock provided...\n");
		return PTR_ERR(clk->clk);
	}

	clk++;
	clk->id = "eth-ck";
	clk->clk = clk_get(dev, clk->id);
	if (IS_ERR(clk->clk)) {
		dev_warn(dev, "No phy clock provided...\n");
		dwmac->extra_clks--;
	}

	/* ethstp is only for suspending, which we don't do in barebox */

	return 0;
}

static int stm32_dwc_probe_dt(struct stm32_dwc_dev *dwmac, struct device_d *dev)
{
	unsigned total_max_clks;
	int ret;

	dwmac->regmap = syscon_regmap_lookup_by_phandle(dev->device_node, "st,syscon");
	if (IS_ERR(dwmac->regmap)) {
		dev_err(dev, "Could not get st,syscon node\n");
		return PTR_ERR(dwmac->regmap);
	}

	ret = of_property_read_u32_index(dev->device_node, "st,syscon",
					 1, &dwmac->mode_reg);
	if (ret) {
		dev_err(dev, "Can't get sysconfig mode offset (%s)\n",
			strerror(-ret));
		return -EINVAL;
	}

	total_max_clks = dwmac->ops->max_extra_clks + ARRAY_SIZE(stm32_clks);
	dwmac->clks = xmalloc(total_max_clks * sizeof(*dwmac->clks));
	memcpy(dwmac->clks, stm32_clks, sizeof stm32_clks);

	ret = clk_bulk_get(dev, ARRAY_SIZE(stm32_clks), dwmac->clks);
	if (ret) {
		dev_err(dev, "Failed to get clks: %s\n", strerror(-ret));
		return ret;
	}

	return 0;
}

static int stm32_dwc_ether_probe(struct device_d *dev)
{
	struct stm32_dwc_dev *dwmac;
	unsigned total_clks;
	int ret;

	dwmac = xzalloc(sizeof(*dwmac));

	ret = dev_get_drvdata(dev, (const void **)&dwmac->ops);
	if (ret)
		return ret;

	ret = stm32_dwc_probe_dt(dwmac, dev);
	if (ret) {
		dev_err(dev, "Failed to set probe device tree node: %s\n", strerror(-ret));
		return ret;
	}

	if (dwmac->ops->probe_dt) {
		ret = dwmac->ops->probe_dt(dwmac, dev);
		if (ret) {
			dev_err(dev, "Failed to set probe specialized device tree properties: %s\n", strerror(-ret));
			return ret;
		}
	}

	total_clks = ARRAY_SIZE(stm32_clks) + dwmac->extra_clks;
	ret = clk_bulk_enable(total_clks, dwmac->clks);
	if (ret) {
		dev_err(dev, "Failed to enable clks: %s\n", strerror(-ret));
		return ret;
	}


	dwmac->priv = dwc_drv_probe(dev, true);
	if (IS_ERR(dwmac->priv)) {
		ret = PTR_ERR(dwmac->priv);
		dev_err(dev, "Failed to probe generic DWC: %s\n", strerror(-ret));
		return ret;
	}

	if (dwmac->ops->set_mode) {
		ret = dwmac->ops->set_mode(dwmac);
		if (ret) {
			dev_err(dev, "Failed to set PHY mode: %s\n", strerror(-ret));
			return ret;
		}
	}

	dev_dbg(dev, "%u clks enabled\n", total_clks);

	return 0;
}

static struct stm32_ops stm32mcu_dwmac_data = {
	.set_mode = stm32mcu_dwc_set_mode,
	.syscfg_eth_mask = SYSCFG_MCU_ETH_MASK,
};

static struct stm32_ops stm32mp1_dwmac_data = {
	.set_mode = stm32mp1_dwc_set_mode,
	.probe_dt = stm32mp1_dwc_probe_dt,
	.syscfg_eth_mask = SYSCFG_MP1_ETH_MASK,
	.max_extra_clks = 2,
};

static __maybe_unused struct of_device_id stm32_dwc_ether_compatible[] = {
	{ .compatible = "st,stm32-dwmac", .data = &stm32mcu_dwmac_data},
	{ .compatible = "st,stm32mp1-dwmac", .data = &stm32mp1_dwmac_data},
	{ /* sentinel */ }
};

static struct driver_d stm32_dwc_ether_driver = {
	.name = "stm32_designware_eth",
	.probe = stm32_dwc_ether_probe,
	.remove	= dwc_drv_remove,
	.of_compatible = DRV_OF_COMPAT(stm32_dwc_ether_compatible),
};
device_platform_driver(stm32_dwc_ether_driver);

