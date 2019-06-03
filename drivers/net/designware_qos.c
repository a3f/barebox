// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016, NVIDIA CORPORATION.
 *
 * Portions based on U-Boot's rtl8169.c.
 */
#define DEBUG 1

/*
 * This driver supports the Synopsys Designware Ethernet QOS (Quality Of
 * Service) IP block. The IP supports multiple options for bus type, clocking/
 * reset structure, and feature list.
 *
 * The driver is written such that generic core logic is kept separate from
 * configuration-specific logic. Code that interacts with configuration-
 * specific resources is split out into separate functions to avoid polluting
 * common code. If/when this driver is enhanced to support multiple
 * configurations, the core code should be adapted to call all configuration-
 * specific functions through function pointers, with the definition of those
 * function pointers being supplied by struct udevice_id eqos_ids[]'s .data
 * field.
 *
 * The following configurations are currently supported:
 * tegra186:
 *    NVIDIA's Tegra186 chip. This configuration uses an AXI master/DMA bus, an
 *    AHB slave/register bus, contains the DMA, MTL, and MAC sub-blocks, and
 *    supports a single RGMII PHY. This configuration also has SW control over
 *    all clock and reset signals to the HW block.
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

/* Core registers */

#define EQOS_MAC_REGS_BASE 0x000
struct eqos_mac_regs {
	uint32_t configuration;				/* 0x000 */
	uint32_t unused_004[(0x070 - 0x004) / 4];	/* 0x004 */
	uint32_t q0_tx_flow_ctrl;			/* 0x070 */
	uint32_t unused_070[(0x090 - 0x074) / 4];	/* 0x074 */
	uint32_t rx_flow_ctrl;				/* 0x090 */
	uint32_t unused_094;				/* 0x094 */
	uint32_t txq_prty_map0;				/* 0x098 */
	uint32_t unused_09c;				/* 0x09c */
	uint32_t rxq_ctrl0;				/* 0x0a0 */
	uint32_t unused_0a4;				/* 0x0a4 */
	uint32_t rxq_ctrl2;				/* 0x0a8 */
	uint32_t unused_0ac[(0x0dc - 0x0ac) / 4];	/* 0x0ac */
	uint32_t us_tic_counter;			/* 0x0dc */
	uint32_t unused_0e0[(0x11c - 0x0e0) / 4];	/* 0x0e0 */
	uint32_t hw_feature0;				/* 0x11c */
	uint32_t hw_feature1;				/* 0x120 */
	uint32_t hw_feature2;				/* 0x124 */
	uint32_t unused_128[(0x200 - 0x128) / 4];	/* 0x128 */
	uint32_t mdio_address;				/* 0x200 */
	uint32_t mdio_data;				/* 0x204 */
	uint32_t unused_208[(0x300 - 0x208) / 4];	/* 0x208 */
	uint32_t address0_high;				/* 0x300 */
	uint32_t address0_low;				/* 0x304 */
};

#define EQOS_MAC_CONFIGURATION_GPSLCE			BIT(23)
#define EQOS_MAC_CONFIGURATION_CST			BIT(21)
#define EQOS_MAC_CONFIGURATION_ACS			BIT(20)
#define EQOS_MAC_CONFIGURATION_WD			BIT(19)
#define EQOS_MAC_CONFIGURATION_JD			BIT(17)
#define EQOS_MAC_CONFIGURATION_JE			BIT(16)
#define EQOS_MAC_CONFIGURATION_PS			BIT(15)
#define EQOS_MAC_CONFIGURATION_FES			BIT(14)
#define EQOS_MAC_CONFIGURATION_DM			BIT(13)
#define EQOS_MAC_CONFIGURATION_TE			BIT(1)
#define EQOS_MAC_CONFIGURATION_RE			BIT(0)

#define EQOS_MAC_Q0_TX_FLOW_CTRL_PT_SHIFT		16
#define EQOS_MAC_Q0_TX_FLOW_CTRL_PT_MASK		0xffff
#define EQOS_MAC_Q0_TX_FLOW_CTRL_TFE			BIT(1)

#define EQOS_MAC_RX_FLOW_CTRL_RFE			BIT(0)

#define EQOS_MAC_TXQ_PRTY_MAP0_PSTQ0_SHIFT		0
#define EQOS_MAC_TXQ_PRTY_MAP0_PSTQ0_MASK		0xff

#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_SHIFT			0
#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_MASK			3
#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_NOT_ENABLED		0
#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_DCB		2
#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_AV		1

#define EQOS_MAC_RXQ_CTRL2_PSRQ0_SHIFT			0
#define EQOS_MAC_RXQ_CTRL2_PSRQ0_MASK			0xff

#define EQOS_MAC_HW_FEATURE1_TXFIFOSIZE_SHIFT		6
#define EQOS_MAC_HW_FEATURE1_TXFIFOSIZE_MASK		0x1f
#define EQOS_MAC_HW_FEATURE1_RXFIFOSIZE_SHIFT		0
#define EQOS_MAC_HW_FEATURE1_RXFIFOSIZE_MASK		0x1f

#define EQOS_MAC_MDIO_ADDRESS_PA_SHIFT			21
#define EQOS_MAC_MDIO_ADDRESS_RDA_SHIFT			16
#define EQOS_MAC_MDIO_ADDRESS_CR_SHIFT			8
#define EQOS_MAC_MDIO_ADDRESS_CR_20_35			2
#define EQOS_MAC_MDIO_ADDRESS_CR_250_300		5
#define EQOS_MAC_MDIO_ADDRESS_SKAP			BIT(4)
#define EQOS_MAC_MDIO_ADDRESS_GOC_SHIFT			2
#define EQOS_MAC_MDIO_ADDRESS_GOC_READ			3
#define EQOS_MAC_MDIO_ADDRESS_GOC_WRITE			1
#define EQOS_MAC_MDIO_ADDRESS_C45E			BIT(1)
#define EQOS_MAC_MDIO_ADDRESS_GB			BIT(0)

#define EQOS_MAC_MDIO_DATA_GD_MASK			0xffff

#define EQOS_MTL_REGS_BASE 0xd00
struct eqos_mtl_regs {
	uint32_t txq0_operation_mode;			/* 0xd00 */
	uint32_t unused_d04;				/* 0xd04 */
	uint32_t txq0_debug;				/* 0xd08 */
	uint32_t unused_d0c[(0xd18 - 0xd0c) / 4];	/* 0xd0c */
	uint32_t txq0_quantum_weight;			/* 0xd18 */
	uint32_t unused_d1c[(0xd30 - 0xd1c) / 4];	/* 0xd1c */
	uint32_t rxq0_operation_mode;			/* 0xd30 */
	uint32_t unused_d34;				/* 0xd34 */
	uint32_t rxq0_debug;				/* 0xd38 */
};

#define EQOS_MTL_TXQ0_OPERATION_MODE_TQS_SHIFT		16
#define EQOS_MTL_TXQ0_OPERATION_MODE_TQS_MASK		0x1ff
#define EQOS_MTL_TXQ0_OPERATION_MODE_TXQEN_SHIFT	2
#define EQOS_MTL_TXQ0_OPERATION_MODE_TXQEN_MASK		3
#define EQOS_MTL_TXQ0_OPERATION_MODE_TXQEN_ENABLED	2
#define EQOS_MTL_TXQ0_OPERATION_MODE_TSF		BIT(1)
#define EQOS_MTL_TXQ0_OPERATION_MODE_FTQ		BIT(0)

#define EQOS_MTL_TXQ0_DEBUG_TXQSTS			BIT(4)
#define EQOS_MTL_TXQ0_DEBUG_TRCSTS_SHIFT		1
#define EQOS_MTL_TXQ0_DEBUG_TRCSTS_MASK			3

