/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * (C) Copyright 2010
 * Vipin Kumar, ST Micoelectronics, vipin.kumar@st.com.
 */

#ifndef __DESIGNWARE_ETH_H
#define __DESIGNWARE_ETH_H

#include <net.h>

struct dw_eth_dev {
	struct eth_device netdev;
	struct mii_bus miibus;

	void (*fix_mac_speed)(int speed);
	u8 macaddr[6];
	u32 tx_currdescnum;
	u32 rx_currdescnum;

	struct dmamacdescr *tx_mac_descrtable;
	struct dmamacdescr *rx_mac_descrtable;

	u8 *txbuffs;
	u8 *rxbuffs;

	void __iomem *mac_regs_p;
	struct eth_dma_regs *dma_regs_p;
	int phy_addr;
	phy_interface_t interface;
	int enh_desc;

	struct reset_control	*rst;
};

struct dw_eth_dev *dwc_drv_probe(struct device_d *dev, bool enh_desc);
void dwc_drv_remove(struct device_d *dev);

#define CONFIG_TX_DESCR_NUM	16
#define CONFIG_RX_DESCR_NUM	16
#define CONFIG_ETH_BUFSIZE	2048
#define TX_TOTAL_BUFSIZE	(CONFIG_ETH_BUFSIZE * CONFIG_TX_DESCR_NUM)
#define RX_TOTAL_BUFSIZE	(CONFIG_ETH_BUFSIZE * CONFIG_RX_DESCR_NUM)

/*  MAC registers */
#define GMAC_CONFIG			0x00000000
#define GMAC_PACKET_FILTER		0x00000008
#define GMAC_HASH_TAB_0_31		0x00000010
#define GMAC_HASH_TAB_32_63		0x00000014
#define GMAC_RX_FLOW_CTRL		0x00000090
#define GMAC_QX_TX_FLOW_CTRL(x)		(0x70 + x * 4)
#define GMAC_TXQ_PRTY_MAP0		0x98
#define GMAC_TXQ_PRTY_MAP1		0x9C
#define GMAC_RXQ_CTRL0			0x000000a0
#define GMAC_RXQ_CTRL1			0x000000a4
#define GMAC_RXQ_CTRL2			0x000000a8
#define GMAC_RXQ_CTRL3			0x000000ac
#define GMAC_INT_STATUS			0x000000b0
#define GMAC_INT_EN			0x000000b4
#define GMAC_1US_TIC_COUNTER		0x000000dc
#define GMAC_PCS_BASE			0x000000e0
#define GMAC_PHYIF_CONTROL_STATUS	0x000000f8
#define GMAC_PMT			0x000000c0
#define GMAC_DEBUG			0x00000114
#define GMAC_HW_FEATURE0		0x0000011c
#define GMAC_HW_FEATURE1		0x00000120
#define GMAC_HW_FEATURE2		0x00000124
#define GMAC_HW_FEATURE3		0x00000128
#define GMAC_MDIO_ADDR			0x00000200
#define GMAC_MDIO_DATA			0x00000204
#define GMAC_ADDR_HIGH(reg)		(0x300 + reg * 8)
#define GMAC_ADDR_LOW(reg)		(0x304 + reg * 8)

/* MAC HW ADDR regs */
#define GMAC_HI_DCS			GENMASK(18, 16)
#define GMAC_HI_DCS_SHIFT		16
#define GMAC_HI_REG_AE			BIT(31)



/* MAC configuration register definitions */
#define FRAMEBURSTENABLE	(1 << 21)
#define MII_PORTSELECT		(1 << 15)
#define FES_100			(1 << 14)
#define DISABLERXOWN		(1 << 13)
#define FULLDPLXMODE		(1 << 11)
#define RXENABLE		(1 << 2)
#define TXENABLE		(1 << 3)

/* MII address register definitions */
#define MII_BUSY		(1 << 0)
#define MII_WRITE		(1 << 1)
#define MII_CLKRANGE_60_100M	(0)
#define MII_CLKRANGE_100_150M	(0x4)
#define MII_CLKRANGE_20_35M	(0x8)
#define MII_CLKRANGE_35_60M	(0xC)
#define MII_CLKRANGE_150_250M	(0x10)
#define MII_CLKRANGE_250_300M	(0x14)

#define MIIADDRSHIFT		(11)
#define MIIREGSHIFT		(6)
#define MII_REGMSK		(0x1F << 6)
#define MII_ADDRMSK		(0x1F << 11)


struct eth_dma_regs {
	u32 busmode;		/* 0x00 */
	u32 txpolldemand;	/* 0x04 */
	u32 rxpolldemand;	/* 0x08 */
	u32 rxdesclistaddr;	/* 0x0c */
	u32 txdesclistaddr;	/* 0x10 */
	u32 status;		/* 0x14 */
	u32 opmode;		/* 0x18 */
	u32 intenable;		/* 0x1c */
	u8 reserved[40];
	u32 currhosttxdesc;	/* 0x48 */
	u32 currhostrxdesc;	/* 0x4c */
	u32 currhosttxbuffaddr;	/* 0x50 */
	u32 currhostrxbuffaddr;	/* 0x54 */
};

#define DW_DMA_BASE_OFFSET	(0x1000)

/* Bus mode register definitions */
#define FIXEDBURST		(1 << 16)
#define PRIORXTX_41		(3 << 14)
#define PRIORXTX_31		(2 << 14)
#define PRIORXTX_21		(1 << 14)
#define PRIORXTX_11		(0 << 14)
#define BURST_1			(1 << 8)
#define BURST_2			(2 << 8)
#define BURST_4			(4 << 8)
#define BURST_8			(8 << 8)
#define BURST_16		(16 << 8)
#define BURST_32		(32 << 8)
#define RXHIGHPRIO		(1 << 1)
#define DMAMAC_SRST		(1 << 0)

