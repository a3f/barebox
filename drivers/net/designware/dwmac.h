/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) Copyright 2019 Ahmad Fatoum
 */

#ifndef __DESIGNWARE_DWMAC_H
#define __DESIGNWARE_DWMAC_H

struct mii_regs {
	unsigned int addr;		 /* MII Address */
	unsigned int data;		 /* MII Data */
	unsigned int addr_shift;	/* MII address shift */
	unsigned int reg_shift;		/* MII reg shift */
	unsigned int addr_mask;		/* MII address mask */
	unsigned int reg_mask;		/* MII reg mask */
	unsigned int clk_csr_shift;
	unsigned int clk_csr_mask;
};

struct dw_eth_dev;

struct dw_eth_ops {
	void (*adjust_link)(struct eth_device *edev);
	int (*get_ethaddr)(struct eth_device *dev, unsigned char *mac);
	int (*set_ethaddr)(struct eth_device *edev, const unsigned char *mac);
	int (*start)(struct eth_device *edev);
	void (*halt)(struct eth_device *dev);
	int (*send)(struct eth_device *dev, void *packet, int length);
	int (*rx)(struct eth_device *edev);
	int (*init)(struct dw_eth_dev *priv);
	int (*reset)(struct dw_eth_dev *, int reset);
	int (*clks_set_rate)(struct dw_eth_dev *);
	unsigned long (*get_tick_clk_rate)(struct dw_eth_dev *);

	bool enh_desc;
	int mdio_wait;
	int has_gmac4;
	struct mii_regs *mii;
	unsigned clk_csr;
};

struct eth_mac_regs;
struct eqos_mac_regs;
struct eth_dma_regs;
struct eqos_dma_regs;
struct eqos_mtl_regs;
struct reset_control;
struct eqos_config;
struct eqos_tegra186_regs;
struct reset_control;
struct regmap;
struct eqos_desc;
struct clk_bulk_data;
struct dmamacdescr;

struct dw_eth_dev {
	struct eth_device netdev;
	struct mii_bus miibus;

	void (*fix_mac_speed)(int speed);
	u8 macaddr[6];
	u32 tx_currdescnum;
	u32 rx_currdescnum;

	union {
		struct dmamacdescr *tx_mac_descrtable;
		struct eqos_desc *tx_descs;
	};

	union {
		struct dmamacdescr *rx_mac_descrtable;
		struct eqos_desc *rx_descs;
	};

	u8 *txbuffs;
	u8 *rxbuffs;

	void __iomem *regs;

	union {
		struct eth_mac_regs *mac_regs_p;
		struct eqos_mac_regs *mac_regs;
	};
	union {
		struct eth_dma_regs *dma_regs_p;
		struct eqos_dma_regs *dma_regs;
	};
	struct eqos_mtl_regs *mtl_regs;
	int phy_addr;
	phy_interface_t interface;

	struct reset_control	*rst;

	const struct eqos_config *config;
	struct eqos_tegra186_regs *tegra186_regs;
	struct regmap *regmap;
	u32 mode_reg;
	int phy_reset_gpio;
	unsigned phy_interface;
	void *descs;
	struct clk_bulk_data *clks;
	int num_clks;
	int eth_clk_sel_reg;
	int eth_ref_clk_sel_reg;

	struct dw_eth_ops *ops;
	bool defer_reg_access;
	unsigned mii_addr_shift;
	unsigned mii_addr_mask;
	unsigned mii_reg_shift;
	unsigned mii_reg_mask;
};


struct dw_eth_dev *dwmac_drv_probe(struct device_d *dev, struct dw_eth_ops *ops);
void dwmac_drv_remove(struct device_d *dev);
int dwmac_reset(struct dw_eth_dev *priv);

#define DMAMAC_SRST		(1 << 0)

#define DW_DMA_BASE_OFFSET	(0x1000)

#endif