#define EQOS_MTL_RXQ0_OPERATION_MODE_RQS_SHIFT		20
#define EQOS_MTL_RXQ0_OPERATION_MODE_RQS_MASK		0x3ff
#define EQOS_MTL_RXQ0_OPERATION_MODE_RFD_SHIFT		14
#define EQOS_MTL_RXQ0_OPERATION_MODE_RFD_MASK		0x3f
#define EQOS_MTL_RXQ0_OPERATION_MODE_RFA_SHIFT		8
#define EQOS_MTL_RXQ0_OPERATION_MODE_RFA_MASK		0x3f
#define EQOS_MTL_RXQ0_OPERATION_MODE_EHFC		BIT(7)
#define EQOS_MTL_RXQ0_OPERATION_MODE_RSF		BIT(5)

#define EQOS_MTL_RXQ0_DEBUG_PRXQ_SHIFT			16
#define EQOS_MTL_RXQ0_DEBUG_PRXQ_MASK			0x7fff
#define EQOS_MTL_RXQ0_DEBUG_RXQSTS_SHIFT		4
#define EQOS_MTL_RXQ0_DEBUG_RXQSTS_MASK			3

#define EQOS_DMA_REGS_BASE 0x1000
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

#define EQOS_DESCRIPTOR_WORDS	4
#define EQOS_DESCRIPTOR_SIZE	(EQOS_DESCRIPTOR_WORDS * 4)
/* We assume ARCH_DMA_MINALIGN >= 16; 16 is the EQOS HW minimum */
#define EQOS_DESCRIPTOR_ALIGN	64
#define EQOS_DESCRIPTORS_TX	4
#define EQOS_DESCRIPTORS_RX	4
#define EQOS_DESCRIPTORS_NUM	(EQOS_DESCRIPTORS_TX + EQOS_DESCRIPTORS_RX)
#define EQOS_DESCRIPTORS_SIZE	ALIGN(EQOS_DESCRIPTORS_NUM * \
				      EQOS_DESCRIPTOR_SIZE, EQOS_DESCRIPTOR_ALIGN)
#define EQOS_BUFFER_ALIGN	EQOS_DESCRIPTOR_ALIGN
#define EQOS_MAX_PACKET_SIZE	ALIGN(1568, EQOS_DESCRIPTOR_ALIGN)
#define EQOS_RX_BUFFER_SIZE	(EQOS_DESCRIPTORS_RX * EQOS_MAX_PACKET_SIZE)

#define SYSCFG_MCU_ETH_MASK		BIT(23)
#define SYSCFG_MP1_ETH_MASK		GENMASK(23, 16)
#define SYSCFG_PMCCLRR_OFFSET		0x40

struct eqos_desc {
	u32 des0;
	u32 des1;
	u32 des2;
	u32 des3;
};

#define EQOS_DESC3_OWN		BIT(31)
#define EQOS_DESC3_FD		BIT(29)
#define EQOS_DESC3_LD		BIT(28)
#define EQOS_DESC3_BUF1V	BIT(24)

struct eqos_config {
	bool reg_access_always_ok;
	int mdio_wait;
	int config_mac;
	int config_mac_mdio;
	struct eqos_ops *ops;
};

struct eqos_priv;
struct eqos_ops {
	int (*eqos_probe_resources)(struct device_d *);
	int (*eqos_remove_resources)(struct device_d *);
	int (*eqos_stop_resets)(struct eqos_priv *);
	int (*eqos_start_resets)(struct eqos_priv *);
	void (*eqos_stop_clks)(struct eqos_priv *);
	int (*eqos_start_clks)(struct eqos_priv *);
	int (*eqos_calibrate_pads)(struct eqos_priv *);
	int (*eqos_disable_calibration)(struct eqos_priv *);
	int (*eqos_set_tx_clk_speed)(struct eth_device *);
	unsigned long (*eqos_get_tick_clk_rate)(struct eqos_priv *);
	int (*eqos_set_mode)(struct eqos_priv *);
};

struct eqos_priv {
	struct eth_device edev;
	struct device_d *dev;
	struct mii_bus miibus;
	const struct eqos_config *config;
	void __iomem *regs;
	struct eqos_mac_regs *mac_regs;
	struct eqos_mtl_regs *mtl_regs;
	struct eqos_dma_regs *dma_regs;
	struct eqos_tegra186_regs *tegra186_regs;
	struct reset_control *reset_ctl;
	struct regmap *regmap;
	u32 mode_reg;
	int phy_reset_gpio;
	unsigned phy_interface;
	int phy_addr;
	void *descs;
	struct eqos_desc *tx_descs;
	struct eqos_desc *rx_descs;
	int tx_desc_idx, rx_desc_idx;
	void *tx_dma_buf; // TODO remove?
	void *rx_dma_buf;
	void *rx_pkt;
	bool started;
	bool reg_access_ok;
	struct clk_bulk_data *clks;
	int eth_clk_sel_reg; /* FIXME: actually use */
	int eth_ref_clk_sel_reg;
};

/*
 * TX and RX descriptors are 16 bytes. This causes problems with the cache
 * maintenance on CPUs where the cache-line size exceeds the size of these
 * descriptors. What will happen is that when the driver receives a packet
 * it will be immediately requeued for the hardware to reuse. The CPU will
 * therefore need to flush the cache-line containing the descriptor, which
 * will cause all other descriptors in the same cache-line to be flushed
 * along with it. If one of those descriptors had been written to by the
 * device those changes (and the associated packet) will be lost.
 *
 * To work around this, we make use of non-cached memory if available. If
 * descriptors are mapped uncached there's no need to manually flush them
 * or invalidate them.
 *
 * Note that this only applies to descriptors. The packet data buffers do
 * not have the same constraints since they are 1536 bytes large, so they
 * are unlikely to share cache-lines.
 */
static void *eqos_alloc_descs(unsigned int num)
{
	return dma_alloc_coherent(EQOS_DESCRIPTORS_SIZE, DMA_ADDRESS_BROKEN);
}

static int eqos_mdio_wait_idle(struct eqos_priv *eqos)
{
	u32 idle;
	return readl_poll_timeout(&eqos->mac_regs->mdio_address, idle,
			   idle & EQOS_MAC_MDIO_ADDRESS_GB,
			   100000);
}

static int eqos_mdio_read(struct mii_bus *bus, int mdio_addr,
			  int mdio_reg)
{
	struct eqos_priv *eqos = bus->priv;
	u32 val;
	int ret;

	pr_debug("%s(dev=%p, addr=%x, reg=%d):\n", __func__, eqos->dev, mdio_addr,
	      mdio_reg);

	ret = eqos_mdio_wait_idle(eqos);
	if (ret) {
		pr_err("MDIO not idle at entry");
		return ret;
	}

	val = readl(&eqos->mac_regs->mdio_address);
	val &= EQOS_MAC_MDIO_ADDRESS_SKAP |
		EQOS_MAC_MDIO_ADDRESS_C45E;
	val |= (mdio_addr << EQOS_MAC_MDIO_ADDRESS_PA_SHIFT) |
		(mdio_reg << EQOS_MAC_MDIO_ADDRESS_RDA_SHIFT) |
		(eqos->config->config_mac_mdio <<
		 EQOS_MAC_MDIO_ADDRESS_CR_SHIFT) |
		(EQOS_MAC_MDIO_ADDRESS_GOC_READ <<
		 EQOS_MAC_MDIO_ADDRESS_GOC_SHIFT) |
		EQOS_MAC_MDIO_ADDRESS_GB;
	writel(val, &eqos->mac_regs->mdio_address);

	udelay(eqos->config->mdio_wait);

	ret = eqos_mdio_wait_idle(eqos);
	if (ret) {
		pr_err("MDIO read didn't complete");
		return ret;
	}

	val = readl(&eqos->mac_regs->mdio_data);
	val &= EQOS_MAC_MDIO_DATA_GD_MASK;

	pr_debug("%s: val=%x\n", __func__, val);

	return val;
}