/* Poll demand definitions */
#define POLL_DATA		(0xFFFFFFFF)

/* Operation mode definitions */
#define STOREFORWARD		(1 << 21)
#define FLUSHTXFIFO		(1 << 20)
#define TXSTART			(1 << 13)
#define TXSECONDFRAME		(1 << 2)
#define RXSTART			(1 << 1)

/* Descriptior related definitions */
#define MAC_MAX_FRAME_SZ	(1600)

struct dmamacdescr {
	u32 txrx_status;
	u32 dmamac_cntl;
	void *dmamac_addr;
	struct dmamacdescr *dmamac_next;
};

/*
 * txrx_status definitions
 */

/* tx status bits definitions */
#define DESC_ENH_TXSTS_OWNBYDMA		(1 << 31)
#define DESC_ENH_TXSTS_TXINT		(1 << 30)
#define DESC_ENH_TXSTS_TXLAST		(1 << 29)
#define DESC_ENH_TXSTS_TXFIRST		(1 << 28)
#define DESC_ENH_TXSTS_TXCRCDIS		(1 << 27)

#define DESC_ENH_TXSTS_TXPADDIS		(1 << 26)
#define DESC_ENH_TXSTS_TXCHECKINSCTRL	(3 << 22)
#define DESC_ENH_TXSTS_TXRINGEND		(1 << 21)
#define DESC_ENH_TXSTS_TXCHAIN		(1 << 20)
#define DESC_ENH_TXSTS_MSK			(0x1FFFF << 0)

#define DESC_TXSTS_OWNBYDMA		(1 << 31)
#define DESC_TXSTS_MSK			(0x1FFFF << 0)

/* rx status bits definitions */
#define DESC_RXSTS_OWNBYDMA		(1 << 31)
#define DESC_RXSTS_DAFILTERFAIL		(1 << 30)
#define DESC_RXSTS_FRMLENMSK		(0x3FFF << 16)
#define DESC_RXSTS_FRMLENSHFT		(16)

#define DESC_RXSTS_ERROR		(1 << 15)
#define DESC_RXSTS_RXTRUNCATED		(1 << 14)
#define DESC_RXSTS_SAFILTERFAIL		(1 << 13)
#define DESC_RXSTS_RXIPC_GIANTFRAME	(1 << 12)
#define DESC_RXSTS_RXDAMAGED		(1 << 11)
#define DESC_RXSTS_RXVLANTAG		(1 << 10)
#define DESC_RXSTS_RXFIRST		(1 << 9)
#define DESC_RXSTS_RXLAST		(1 << 8)
#define DESC_RXSTS_RXIPC_GIANT		(1 << 7)
#define DESC_RXSTS_RXCOLLISION		(1 << 6)
#define DESC_RXSTS_RXFRAMEETHER		(1 << 5)
#define DESC_RXSTS_RXWATCHDOG		(1 << 4)
#define DESC_RXSTS_RXMIIERROR		(1 << 3)
#define DESC_RXSTS_RXDRIBBLING		(1 << 2)
#define DESC_RXSTS_RXCRC		(1 << 1)

/*
 * dmamac_cntl definitions
 */

/* tx control bits definitions */
#define DESC_ENH_TXCTRL_SIZE1MASK		(0x1FFF << 0)
#define DESC_ENH_TXCTRL_SIZE1SHFT		(0)
#define DESC_ENH_TXCTRL_SIZE2MASK		(0x1FFF << 16)
#define DESC_ENH_TXCTRL_SIZE2SHFT		(16)

#define DESC_TXCTRL_TXINT		(1 << 31)
#define DESC_TXCTRL_TXLAST		(1 << 30)
#define DESC_TXCTRL_TXFIRST		(1 << 29)
#define DESC_TXCTRL_TXCHECKINSCTRL	(3 << 27)
#define DESC_TXCTRL_TXCRCDIS		(1 << 26)
#define DESC_TXCTRL_TXRINGEND		(1 << 25)
#define DESC_TXCTRL_TXCHAIN		(1 << 24)

#define DESC_TXCTRL_SIZE1MASK		(0x7FF << 0)
#define DESC_TXCTRL_SIZE1SHFT		(0)
#define DESC_TXCTRL_SIZE2MASK		(0x7FF << 11)
#define DESC_TXCTRL_SIZE2SHFT		(11)

/* rx control bits definitions */
#define DESC_ENH_RXCTRL_RXINTDIS		(1 << 31)
#define DESC_ENH_RXCTRL_RXRINGEND		(1 << 15)
#define DESC_ENH_RXCTRL_RXCHAIN		(1 << 14)

#define DESC_ENH_RXCTRL_SIZE1MASK		(0x1FFF << 0)
#define DESC_ENH_RXCTRL_SIZE1SHFT		(0)
#define DESC_ENH_RXCTRL_SIZE2MASK		(0x1FFF << 16)
#define DESC_ENH_RXCTRL_SIZE2SHFT		(16)

#define DESC_RXCTRL_RXINTDIS		(1 << 31)
#define DESC_RXCTRL_RXRINGEND		(1 << 25)
#define DESC_RXCTRL_RXCHAIN		(1 << 24)

#define DESC_RXCTRL_SIZE1MASK		(0x7FF << 0)
#define DESC_RXCTRL_SIZE1SHFT		(0)
#define DESC_RXCTRL_SIZE2MASK		(0x7FF << 11)
#define DESC_RXCTRL_SIZE2SHFT		(11)

#endif
