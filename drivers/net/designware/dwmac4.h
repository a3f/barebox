/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 Ahmad Fatoum, Pengutronix
 */

#ifndef __DWMAC4_H_
#define __DWMAC4_H_

int eqos_get_ethaddr(struct eth_device *edev, unsigned char *mac);
int eqos_set_ethaddr(struct eth_device *edev, const unsigned char *mac);
int eqos_start(struct eth_device *edev);
void eqos_stop(struct eth_device *edev);
int eqos_send(struct eth_device *edev, void *packet, int length);
int eqos_recv(struct eth_device *edev);
int eqos_init(struct dw_eth_dev *eqos);
void eqos_adjust_link(struct eth_device *edev);

void eqos_remove_resources(struct device_d *dev); // FIXME removable?


#define MII_BUSY		(1 << 0)

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

struct eqos_config {
	int config_mac;
};

#define EQOS_MAC_MDIO_ADDRESS_CR_20_35			2
#define EQOS_MAC_MDIO_ADDRESS_CR_250_300		5
#define EQOS_MAC_MDIO_ADDRESS_SKAP			BIT(4)
#define EQOS_MAC_MDIO_ADDRESS_GOC_SHIFT			2
#define EQOS_MAC_MDIO_ADDRESS_GOC_READ			3
#define EQOS_MAC_MDIO_ADDRESS_GOC_WRITE			1
#define EQOS_MAC_MDIO_ADDRESS_C45E			BIT(1)

extern const struct mii_regs dwmac4_mii_regs;

#endif