static int eqos_mdio_write(struct mii_bus *bus, int mdio_addr,
			   int mdio_reg, u16 mdio_val)
{
	struct eqos_priv *eqos = bus->priv;
	u32 val;
	int ret;

	pr_debug("%s(dev=%p, addr=%x, reg=%d, val=%x):\n", __func__, eqos->dev,
	      mdio_addr, mdio_reg, mdio_val);

	ret = eqos_mdio_wait_idle(eqos);
	if (ret) {
		pr_err("MDIO not idle at entry");
		return ret;
	}

	writel(mdio_val, &eqos->mac_regs->mdio_data);

	val = readl(&eqos->mac_regs->mdio_address);
	val &= EQOS_MAC_MDIO_ADDRESS_SKAP |
		EQOS_MAC_MDIO_ADDRESS_C45E;
	val |= (mdio_addr << EQOS_MAC_MDIO_ADDRESS_PA_SHIFT) |
		(mdio_reg << EQOS_MAC_MDIO_ADDRESS_RDA_SHIFT) |
		(eqos->config->config_mac_mdio <<
		 EQOS_MAC_MDIO_ADDRESS_CR_SHIFT) |
		(EQOS_MAC_MDIO_ADDRESS_GOC_WRITE <<
		 EQOS_MAC_MDIO_ADDRESS_GOC_SHIFT) |
		EQOS_MAC_MDIO_ADDRESS_GB;
	writel(val, &eqos->mac_regs->mdio_address);

	udelay(eqos->config->mdio_wait);

	ret = eqos_mdio_wait_idle(eqos);
	if (ret) {
		pr_err("MDIO read didn't complete");
		return ret;
	}

	return 0;
}

enum { CLK_SLAVE_BUS, CLK_MASTER_BUS, CLK_RX, CLK_PTP_REF, CLK_TX };
static const struct clk_bulk_data tegra_clks[] = {
	[CLK_SLAVE_BUS]  = { .id = "slave_bus" },
	[CLK_MASTER_BUS] = { .id = "master_bus" },
	[CLK_RX]         = { .id = "rx" },
	[CLK_PTP_REF]    = { .id = "ptp_ref" },
	[CLK_TX]         = { .id = "tx" },
};

