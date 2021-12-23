/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019 Ahmad Fatoum, Pengutronix
 */

#ifndef __EQOS_H_
#define __EQOS_H_

#include "designware_common.h"

struct eqos;
struct eth_device;

struct eqos_desc;
struct eqos_dma_regs;
struct eqos_mac_regs;
struct eqos_mtl_regs;

struct eqos {
	struct dwc_common dwc;
	struct mii_bus miibus;

	u32 tx_currdescnum, rx_currdescnum;

	struct eqos_desc *tx_descs, *rx_descs;

	struct eqos_mac_regs __iomem *mac_regs;
	struct eqos_dma_regs __iomem *dma_regs;
	struct eqos_mtl_regs __iomem *mtl_regs;

	int phy_addr;
};

struct device;
int eqos_reset(struct eqos *priv);
struct eqos *eqos_probe(struct device *dev, const struct dwc_common_ops *ops, void *priv);
void eqos_remove(struct dwc_common *dwc);

int eqos_set_ethaddr(struct dwc_common *dwc, const unsigned char *mac);
void eqos_adjust_link(struct dwc_common *dwc, struct phy_device *phydev);

#endif
