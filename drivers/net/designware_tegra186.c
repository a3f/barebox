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

#include "designware.h"
#include "dwmac.h"
#include "dwmac4.h"

#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_SHIFT			0
#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_MASK			3
#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_NOT_ENABLED		0
#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_DCB		2
#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_AV		1

#define EQOS_MAC_MDIO_ADDRESS_CR_SHIFT			8
#define EQOS_MAC_MDIO_ADDRESS_CR_20_35			2
#define EQOS_MAC_MDIO_ADDRESS_CR_250_300		5
#define EQOS_MAC_MDIO_ADDRESS_SKAP			BIT(4)
#define EQOS_MAC_MDIO_ADDRESS_GOC_SHIFT			2
#define EQOS_MAC_MDIO_ADDRESS_GOC_READ			3
#define EQOS_MAC_MDIO_ADDRESS_GOC_WRITE			1
#define EQOS_MAC_MDIO_ADDRESS_C45E			BIT(1)

struct eqos_dma_regs {
	uint32_t mode;					/* 0x1000 */
	uint32_t sysbus_mode;				/* 0x1004 */
	uint32_t unused_1008[(0x1100 - 0x1008) / 4];	/* 0x1008 */
	uint32_t ch0_control;				/* 0x1100 */
	uint32_t ch0_tx_control;			/* 0x1104 */
	uint32_t ch0_rx_control;			/* 0x1108 */
	uint32_t unused_110c;				/* 0x110c */
	uint32_t ch0_txdesc_list_haddress;		/* 0x1110 */
	uint32_t ch0_txdesc_list_address;		/* 0x1114 */
	uint32_t ch0_rxdesc_list_haddress;		/* 0x1118 */
	uint32_t ch0_rxdesc_list_address;		/* 0x111c */
	uint32_t ch0_txdesc_tail_pointer;		/* 0x1120 */
	uint32_t unused_1124;				/* 0x1124 */
	uint32_t ch0_rxdesc_tail_pointer;		/* 0x1128 */
	uint32_t ch0_txdesc_ring_length;		/* 0x112c */
	uint32_t ch0_rxdesc_ring_length;		/* 0x1130 */
};

#define EQOS_DMA_MODE_SWR				BIT(0)

#define EQOS_DMA_SYSBUS_MODE_RD_OSR_LMT_SHIFT		16
#define EQOS_DMA_SYSBUS_MODE_RD_OSR_LMT_MASK		0xf
#define EQOS_DMA_SYSBUS_MODE_EAME			BIT(11)
#define EQOS_DMA_SYSBUS_MODE_BLEN16			BIT(3)
#define EQOS_DMA_SYSBUS_MODE_BLEN8			BIT(2)
#define EQOS_DMA_SYSBUS_MODE_BLEN4			BIT(1)

#define EQOS_DMA_CH0_CONTROL_PBLX8			BIT(16)

#define EQOS_DMA_CH0_TX_CONTROL_TXPBL_SHIFT		16
#define EQOS_DMA_CH0_TX_CONTROL_TXPBL_MASK		0x3f
#define EQOS_DMA_CH0_TX_CONTROL_OSP			BIT(4)
#define EQOS_DMA_CH0_TX_CONTROL_ST			BIT(0)

#define EQOS_DMA_CH0_RX_CONTROL_RXPBL_SHIFT		16
#define EQOS_DMA_CH0_RX_CONTROL_RXPBL_MASK		0x3f
#define EQOS_DMA_CH0_RX_CONTROL_RBSZ_SHIFT		1
#define EQOS_DMA_CH0_RX_CONTROL_RBSZ_MASK		0x3fff
#define EQOS_DMA_CH0_RX_CONTROL_SR			BIT(0)

/* These registers are Tegra186-specific */
#define EQOS_TEGRA186_REGS_BASE 0x8800
struct eqos_tegra186_regs {
	uint32_t sdmemcomppadctrl;			/* 0x8800 */
	uint32_t auto_cal_config;			/* 0x8804 */
	uint32_t unused_8808;				/* 0x8808 */
	uint32_t auto_cal_status;			/* 0x880c */
};

#define EQOS_SDMEMCOMPPADCTRL_PAD_E_INPUT_OR_E_PWRD	BIT(31)

#define EQOS_AUTO_CAL_CONFIG_START			BIT(31)
#define EQOS_AUTO_CAL_CONFIG_ENABLE			BIT(29)

#define EQOS_AUTO_CAL_STATUS_ACTIVE			BIT(31)

/* Descriptors */

struct eqos_config {
	int mdio_wait;
	int config_mac;
	int config_mac_mdio;
	struct eqos_ops *ops;
};

struct dw_eth_dev;
struct eqos_ops { // FIXME get rid of?
	int (*probe_resources)(struct device_d *);
	void (*remove_resources)(struct device_d *);
	int (*calibrate_link)(struct dw_eth_dev *, unsigned speed);
	unsigned long (*get_tick_clk_rate)(struct dw_eth_dev *);
};