static int eqos_start_clks_tegra186(struct eqos_priv *eqos)
{
	int ret;

	// FIXME: xmemdup for tegra_clks here
	ret = clk_bulk_enable(ARRAY_SIZE(tegra_clks), eqos->clks);
	if (ret < 0) {
		pr_err("Enabling clocks failed: %s\n", strerror(-ret));
		return ret;
	}

#if 0 // FIXME
	       ret = clk_set_rate(&eqos->clk_ptp_ref, 125 * 1000 * 1000);
if (ret < 0) {
        pr_err("clk_set_rate(clk_ptp_ref) failed: %d\n", ret);
        goto err_disable_clk_ptp_ref;
#endif

	return ret;
}

enum { CLK_STMMACETH, CLK_MAX_RX, CLK_MAX_TX, CLK_SYSCFG };
static const struct clk_bulk_data stm32_clks[] = {
	[CLK_STMMACETH] = { .id = "stmmaceth" },
	[CLK_MAX_RX]    = { .id = "mac-clk-rx" },
	[CLK_MAX_TX]    = { .id = "mac-clk-tx" },
	[CLK_SYSCFG]    = { .id = "syscfg-clk" },
};

static int eqos_start_clks_stm32(struct eqos_priv *eqos)
{
	int ret;

	// FIXME that one optional PHY clock
	ret = clk_bulk_enable(ARRAY_SIZE(stm32_clks), eqos->clks);
	if (ret < 0) {
		pr_err("clk_enable(clk_master_bus) failed: %s\n", strerror(-ret));
		return ret;
	}

	pr_debug("%s: OK\n", __func__);
	return 0;
}

static void eqos_stop_clks_tegra186(struct eqos_priv *eqos)
{
	clk_bulk_disable(ARRAY_SIZE(tegra_clks), eqos->clks);

	pr_debug("%s: OK\n", __func__);
}

static void eqos_stop_clks_stm32(struct eqos_priv *eqos)
{
	clk_bulk_disable(ARRAY_SIZE(stm32_clks), eqos->clks);
	// FIXME that one  optional PHY clock

	pr_debug("%s: OK\n", __func__);
}

static int eqos_start_resets_tegra186(struct eqos_priv *eqos)
{
	int ret;

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

	pr_debug("%s: OK\n", __func__);
	return 0;
}

static int eqos_stop_resets_tegra186(struct eqos_priv *eqos)
{
	reset_control_assert(eqos->reset_ctl);
	gpio_set_value(eqos->phy_reset_gpio, 1);

	return 0;
}

static int eqos_calibrate_pads_tegra186(struct eqos_priv *eqos)
{
	u32 active;
	int ret;

	setbits_le32(&eqos->tegra186_regs->sdmemcomppadctrl,
		     EQOS_SDMEMCOMPPADCTRL_PAD_E_INPUT_OR_E_PWRD);

	udelay(1);

	setbits_le32(&eqos->tegra186_regs->auto_cal_config,
		     EQOS_AUTO_CAL_CONFIG_START | EQOS_AUTO_CAL_CONFIG_ENABLE);

	ret = readl_poll_timeout(&eqos->tegra186_regs->auto_cal_status, active,
				 (active & EQOS_AUTO_CAL_STATUS_ACTIVE) == EQOS_AUTO_CAL_STATUS_ACTIVE,
				 10000);
	if (ret) {
		pr_err("calibrate didn't start");
		goto failed;
	}

	ret = readl_poll_timeout(&eqos->tegra186_regs->auto_cal_status, active,
				 (~active & EQOS_AUTO_CAL_STATUS_ACTIVE) == EQOS_AUTO_CAL_STATUS_ACTIVE,
				 10000);
	if (ret) {
		pr_err("calibrate didn't finish");
		goto failed;
	}

	ret = 0;

failed:
	clrbits_le32(&eqos->tegra186_regs->sdmemcomppadctrl,
		     EQOS_SDMEMCOMPPADCTRL_PAD_E_INPUT_OR_E_PWRD);

	pr_debug("%s: returns %d\n", __func__, ret);

	return ret;
}

static int eqos_disable_calibration_tegra186(struct eqos_priv *eqos)
{
	clrbits_le32(&eqos->tegra186_regs->auto_cal_config,
		     EQOS_AUTO_CAL_CONFIG_ENABLE);

	return 0;
}

static unsigned long eqos_get_tick_clk_rate_tegra186(struct eqos_priv *eqos)
{
	return clk_get_rate(eqos->clks[CLK_SLAVE_BUS].clk);
}

static unsigned long eqos_get_tick_clk_rate_stm32(struct eqos_priv *eqos)
{
	return clk_get_rate(eqos->clks[CLK_STMMACETH].clk);
}

static int eqos_set_full_duplex(struct eqos_priv *eqos)
{
	setbits_le32(&eqos->mac_regs->configuration, EQOS_MAC_CONFIGURATION_DM);

	return 0;
}

static int eqos_set_half_duplex(struct eqos_priv *eqos)
{
	clrbits_le32(&eqos->mac_regs->configuration, EQOS_MAC_CONFIGURATION_DM);

	/* WAR: Flush TX queue when switching to half-duplex */
	setbits_le32(&eqos->mtl_regs->txq0_operation_mode,
		     EQOS_MTL_TXQ0_OPERATION_MODE_FTQ);

	return 0;
}

static int eqos_set_mode_stm32(struct eqos_priv *eqos)
{
	u32 val, reg = eqos->mode_reg;
	int ret;

	switch (eqos->phy_interface) {
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
		pr_err("bad phy mode %d\n",
			eqos->phy_interface);
		return -EINVAL;
	}

	/* Need to update PMCCLRR (clear register) */
	ret = regmap_write(eqos->regmap, reg + SYSCFG_PMCCLRR_OFFSET,
			   SYSCFG_MP1_ETH_MASK);

	/* Update PMCSETR (set register) */
	return regmap_update_bits(eqos->regmap, reg,
				  GENMASK(23, 16), val); // BIT(32) for MCU..
}

static int eqos_set_gmii_speed(struct eqos_priv *eqos)
{
	clrbits_le32(&eqos->mac_regs->configuration,
		     EQOS_MAC_CONFIGURATION_PS | EQOS_MAC_CONFIGURATION_FES);

	return 0;
}

static int eqos_set_mii_speed_100(struct eqos_priv *eqos)
{
	setbits_le32(&eqos->mac_regs->configuration,
		     EQOS_MAC_CONFIGURATION_PS | EQOS_MAC_CONFIGURATION_FES);

	return 0;
}

static int eqos_set_mii_speed_10(struct eqos_priv *eqos)
{
	clrsetbits_le32(&eqos->mac_regs->configuration,
			EQOS_MAC_CONFIGURATION_FES, EQOS_MAC_CONFIGURATION_PS);

	return 0;
}

static int eqos_set_tx_clk_speed_tegra186(struct eth_device *edev)
{
	struct eqos_priv *eqos = edev->priv;
	unsigned speed = edev->phydev->speed;
	unsigned long rate;
	int ret;

	switch (speed) {
	case SPEED_1000:
		rate = 125 * 1000 * 1000;
		break;
	case SPEED_100:
		rate = 25 * 1000 * 1000;
		break;
	case SPEED_10:
		rate = 2.5 * 1000 * 1000;
		break;
	default:
		pr_err("invalid speed %d\n", speed);
		return -EINVAL;
	}

	ret = clk_set_rate(eqos->clks[CLK_TX].clk, rate);
	if (ret < 0) {
		pr_err("clk_set_rate(tx_clk, %lu) failed: %d\n", rate, ret);
		return ret;
	}

	return 0;
}

static void eqos_adjust_link(struct eth_device *edev)
{
	struct eqos_priv *eqos = edev->priv;
	unsigned speed = edev->phydev->speed;
	int ret;
	bool en_calibration;

	pr_debug("%s(edev=%p):\n", __func__, edev);

	if (edev->phydev->duplex)
		ret = eqos_set_full_duplex(eqos);
	else
		ret = eqos_set_half_duplex(eqos);
	if (ret < 0) {
		pr_err("eqos_set_*_duplex() failed: %s\n", strerror(-ret));
		return;
	}

	switch (speed) {
	case SPEED_1000:
		en_calibration = true;
		ret = eqos_set_gmii_speed(eqos);
		break;
	case SPEED_100:
		en_calibration = true;
		ret = eqos_set_mii_speed_100(eqos);
		break;
	case SPEED_10:
		en_calibration = false;
		ret = eqos_set_mii_speed_10(eqos);
		break;
	default:
		pr_err("invalid speed %d\n", speed);
		return;
	}
	if (ret < 0) {
		pr_err("eqos_set_*mii_speed*() failed: %s\n", strerror(-ret));
		return;
	}

	if (en_calibration) {
		if (eqos->config->ops->eqos_calibrate_pads) {
			ret = eqos->config->ops->eqos_calibrate_pads(eqos);
			if (ret < 0) {
				pr_err("eqos_calibrate_pads() failed: %d\n",
				       ret);
				return;
			}
		}
	} else {
		if (eqos->config->ops->eqos_disable_calibration) {
			ret = eqos->config->ops->eqos_disable_calibration(eqos);
			if (ret < 0) {
				pr_err("eqos_disable_calibration() failed: %d\n",
				       ret);
				return;
			}
		}
	}
	if (eqos->config->ops->eqos_set_tx_clk_speed) {
		ret = eqos->config->ops->eqos_set_tx_clk_speed(edev);
		if (ret < 0) {
			pr_err("eqos_set_tx_clk_speed() failed: %s\n", strerror(-ret));
			return;
		}
	}
}

static int eqos_get_hwaddr(struct eth_device *dev, unsigned char *mac)
{
	return -1;
}

static int eqos_set_hwaddr(struct eth_device *edev, const unsigned char *mac)
{
	struct eqos_priv *eqos = edev->priv;
	uint32_t val;

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
#if 0 // FIXME
	if (!eqos->config->reg_access_always_ok && !eqos->reg_access_ok)
		return 0;
#endif
	
	pr_info("HARDWAR EADDRESS SET!!!!!!!!!!\n");

	/* Update the MAC address */
	val = (mac[5] << 8) |
		(mac[4]);
	writel(val, &eqos->mac_regs->address0_high);
	val = (mac[3] << 24) |
		(mac[2] << 16) |
		(mac[1] << 8) |
		(mac[0]);
	writel(val, &eqos->mac_regs->address0_low);

	return 0;
}

#define DMA_BUS_MODE_SFT_RESET		BIT(0)
#define DMA_BUS_MODE			0x00001000

static int dwmac4_dma_reset(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + DMA_BUS_MODE);
	int limit;

	/* DMA SW reset */
	value |= DMA_BUS_MODE_SFT_RESET;
	writel(value, ioaddr + DMA_BUS_MODE);
	limit = 10;
	while (limit--) {
		if (!(readl(ioaddr + DMA_BUS_MODE) & DMA_BUS_MODE_SFT_RESET))
			break;
		mdelay(10);
	}

	if (limit < 0)
		return -EBUSY;

	return 0;
}

