// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016, NVIDIA CORPORATION.
 * Copyright (c) 2019, Ahmad Fatoum, Pengutronix
 *
 * Portions based on U-Boot's rtl8169.c and dwc_eth_qos.
 */

#include <common.h>
#include <init.h>
#include <dma.h>
#include <io.h>
#include <gpio.h>
#include <of_gpio.h>
#include <net.h>
#include <linux/clk.h>
#include <of_net.h>
#include <linux/reset.h>
#include <linux/iopoll.h>
#include <mfd/syscon.h>
#include <linux/mii.h>

#include "dwmac.h"
#include "dwmac4.h"

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

/* Descriptors */

#define SYSCFG_MCU_ETH_MASK		BIT(23)
#define SYSCFG_MP1_ETH_MASK		GENMASK(23, 16)
#define SYSCFG_PMCCLRR_OFFSET		0x40

enum { CLK_STMMACETH, CLK_MAX_RX, CLK_MAX_TX, CLK_SYSCFG,  };
static const struct clk_bulk_data stm32_clks[] = {
	[CLK_STMMACETH] = { .id = "stmmaceth" },
	[CLK_MAX_RX]    = { .id = "mac-clk-rx" },
	[CLK_MAX_TX]    = { .id = "mac-clk-tx" },
	[CLK_SYSCFG]    = { .id = "syscfg-clk" },
};

static unsigned long eqos_get_tick_clk_rate_stm32(struct dw_eth_dev *eqos)
{
	return clk_get_rate(eqos->clks[CLK_STMMACETH].clk);
}

static void eqos_set_mode_stm32(struct dw_eth_dev *eqos)
{
	u32 val, reg = eqos->mode_reg;
	int ret;

	switch (eqos->interface) {
	case PHY_INTERFACE_MODE_MII:
		val = SYSCFG_PMCR_ETH_SEL_MII;
		pr_debug("SYSCFG init : PHY_INTERFACE_MODE_MII\n");
		break;
	case PHY_INTERFACE_MODE_GMII:
		val = SYSCFG_PMCR_ETH_SEL_GMII;
		if (eqos->eth_clk_sel_reg)
			val |= SYSCFG_PMCR_ETH_CLK_SEL;
		pr_debug("SYSCFG init : PHY_INTERFACE_MODE_GMII\n");
		break;
	case PHY_INTERFACE_MODE_RMII:
		val = SYSCFG_PMCR_ETH_SEL_RMII;
		if (eqos->eth_ref_clk_sel_reg)
			val |= SYSCFG_PMCR_ETH_REF_CLK_SEL;
		pr_debug("SYSCFG init : PHY_INTERFACE_MODE_RMII\n");
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
		val = SYSCFG_PMCR_ETH_SEL_RGMII;
		if (eqos->eth_clk_sel_reg)
			val |= SYSCFG_PMCR_ETH_CLK_SEL;
		pr_debug("SYSCFG init : PHY_INTERFACE_MODE_RGMII\n");
		break;
	default:
		pr_err("bad phy mode %d\n", eqos->interface);
		return;
	}

	/* Need to update PMCCLRR (clear register) */
	ret = regmap_write(eqos->regmap, reg + SYSCFG_PMCCLRR_OFFSET,
			   SYSCFG_MP1_ETH_MASK);
	if (ret)
		return;

	/* Update PMCSETR (set register) */
	regmap_update_bits(eqos->regmap, reg,
			   GENMASK(23, 16), val); // BIT(32) for MCU..
	return;
}

static int eqos_init_stm32(struct dw_eth_dev *eqos)
{
	struct device_d *dev = eqos->netdev.parent;
	struct device_node *np = dev->device_node;
	struct clk_bulk_data *eth_ck;
	int ret;

	ret = eqos_init(eqos);
	if (ret)
		return ret;

	/* Gigabit Ethernet 125MHz clock selection. */
	eqos->eth_clk_sel_reg = of_property_read_bool(np, "st,eth-clk-sel");

	/* Ethernet 50Mhz RMII clock selection */
	eqos->eth_ref_clk_sel_reg =
		of_property_read_bool(np, "st,eth-ref-clk-sel");

	eqos->regmap = syscon_regmap_lookup_by_phandle(dev->device_node,
						       "st,syscon");
	if (IS_ERR(eqos->regmap)) {
		dev_err(dev, "Could not get st,syscon node\n");
		return PTR_ERR(eqos->regmap);
	}

	ret = of_property_read_u32_index(dev->device_node, "st,syscon",
					 1, &eqos->mode_reg);
	if (ret) {
		dev_err(dev, "Can't get sysconfig mode offset (%s)\n",
			strerror(-ret));
		return -EINVAL;
	}

	eqos_set_mode_stm32(eqos);

	eqos->num_clks = ARRAY_SIZE(stm32_clks) + 1;
	eqos->clks = xmalloc(eqos->num_clks * sizeof(*eqos->clks));
	memcpy(eqos->clks, stm32_clks, sizeof stm32_clks);

	ret = clk_bulk_get(dev, ARRAY_SIZE(stm32_clks), eqos->clks);
	if (ret) {
		dev_err(dev, "Failed to get clks: %s\n", strerror(-ret));
		return ret;
	}

	eth_ck = &eqos->clks[ARRAY_SIZE(stm32_clks)];
	eth_ck->id = "eth-ck";
	eth_ck->clk = clk_get(dev, eth_ck->id);
	if (IS_ERR(eth_ck->clk)) {
		eqos->num_clks--;
		pr_debug("No phy clock provided. Continuing without.\n");
	}

	return 0;

}

// todo split!
static struct dw_eth_ops dwmac4_ops = {
	.get_ethaddr = eqos_get_ethaddr,
	.set_ethaddr = eqos_set_ethaddr,
	.start = eqos_start,
	.halt = eqos_stop,
	.send = eqos_send,
	.rx = eqos_recv,
	.init = eqos_init_stm32,
	.adjust_link = eqos_adjust_link,
	.get_tick_clk_rate = eqos_get_tick_clk_rate_stm32,

	.has_gmac4 = 1, /* FIXME */
	.mdio_wait = 10000,
	.mii = &dwmac4_mii_regs,
	.clk_csr = EQOS_MAC_MDIO_ADDRESS_CR_250_300,
};

static int eqos_probe_stm32(struct device_d *dev)
{
	struct dw_eth_dev *dwc = dwmac_drv_probe(dev, &dwmac4_ops);

	return PTR_ERR_OR_ZERO(dwc);
}

static void eqos_remove_stm32(struct device_d *dev)
{
	struct dw_eth_dev *eqos = dev->priv;
	mdiobus_unregister(&eqos->miibus);
	eqos_remove_resources(dev);

	dwmac_drv_remove(dev);
}

static const struct eqos_config eqos_stm32_config = {
	.config_mac = EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_AV,
};

static const struct of_device_id eqos_ids[] = {
	{ .compatible = "st,stm32mp1-dwmac", .data = &eqos_stm32_config },
	{ /* sentinel */ }
};

static struct driver_d stm32_dwc_ether_driver = {
	.name = "eth_eqos", // FIXME rename
	.probe = eqos_probe_stm32,
	.remove	= eqos_remove_stm32,
	.of_compatible = DRV_OF_COMPAT(eqos_ids),
};
device_platform_driver(stm32_dwc_ether_driver);