enum { CLK_SLAVE_BUS, CLK_MASTER_BUS, CLK_RX, CLK_PTP_REF, CLK_TX };
static const struct clk_bulk_data tegra186_clks[] = {
	[CLK_SLAVE_BUS]  = { .id = "slave_bus" },
	[CLK_MASTER_BUS] = { .id = "master_bus" },
	[CLK_RX]         = { .id = "rx" },
	[CLK_PTP_REF]    = { .id = "ptp_ref" },
	[CLK_TX]         = { .id = "tx" },
};

static int eqos_clks_set_rate_tegra186(struct dw_eth_dev *eqos)
{
	int ret;

	ret = clk_set_rate(eqos->clks[CLK_PTP_REF].clk, 125 * 1000 * 1000);
	if (ret < 0)
		pr_err("clk_set_rate(clk_ptp_ref) failed: %d\n", ret);

	return ret;
}

static int eqos_reset_tegra186(struct dw_eth_dev *eqos, int reset)
{
	int ret;

	if (reset > 0) {
		reset_control_assert(eqos->reset_ctl);
		gpio_set_value(eqos->phy_reset_gpio, 1);
		return 0;
	}

	gpio_set_value(eqos->phy_reset_gpio, 1);

	udelay(2);

	gpio_set_value(eqos->phy_reset_gpio, 0);

	ret = reset_control_assert(eqos->reset_ctl);
	if (ret < 0) {
		pr_err("reset_assert() failed: %s\n", strerror(-ret));
		return ret;
	}

	udelay(2);

	ret = reset_control_deassert(eqos->reset_ctl);
	if (ret < 0) {
		pr_err("reset_deassert() failed: %s\n", strerror(-ret));
		return ret;
	}

	return 0;
}

static int eqos_calibrate_pads_tegra186(struct dw_eth_dev *eqos)
{
	u32 active;
	int ret;

	setbits_le32(&eqos->tegra186_regs->sdmemcomppadctrl,
		     EQOS_SDMEMCOMPPADCTRL_PAD_E_INPUT_OR_E_PWRD);

	udelay(1);

	setbits_le32(&eqos->tegra186_regs->auto_cal_config,
		     EQOS_AUTO_CAL_CONFIG_START | EQOS_AUTO_CAL_CONFIG_ENABLE);

	ret = readl_poll_timeout(&eqos->tegra186_regs->auto_cal_status, active,
				 active & EQOS_AUTO_CAL_STATUS_ACTIVE,
				 10000);
	if (ret) {
		pr_err("calibrate didn't start\n");
		goto failed;
	}

	ret = readl_poll_timeout(&eqos->tegra186_regs->auto_cal_status, active,
				 !(active & EQOS_AUTO_CAL_STATUS_ACTIVE),
				 10000);
	if (ret) {
		pr_err("calibrate didn't finish\n");
		goto failed;
	}

failed:
	clrbits_le32(&eqos->tegra186_regs->sdmemcomppadctrl,
		     EQOS_SDMEMCOMPPADCTRL_PAD_E_INPUT_OR_E_PWRD);

	return ret;
}

static int eqos_calibrate_link_tegra186(struct dw_eth_dev *eqos, unsigned speed)
{
	int ret = 0;
	unsigned long rate;
	bool calibrate;

	switch (speed) {
	case SPEED_1000:
		rate = 125 * 1000 * 1000;
		calibrate = true;
		break;
	case SPEED_100:
		rate = 25 * 1000 * 1000;
		calibrate = true;
		break;
	case SPEED_10:
		rate = 2.5 * 1000 * 1000;
		calibrate = false;
		break;
	default:
		return -EINVAL;
	}

	if (calibrate) {
		ret = eqos_calibrate_pads_tegra186(eqos);
		if (ret)
			return ret;
	} else {
		clrbits_le32(&eqos->tegra186_regs->auto_cal_config,
			     EQOS_AUTO_CAL_CONFIG_ENABLE);
	}

	ret = clk_set_rate(eqos->clks[CLK_TX].clk, rate);
	if (ret < 0) {
		pr_err("clk_set_rate(tx_clk, %lu) failed: %d\n", rate, ret);
		return ret;
	}

	return 0;
}

static unsigned long eqos_get_tick_clk_rate_tegra186(struct dw_eth_dev *eqos)
{
	return clk_get_rate(eqos->clks[CLK_SLAVE_BUS].clk);
}

static int eqos_set_ethaddr_tegra186(struct eth_device *edev, const unsigned char *mac)
{
	struct dw_eth_dev *eqos = edev->priv;

	/*
	 * This function may be called before start() or after stop(). At that
	 * time, on at least some configurations of the EQoS HW, all clocks to
	 * the EQoS HW block will be stopped, and a reset signal applied. If
	 * any register access is attempted in this state, bus timeouts or CPU
	 * hangs may occur. This check prevents that.
	 *
	 * A simple solution to this problem would be to not implement
	 * write_hwaddr(), since start() always writes the MAC address into HW
	 * anyway. However, it is desirable to implement write_hwaddr() to
	 * support the case of SW that runs subsequent to U-Boot which expects
	 * the MAC address to already be programmed into the EQoS registers,
	 * which must happen irrespective of whether the U-Boot user (or
	 * scripts) actually made use of the EQoS device, and hence
	 * irrespective of whether start() was ever called.
	 *
	 * Note that this requirement by subsequent SW is not valid for
	 * Tegra186, and is likely not valid for any non-PCI instantiation of
	 * the EQoS HW block. This function is implemented solely as
	 * future-proofing with the expectation the driver will eventually be
	 * ported to some system where the expectation above is true.
	 */

	if (eqos->defer_reg_access) {
		memcpy(eqos->macaddr, mac, 6);
		return 0;
	}

	return eqos_set_ethaddr(edev, mac);
}