static int eqos_start(struct eth_device *edev)
{
	struct eqos_priv *eqos = edev->priv;
	int ret, i;
	unsigned long rate;
	u32 mode_set;
	u32 val, tx_fifo_sz, rx_fifo_sz, tqs, rqs, pbl;
	unsigned long last_rx_desc;

	pr_debug("%s(edev=%p):\n", __func__, edev);

	eqos->tx_desc_idx = 0;
	eqos->rx_desc_idx = 0;

	dwmac4_dma_reset(eqos->regs);

	ret = eqos->config->ops->eqos_start_clks(eqos);
	if (ret < 0) {
		pr_err("eqos_start_clks() failed: %s\n", strerror(-ret));
		goto err;
	}

	if (eqos->config->ops->eqos_set_mode)
		eqos->config->ops->eqos_set_mode(eqos);

	pr_debug("%s(line=%d):\n", __func__, __LINE__);
	if (eqos->config->ops->eqos_start_resets) {
		ret = eqos->config->ops->eqos_start_resets(eqos);
		if (ret < 0) {
			pr_err("eqos_start_resets() failed: %s\n", strerror(-ret));
			goto err_stop_clks;
		}
	}

	pr_debug("%s(line=%d):\n", __func__, __LINE__);
	udelay(10);

	eqos->reg_access_ok = true;

	pr_debug("%s(line=%d):\n", __func__, __LINE__);
	ret = readl_poll_timeout(&eqos->dma_regs->mode, mode_set,
				 (mode_set & EQOS_DMA_MODE_SWR) == EQOS_DMA_MODE_SWR,
				 10000);
	if (ret) {
		pr_err("EQOS_DMA_MODE_SWR stuck: 0x%08x", mode_set);
		goto err_stop_resets;
	}

	pr_debug("%s(line=%d):\n", __func__, __LINE__);
	if (eqos->config->ops->eqos_calibrate_pads) {
	pr_debug("%s(line=%d):\n", __func__, __LINE__);
		ret = eqos->config->ops->eqos_calibrate_pads(eqos);
		if (ret < 0) {
			pr_err("eqos_calibrate_pads() failed: %s\n", strerror(-ret));
			goto err_stop_resets;
		}
	}

	pr_debug("%s(line=%d):\n", __func__, __LINE__);
	rate = eqos->config->ops->eqos_get_tick_clk_rate(eqos);
	pr_debug("%s(line=%d):\n", __func__, __LINE__);

	val = (rate / 1000000) - 1;
	writel(val, &eqos->mac_regs->us_tic_counter);

	pr_debug("%s(line=%d):\n", __func__, __LINE__);
	ret = phy_device_connect(edev, &eqos->miibus, eqos->phy_addr,
				 eqos_adjust_link, 0, eqos->phy_interface);
	if (ret) {
		pr_err("phy_device_connect() failed");
		goto err_stop_resets;
	}
	pr_debug("%s(line=%d):\n", __func__, __LINE__);

	/* Configure MTL */

	/* Enable Store and Forward mode for TX */
	/* Program Tx operating mode */
	setbits_le32(&eqos->mtl_regs->txq0_operation_mode,
		     EQOS_MTL_TXQ0_OPERATION_MODE_TSF |
		     (EQOS_MTL_TXQ0_OPERATION_MODE_TXQEN_ENABLED <<
		      EQOS_MTL_TXQ0_OPERATION_MODE_TXQEN_SHIFT));

	/* Transmit Queue weight */
	writel(0x10, &eqos->mtl_regs->txq0_quantum_weight);

	/* Enable Store and Forward mode for RX, since no jumbo frame */
	setbits_le32(&eqos->mtl_regs->rxq0_operation_mode,
		     EQOS_MTL_RXQ0_OPERATION_MODE_RSF);

	/* Transmit/Receive queue fifo size; use all RAM for 1 queue */
	val = readl(&eqos->mac_regs->hw_feature1);
	tx_fifo_sz = (val >> EQOS_MAC_HW_FEATURE1_TXFIFOSIZE_SHIFT) &
		EQOS_MAC_HW_FEATURE1_TXFIFOSIZE_MASK;
	rx_fifo_sz = (val >> EQOS_MAC_HW_FEATURE1_RXFIFOSIZE_SHIFT) &
		EQOS_MAC_HW_FEATURE1_RXFIFOSIZE_MASK;

	/*
	 * r/tx_fifo_sz is encoded as log2(n / 128). Undo that by shifting.
	 * r/tqs is encoded as (n / 256) - 1.
	 */
	tqs = (128 << tx_fifo_sz) / 256 - 1;
	rqs = (128 << rx_fifo_sz) / 256 - 1;

	clrsetbits_le32(&eqos->mtl_regs->txq0_operation_mode,
			EQOS_MTL_TXQ0_OPERATION_MODE_TQS_MASK <<
			EQOS_MTL_TXQ0_OPERATION_MODE_TQS_SHIFT,
			tqs << EQOS_MTL_TXQ0_OPERATION_MODE_TQS_SHIFT);
	clrsetbits_le32(&eqos->mtl_regs->rxq0_operation_mode,
			EQOS_MTL_RXQ0_OPERATION_MODE_RQS_MASK <<
			EQOS_MTL_RXQ0_OPERATION_MODE_RQS_SHIFT,
			rqs << EQOS_MTL_RXQ0_OPERATION_MODE_RQS_SHIFT);

	/* Flow control used only if each channel gets 4KB or more FIFO */
	if (rqs >= ((4096 / 256) - 1)) {
		u32 rfd, rfa;

		setbits_le32(&eqos->mtl_regs->rxq0_operation_mode,
			     EQOS_MTL_RXQ0_OPERATION_MODE_EHFC);

		/*
		 * Set Threshold for Activating Flow Contol space for min 2
		 * frames ie, (1500 * 1) = 1500 bytes.
		 *
		 * Set Threshold for Deactivating Flow Contol for space of
		 * min 1 frame (frame size 1500bytes) in receive fifo
		 */
		if (rqs == ((4096 / 256) - 1)) {
			/*
			 * This violates the above formula because of FIFO size
			 * limit therefore overflow may occur inspite of this.
			 */
			rfd = 0x3;	/* Full-3K */
			rfa = 0x1;	/* Full-1.5K */
		} else if (rqs == ((8192 / 256) - 1)) {
			rfd = 0x6;	/* Full-4K */
			rfa = 0xa;	/* Full-6K */
		} else if (rqs == ((16384 / 256) - 1)) {
			rfd = 0x6;	/* Full-4K */
			rfa = 0x12;	/* Full-10K */
		} else {
			rfd = 0x6;	/* Full-4K */
			rfa = 0x1E;	/* Full-16K */
		}

		clrsetbits_le32(&eqos->mtl_regs->rxq0_operation_mode,
				(EQOS_MTL_RXQ0_OPERATION_MODE_RFD_MASK <<
				 EQOS_MTL_RXQ0_OPERATION_MODE_RFD_SHIFT) |
				(EQOS_MTL_RXQ0_OPERATION_MODE_RFA_MASK <<
				 EQOS_MTL_RXQ0_OPERATION_MODE_RFA_SHIFT),
				(rfd <<
				 EQOS_MTL_RXQ0_OPERATION_MODE_RFD_SHIFT) |
				(rfa <<
				 EQOS_MTL_RXQ0_OPERATION_MODE_RFA_SHIFT));
	}

	/* Configure MAC */

	clrsetbits_le32(&eqos->mac_regs->rxq_ctrl0,
			EQOS_MAC_RXQ_CTRL0_RXQ0EN_MASK <<
			EQOS_MAC_RXQ_CTRL0_RXQ0EN_SHIFT,
			eqos->config->config_mac <<
			EQOS_MAC_RXQ_CTRL0_RXQ0EN_SHIFT);

	/* Set TX flow control parameters */
	/* Set Pause Time */
	setbits_le32(&eqos->mac_regs->q0_tx_flow_ctrl,
		     0xffff << EQOS_MAC_Q0_TX_FLOW_CTRL_PT_SHIFT);
	/* Assign priority for TX flow control */
	clrbits_le32(&eqos->mac_regs->txq_prty_map0,
		     EQOS_MAC_TXQ_PRTY_MAP0_PSTQ0_MASK <<
		     EQOS_MAC_TXQ_PRTY_MAP0_PSTQ0_SHIFT);
	/* Assign priority for RX flow control */
	clrbits_le32(&eqos->mac_regs->rxq_ctrl2,
		     EQOS_MAC_RXQ_CTRL2_PSRQ0_MASK <<
		     EQOS_MAC_RXQ_CTRL2_PSRQ0_SHIFT);
	/* Enable flow control */
	setbits_le32(&eqos->mac_regs->q0_tx_flow_ctrl,
		     EQOS_MAC_Q0_TX_FLOW_CTRL_TFE);
	setbits_le32(&eqos->mac_regs->rx_flow_ctrl,
		     EQOS_MAC_RX_FLOW_CTRL_RFE);

	clrsetbits_le32(&eqos->mac_regs->configuration,
			EQOS_MAC_CONFIGURATION_GPSLCE |
			EQOS_MAC_CONFIGURATION_WD |
			EQOS_MAC_CONFIGURATION_JD |
			EQOS_MAC_CONFIGURATION_JE,
			EQOS_MAC_CONFIGURATION_CST |
			EQOS_MAC_CONFIGURATION_ACS);

	/* Configure DMA */

	/* Enable OSP mode */
	setbits_le32(&eqos->dma_regs->ch0_tx_control,
		     EQOS_DMA_CH0_TX_CONTROL_OSP);

	/* RX buffer size. Must be a multiple of bus width */
	clrsetbits_le32(&eqos->dma_regs->ch0_rx_control,
			EQOS_DMA_CH0_RX_CONTROL_RBSZ_MASK <<
			EQOS_DMA_CH0_RX_CONTROL_RBSZ_SHIFT,
			EQOS_MAX_PACKET_SIZE <<
			EQOS_DMA_CH0_RX_CONTROL_RBSZ_SHIFT);

	setbits_le32(&eqos->dma_regs->ch0_control,
		     EQOS_DMA_CH0_CONTROL_PBLX8);

	/*
	 * Burst length must be < 1/2 FIFO size.
	 * FIFO size in tqs is encoded as (n / 256) - 1.
	 * Each burst is n * 8 (PBLX8) * 16 (AXI width) == 128 bytes.
	 * Half of n * 256 is n * 128, so pbl == tqs, modulo the -1.
	 */
	pbl = tqs + 1;
	if (pbl > 32)
		pbl = 32;
	clrsetbits_le32(&eqos->dma_regs->ch0_tx_control,
			EQOS_DMA_CH0_TX_CONTROL_TXPBL_MASK <<
			EQOS_DMA_CH0_TX_CONTROL_TXPBL_SHIFT,
			pbl << EQOS_DMA_CH0_TX_CONTROL_TXPBL_SHIFT);

	clrsetbits_le32(&eqos->dma_regs->ch0_rx_control,
			EQOS_DMA_CH0_RX_CONTROL_RXPBL_MASK <<
			EQOS_DMA_CH0_RX_CONTROL_RXPBL_SHIFT,
			8 << EQOS_DMA_CH0_RX_CONTROL_RXPBL_SHIFT);

	/* DMA performance configuration */
	val = (2 << EQOS_DMA_SYSBUS_MODE_RD_OSR_LMT_SHIFT) |
		EQOS_DMA_SYSBUS_MODE_EAME | EQOS_DMA_SYSBUS_MODE_BLEN16 |
		EQOS_DMA_SYSBUS_MODE_BLEN8 | EQOS_DMA_SYSBUS_MODE_BLEN4;
	writel(val, &eqos->dma_regs->sysbus_mode);

	/* Set up descriptors */

	pr_debug("%s(line=%d):\n", __func__, __LINE__);
	for (i = 0; i < EQOS_DESCRIPTORS_RX; i++) {
		struct eqos_desc *rx_desc = &(eqos->rx_descs[i]);
		rx_desc->des0 = (u32)(ulong)(eqos->rx_dma_buf +
					     (i * EQOS_MAX_PACKET_SIZE));
		rx_desc->des3 |= EQOS_DESC3_BUF1V;
		barrier();
		rx_desc->des3 |= EQOS_DESC3_OWN;
	}

	barrier();

	writel(0, &eqos->dma_regs->ch0_txdesc_list_haddress);
	writel((ulong)eqos->tx_descs, &eqos->dma_regs->ch0_txdesc_list_address);
	writel(EQOS_DESCRIPTORS_TX - 1,
	       &eqos->dma_regs->ch0_txdesc_ring_length);

	writel(0, &eqos->dma_regs->ch0_rxdesc_list_haddress);
	writel((ulong)eqos->rx_descs, &eqos->dma_regs->ch0_rxdesc_list_address);
	writel(EQOS_DESCRIPTORS_RX - 1,
	       &eqos->dma_regs->ch0_rxdesc_ring_length);
	pr_debug("%s(line=%d):\n", __func__, __LINE__);

	/* Enable everything */

	setbits_le32(&eqos->mac_regs->configuration,
		     EQOS_MAC_CONFIGURATION_TE | EQOS_MAC_CONFIGURATION_RE);

	setbits_le32(&eqos->dma_regs->ch0_tx_control,
		     EQOS_DMA_CH0_TX_CONTROL_ST);
	setbits_le32(&eqos->dma_regs->ch0_rx_control,
		     EQOS_DMA_CH0_RX_CONTROL_SR);

	/* TX tail pointer not written until we need to TX a packet */
	/*
	 * Point RX tail pointer at last descriptor. Ideally, we'd point at the
	 * first descriptor, implying all descriptors were available. However,
	 * that's not distinguishable from none of the descriptors being
	 * available.
	 */
	last_rx_desc = (ulong)&(eqos->rx_descs[(EQOS_DESCRIPTORS_RX - 1)]);
	writel(last_rx_desc, &eqos->dma_regs->ch0_rxdesc_tail_pointer);

	eqos->started = true;

	pr_debug("%s: OK\n", __func__);
	return 0;

err_stop_resets:
	if (eqos->config->ops->eqos_stop_resets)
		eqos->config->ops->eqos_stop_resets(eqos);
err_stop_clks:
	if (eqos->config->ops->eqos_stop_clks)
		eqos->config->ops->eqos_stop_clks(eqos);
err:
	pr_err("FAILED: %s\n", strerror(-ret));
	return ret;
}

