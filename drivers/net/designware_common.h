/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 Ahmad Fatoum, Pengutronix
 */

#ifndef __DESIGNWARE_COMMON_H_
#define __DESIGNWARE_COMMON_H_

#include <net.h>

struct dwc_common;

struct dwc_common_ops {
	int (*init)(struct device *dev, struct dwc_common *priv);
	int (*get_ethaddr)(struct eth_device *dev, unsigned char *mac);
	int (*set_ethaddr)(struct eth_device *edev, const unsigned char *mac);
	void (*adjust_link)(struct eth_device *edev);
	unsigned long (*get_csr_clk_rate)(struct dwc_common *);

	bool enh_desc;
#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_SHIFT		0
#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_MASK		3
#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_NOT_ENABLED	0
#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_DCB	2
#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_AV	1
	unsigned clk_csr;
#define EQOS_MDIO_ADDR_CR_20_35			2
#define EQOS_MDIO_ADDR_CR_250_300		5
#define EQOS_MDIO_ADDR_SKAP			BIT(4)
#define EQOS_MDIO_ADDR_GOC_SHIFT		2
#define EQOS_MDIO_ADDR_GOC_READ			3
#define EQOS_MDIO_ADDR_GOC_WRITE		1
#define EQOS_MDIO_ADDR_C45E			BIT(1)
	unsigned config_mac;
};

struct dwc_common {
	struct eth_device netdev;
	const struct dwc_common_ops *ops;
	void *priv;
	u8 macaddr[6];
	void __iomem *regs;
	phy_interface_t interface;

	bool started;
	bool eqos;
};

int dwc_common_probe(struct device *dev, const struct dwc_common_ops *ops, void *priv);
void dwc_common_remove(struct device *dev);

int dwc_common_get_ethaddr(struct eth_device *edev, unsigned char *mac);
int dwc_common_set_ethaddr(struct eth_device *edev, const unsigned char *mac);
void dwc_common_adjust_link(struct eth_device *edev);


#define dwc_dbg(dwc, ...) dev_dbg(&(dwc)->netdev.dev, __VA_ARGS__)
#define dwc_warn(dwc, ...) dev_warn(&(dwc)->netdev.dev, __VA_ARGS__)
#define dwc_err(dwc, ...) dev_err(&(dwc)->netdev.dev, __VA_ARGS__)

#endif