static int eqos_probe_resources_tegra186(struct device_d *dev)
{
	struct dw_eth_dev *eqos = dev->priv;
	int phy_reset;
	int ret;

	eqos->reset_ctl = reset_control_get(dev, "eqos");
	if (IS_ERR(eqos->reset_ctl)) {
		ret = PTR_ERR(eqos->reset_ctl);
		pr_err("reset_get_by_name(rst) failed: %s\n", strerror(-ret));
		return ret;
	}

	phy_reset = of_get_named_gpio(dev->device_node, "phy-reset-gpios", 0);
	if (gpio_is_valid(phy_reset)) {
		ret = gpio_request(phy_reset, "phy-reset");
		if (ret)
			goto release_res;

		eqos->phy_reset_gpio = phy_reset;
	}

	eqos->clks = xmemdup(tegra186_clks, sizeof(tegra186_clks));
	eqos->num_clks = ARRAY_SIZE(tegra186_clks);
	ret = clk_bulk_get(dev, eqos->num_clks, eqos->clks);
	if (ret) {
		dev_err(dev, "Failed to get clks: %s\n", strerror(-ret));
		return ret;
	}

	return 0;

release_res:
	reset_control_put(eqos->reset_ctl);
	return ret;
}

static void eqos_remove_resources_tegra186(struct device_d *dev)
{
	struct dw_eth_dev *eqos = dev->priv;

	gpio_free(eqos->phy_reset_gpio);
	reset_control_put(eqos->reset_ctl);
}

static int eqos_init_tegra186(struct dw_eth_dev *eqos)
{
	eqos->tegra186_regs = IOMEM(eqos->regs + EQOS_TEGRA186_REGS_BASE);
	eqos->defer_reg_access = true;

	return eqos_init(eqos);
}

static int eqos_start_tegra186(struct eth_device *edev)
{
	struct dw_eth_dev *eqos = edev->priv;

	eqos->defer_reg_access = false;
	return eqos_start(edev);
}

static void eqos_stop_tegra186(struct eth_device *edev)
{
	struct dw_eth_dev *eqos = edev->priv;

	eqos->defer_reg_access = true;
	eqos_stop(edev);
}


static struct dw_eth_ops dwmac4_ops = {
	.get_ethaddr = eqos_get_ethaddr,
	.set_ethaddr = eqos_set_ethaddr_tegra186,
	.start = eqos_start_tegra186,
	.halt = eqos_stop_tegra186,
	.send = eqos_send,
	.rx = eqos_recv,
	.init = eqos_init_tegra186,
	.adjust_link = eqos_adjust_link,
	.reset = eqos_reset_tegra186,
	.clks_set_rate = eqos_clks_set_rate_tegra186,

	.enh_desc = 1, /* FIXME */
	.clk_csr_shift = 8,
};

static int eqos_probe_tegra186(struct device_d *dev)
{
	struct dw_eth_dev *dwc = dwmac_drv_probe(dev, &dwmac4_ops);

	return PTR_ERR_OR_ZERO(dwc);
}

static void eqos_remove_tegra186(struct device_d *dev)
{
	struct dw_eth_dev *eqos = dev->priv;
	mdiobus_unregister(&eqos->miibus);
	eqos_remove_resources(dev);

	dwmac_drv_remove(dev);
}

static struct eqos_ops eqos_tegra186_ops = {
	.probe_resources = eqos_probe_resources_tegra186,
	.remove_resources = eqos_remove_resources_tegra186,
	.calibrate_link = eqos_calibrate_link_tegra186,
	.get_tick_clk_rate = eqos_get_tick_clk_rate_tegra186
};

static const struct eqos_config eqos_tegra186_config = {
	.mdio_wait = 10,
	.config_mac = EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_DCB,
	.config_mac_mdio = EQOS_MAC_MDIO_ADDRESS_CR_20_35,
	.ops = &eqos_tegra186_ops
};

static const struct of_device_id eqos_ids[] = {
	{ .compatible = "nvidia,tegra186-eqos", .data = &eqos_tegra186_config },
	{ /* sentinel */ }
};

static struct driver_d tegra_dwc_ether_driver = {
	.name = "eth_eqos", // FIXME rename
	.probe = eqos_probe_tegra186,
	.remove	= eqos_remove_tegra186,
	.of_compatible = DRV_OF_COMPAT(eqos_ids),
};
device_platform_driver(tegra_dwc_ether_driver);