static void eqos_stop(struct eth_device *dev)
{
	struct eqos_priv *eqos = dev->priv;
	int i;

	pr_debug("%s(dev=%p):\n", __func__, dev);

	if (!eqos->started)
		return;
	eqos->started = false;
	eqos->reg_access_ok = false;

	/* Disable TX DMA */
	clrbits_le32(&eqos->dma_regs->ch0_tx_control,
		     EQOS_DMA_CH0_TX_CONTROL_ST);

	/* Wait for TX all packets to drain out of MTL */
	for (i = 0; i < 1000000; i++) {
		u32 val = readl(&eqos->mtl_regs->txq0_debug);
		u32 trcsts = (val >> EQOS_MTL_TXQ0_DEBUG_TRCSTS_SHIFT) &
			EQOS_MTL_TXQ0_DEBUG_TRCSTS_MASK;
		u32 txqsts = val & EQOS_MTL_TXQ0_DEBUG_TXQSTS;
		if ((trcsts != 1) && (!txqsts))
			break;
	}

	/* Turn off MAC TX and RX */
	clrbits_le32(&eqos->mac_regs->configuration,
		     EQOS_MAC_CONFIGURATION_TE | EQOS_MAC_CONFIGURATION_RE);

	/* Wait for all RX packets to drain out of MTL */
	for (i = 0; i < 1000000; i++) {
		u32 val = readl(&eqos->mtl_regs->rxq0_debug);
		u32 prxq = (val >> EQOS_MTL_RXQ0_DEBUG_PRXQ_SHIFT) &
			EQOS_MTL_RXQ0_DEBUG_PRXQ_MASK;
		u32 rxqsts = (val >> EQOS_MTL_RXQ0_DEBUG_RXQSTS_SHIFT) &
			EQOS_MTL_RXQ0_DEBUG_RXQSTS_MASK;
		if ((!prxq) && (!rxqsts))
			break;
	}

	/* Turn off RX DMA */
	clrbits_le32(&eqos->dma_regs->ch0_rx_control,
		     EQOS_DMA_CH0_RX_CONTROL_SR);

	if (eqos->config->ops->eqos_stop_resets)
		eqos->config->ops->eqos_stop_resets(eqos);
	if (eqos->config->ops->eqos_stop_clks)
		eqos->config->ops->eqos_stop_clks(eqos);

	pr_debug("%s: OK\n", __func__);
}

static int eqos_send(struct eth_device *dev, void *packet, int length)
{
	struct eqos_priv *eqos = dev->priv;
	struct eqos_desc *tx_desc;
	uint64_t start;
	int ret;

	pr_debug("%s(dev=%p, packet=%p, length=%d):\n", __func__, dev, packet,
	      length);

	memcpy(eqos->tx_dma_buf, packet, length);
	barrier();

	tx_desc = &(eqos->tx_descs[eqos->tx_desc_idx]);
	eqos->tx_desc_idx++;
	eqos->tx_desc_idx %= EQOS_DESCRIPTORS_TX;

	tx_desc->des0 = (ulong)eqos->tx_dma_buf;
	tx_desc->des1 = 0;
	tx_desc->des2 = length;
	/*
	 * Make sure that if HW sees the _OWN write below, it will see all the
	 * writes to the rest of the descriptor too.
	 */
	barrier();

	writel(EQOS_DESC3_OWN | EQOS_DESC3_FD | EQOS_DESC3_LD | length, &tx_desc->des3);
	writel((ulong)(tx_desc + 1), &eqos->dma_regs->ch0_txdesc_tail_pointer);

	start = get_time_ns();
	ret = -ETIMEDOUT;
	do {
		if (!(readl(&tx_desc->des3) & EQOS_DESC3_OWN)) {
			ret = 0;
			break;
		}
	} while (!is_timeout(start, 100 * MSECOND));

	if (ret == -ETIMEDOUT)
		pr_debug("%s: TX timeout\n", __func__);
	return ret;
}

static int eqos_recv(struct eth_device *edev)
{
	struct eqos_priv *eqos = edev->priv;
	struct eqos_desc *rx_desc;
	u8 *buffer, *packet_expected;
	int length;

	pr_debug("%s(edev=%p):\n", __func__, edev);

	rx_desc = &(eqos->rx_descs[eqos->rx_desc_idx]);
	if (readl(&rx_desc->des3) & EQOS_DESC3_OWN) {
		pr_debug("%s: RX packet not available\n", __func__);
		return 0;
	}

	buffer = eqos->rx_dma_buf + (eqos->rx_desc_idx * EQOS_MAX_PACKET_SIZE);
	length = rx_desc->des3 & 0x7fff;
	pr_debug("%s: buffer=%p, length=%d\n", __func__, buffer, length);

	barrier(); // FIXME remove
	net_receive(edev, buffer, length);
	barrier(); // TODO use streaming mapping

	packet_expected = eqos->rx_dma_buf +
		(eqos->rx_desc_idx * EQOS_MAX_PACKET_SIZE);
	if (buffer != packet_expected) {
		debug("%s: Unexpected packet (expected %p)\n", __func__,
		      packet_expected);
		return -EINVAL;
	}

	rx_desc->des0 = (u32)(ulong)buffer;
	rx_desc->des1 = 0;
	rx_desc->des2 = 0;
	/*
	 * Make sure that if HW sees the _OWN write below, it will see all the
	 * writes to the rest of the descriptor too.
	 */
	barrier();
	rx_desc->des3 |= EQOS_DESC3_OWN | EQOS_DESC3_BUF1V;
	//dma_sync_single_for_device((dma_addr_t)rx_desc, length, DMA_FROM_DEVICE);
	barrier();

	writel((ulong)rx_desc, &eqos->dma_regs->ch0_rxdesc_tail_pointer);

	eqos->rx_desc_idx++;
	eqos->rx_desc_idx %= EQOS_DESCRIPTORS_RX;

	return 0;
}

static int eqos_probe_resources_core(struct eqos_priv *eqos)
{
	int ret;

	eqos->descs = eqos_alloc_descs(EQOS_DESCRIPTORS_TX +
				       EQOS_DESCRIPTORS_RX);
	if (!eqos->descs) {
		pr_debug("%s: eqos_alloc_descs() failed\n", __func__);
		ret = -ENOMEM;
		goto err;
	}
	eqos->tx_descs = (struct eqos_desc *)eqos->descs;
	eqos->rx_descs = (eqos->tx_descs + EQOS_DESCRIPTORS_TX);
	pr_debug("%s: tx_descs=%p, rx_descs=%p\n", __func__, eqos->tx_descs,
	      eqos->rx_descs);

	eqos->tx_dma_buf = dma_alloc_coherent(EQOS_MAX_PACKET_SIZE, DMA_ADDRESS_BROKEN);
	if (!eqos->tx_dma_buf) {
		pr_debug("%s: dma_alloc_coherent(tx_dma_buf) failed\n", __func__);
		ret = -ENOMEM;
		goto err;
	}
	pr_debug("%s: tx_dma_buf=%p\n", __func__, eqos->tx_dma_buf);

	eqos->rx_dma_buf = dma_alloc_coherent(EQOS_RX_BUFFER_SIZE, DMA_ADDRESS_BROKEN);
	if (!eqos->rx_dma_buf) {
		pr_debug("%s: dma_alloc_coherent(rx_dma_buf) failed\n", __func__);
		ret = -ENOMEM;
		goto err_free_tx_dma_buf;
	}
	pr_debug("%s: rx_dma_buf=%p\n", __func__, eqos->rx_dma_buf);

	eqos->rx_pkt = dma_alloc_coherent(EQOS_MAX_PACKET_SIZE, DMA_ADDRESS_BROKEN);
	if (!eqos->rx_pkt) {
		pr_debug("%s: dma_alloc_coherent(rx_pkt) failed\n", __func__);
		ret = -ENOMEM;
		goto err_free_rx_dma_buf;
	}
	pr_debug("%s: rx_pkt=%p\n", __func__, eqos->rx_pkt);

	pr_debug("%s: OK\n", __func__);
	return 0;

err_free_rx_dma_buf:
	free(eqos->rx_dma_buf);
err_free_tx_dma_buf:
	free(eqos->tx_dma_buf);
err:

	pr_debug("%s: returns %d\n", __func__, ret);
	return ret;
}

static int eqos_remove_resources_core(struct device_d *dev)
{
	struct eqos_priv *eqos = dev->priv;

	pr_debug("%s(dev=%p):\n", __func__, dev);

	free(eqos->rx_pkt);
	free(eqos->rx_dma_buf);
	free(eqos->tx_dma_buf);

	pr_debug("%s: OK\n", __func__);
	return 0;
}

static int eqos_probe_resources_tegra186(struct device_d *dev)
{
	struct eqos_priv *eqos = dev->priv;
	int phy_reset;
	int ret;

	pr_debug("%s(dev=%p):\n", __func__, dev);

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

	eqos->clks = xmemdup(tegra_clks, sizeof(tegra_clks));
	ret = clk_bulk_get(dev, ARRAY_SIZE(tegra_clks), eqos->clks);
	if (ret) {
		dev_err(dev, "Failed to get clks: %s\n", strerror(-ret));
		return ret;
	}

	pr_debug("%s: OK\n", __func__);
	return 0;

release_res:
	reset_control_put(eqos->reset_ctl);
	return ret;
}

static int eqos_probe_resources_stm32(struct device_d *dev)
{
	struct eqos_priv *eqos = dev->priv;
	struct device_node *np = dev->device_node;
	size_t max_clks;
	int ret;

	pr_debug("%s(dev=%p):\n", __func__, dev);

	/* Gigabit Ethernet 125MHz clock selection. */
	eqos->eth_clk_sel_reg = of_property_read_bool(np, "st,eth-clk-sel");

	/* Ethernet 50Mhz RMII clock selection */
	eqos->eth_ref_clk_sel_reg =
		of_property_read_bool(np, "st,eth-ref-clk-sel");

	max_clks = ARRAY_SIZE(stm32_clks) + 1;
	eqos->clks = xmalloc(max_clks * sizeof(*eqos->clks));
	memcpy(eqos->clks, stm32_clks, sizeof stm32_clks);

	ret = clk_bulk_get(dev, ARRAY_SIZE(stm32_clks), eqos->clks);
	if (ret) {
		dev_err(dev, "Failed to get clks: %s\n", strerror(-ret));
		return ret;
	}

#if 0
	/*  Get ETH_CLK clocks (optional) */
	ret = clk_get_by_name(dev, "eth-ck", eqos->clk_ck);
	if (ret)
		pr_warn("No phy clock provided %s\n", strerror(-ret));

	pr_debug("%s: OK\n", __func__);
#endif
	return 0;

}

static int eqos_remove_resources_tegra186(struct device_d *dev)
{
	struct eqos_priv *eqos = dev->priv;

	clk_bulk_put(ARRAY_SIZE(tegra_clks), eqos->clks);
	gpio_free(eqos->phy_reset_gpio);
	reset_control_put(eqos->reset_ctl);

	pr_debug("%s: OK\n", __func__);
	return 0;
}

static int eqos_remove_resources_stm32(struct device_d *dev)
{
	struct eqos_priv *eqos = dev->priv;

	pr_debug("%s(dev=%p):\n", __func__, dev);

	clk_bulk_put(ARRAY_SIZE(stm32_clks), eqos->clks);

	pr_debug("%s: OK\n", __func__);
	return 0;
}

static int eqos_probe(struct device_d *dev)
{
	struct eqos_priv *eqos = dev->priv;
	struct device_node *mdiobus;
	struct eth_device *edev;
	struct resource *iores;
	int ret;

	pr_debug("%s(dev=%p):\n", __func__, dev);

	eqos = xzalloc(sizeof(*eqos));

	eqos->dev = dev;
	dev->priv = eqos;
	ret = dev_get_drvdata(dev, (const void **)&eqos->config);
	if (ret)
		return ret;

	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return PTR_ERR(iores);
	eqos->regs = IOMEM(iores->start);

	eqos->mac_regs = (void *)(eqos->regs + EQOS_MAC_REGS_BASE);
	eqos->mtl_regs = (void *)(eqos->regs + EQOS_MTL_REGS_BASE);
	eqos->dma_regs = (void *)(eqos->regs + EQOS_DMA_REGS_BASE);
	eqos->tegra186_regs = (void *)(eqos->regs + EQOS_TEGRA186_REGS_BASE);

	ret = eqos_probe_resources_core(eqos);
	if (ret < 0) {
		pr_err("eqos_probe_resources_core() failed: %s\n", strerror(-ret));
		return ret;
	}

	ret = eqos->config->ops->eqos_probe_resources(dev);
	if (ret < 0) {
		pr_err("eqos_probe_resources() failed: %s\n", strerror(-ret));
		goto err_remove_resources_core;
	}

	if (ret < 0) {
		pr_err("mdio_register() failed: %s\n", strerror(-ret));
		goto err_remove_resources_tegra;
	}

	edev = &eqos->edev;
	edev->priv = eqos;

	edev->parent = dev;
	edev->open = eqos_start;
	edev->send = eqos_send;
	edev->recv = eqos_recv;
	edev->halt = eqos_stop;
	edev->get_ethaddr = eqos_get_hwaddr;
	edev->set_ethaddr = eqos_set_hwaddr;

	ret = of_get_phy_mode(dev->device_node);
	if (ret < 0)
		eqos->phy_interface = PHY_INTERFACE_MODE_MII;
	else
		eqos->phy_interface = ret;

	eqos->phy_addr = -1;
	mdiobus = of_get_child_by_name(dev->device_node, "mdio");
	if (mdiobus)
		eqos->miibus.dev.device_node = mdiobus;

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

	eqos->miibus.parent = dev;
	eqos->miibus.read = eqos_mdio_read;
	eqos->miibus.write = eqos_mdio_write;
	eqos->miibus.priv = eqos;

	mdiobus_register(&eqos->miibus);
	eth_register(edev);

	pr_debug("%s: OK\n", __func__);
	return 0;

err_remove_resources_tegra:
	eqos->config->ops->eqos_remove_resources(dev);
err_remove_resources_core:
	eqos_remove_resources_core(dev);

	pr_debug("%s: returns %d\n", __func__, ret);
	return ret;
}

static void eqos_remove(struct device_d *dev)
{
	struct eqos_priv *eqos = dev->priv;

	pr_debug("%s(dev=%p):\n", __func__, dev);

	mdiobus_unregister(&eqos->miibus);
	eqos->config->ops->eqos_remove_resources(dev);

	eqos_probe_resources_core(eqos);

	pr_debug("%s: OK\n", __func__);
}

static struct eqos_ops eqos_tegra186_ops = {
	.eqos_probe_resources = eqos_probe_resources_tegra186,
	.eqos_remove_resources = eqos_remove_resources_tegra186,
	.eqos_stop_resets = eqos_stop_resets_tegra186,
	.eqos_start_resets = eqos_start_resets_tegra186,
	.eqos_stop_clks = eqos_stop_clks_tegra186,
	.eqos_start_clks = eqos_start_clks_tegra186,
	.eqos_calibrate_pads = eqos_calibrate_pads_tegra186,
	.eqos_disable_calibration = eqos_disable_calibration_tegra186,
	.eqos_set_tx_clk_speed = eqos_set_tx_clk_speed_tegra186,
	.eqos_get_tick_clk_rate = eqos_get_tick_clk_rate_tegra186
};

static const struct eqos_config eqos_tegra186_config = {
	.reg_access_always_ok = false,
	.mdio_wait = 10,
	.config_mac = EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_DCB,
	.config_mac_mdio = EQOS_MAC_MDIO_ADDRESS_CR_20_35,
	.ops = &eqos_tegra186_ops
};

static struct eqos_ops eqos_stm32_ops = {
	.eqos_probe_resources = eqos_probe_resources_stm32,
	.eqos_remove_resources = eqos_remove_resources_stm32,
	.eqos_stop_clks = eqos_stop_clks_stm32,
	.eqos_start_clks = eqos_start_clks_stm32,
	.eqos_get_tick_clk_rate = eqos_get_tick_clk_rate_stm32,
	.eqos_set_mode = eqos_set_mode_stm32,
};

static const struct eqos_config eqos_stm32_config = {
	.reg_access_always_ok = false,
	.mdio_wait = 10000,
	.config_mac = EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_AV,
	.config_mac_mdio = EQOS_MAC_MDIO_ADDRESS_CR_250_300,
	.ops = &eqos_stm32_ops
};

static const struct of_device_id eqos_ids[] = {
	{
		.compatible = "nvidia,tegra186-eqos",
		.data = &eqos_tegra186_config
	},
	{
		.compatible = "snps,dwmac-4.20a",
		.data = &eqos_stm32_config
	},
	{ }
};

static struct driver_d stm32_dwc_ether_driver = {
	.name = "eth_eqos",
	.probe = eqos_probe,
	.remove	= eqos_remove,
	.of_compatible = DRV_OF_COMPAT(eqos_ids),
};
device_platform_driver(stm32_dwc_ether_driver);

