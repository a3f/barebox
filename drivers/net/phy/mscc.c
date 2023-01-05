// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/*
 * Driver for Microsemi VSC85xx PHYs
 *
 * Author: Nagaraju Lakkaraju
 * License: Dual MIT/GPL
 * Copyright (c) 2016 Microsemi Corporation
 */

#include <common.h>
#include <init.h>
#include <linux/clk.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/mdio.h>
#include <linux/micrel_phy.h>
#include <linux/bitfield.h>
#include <dt-bindings/net/mscc-phy-vsc8531.h>
#include <linux/mutex.h>

enum rgmii_clock_delay {
	RGMII_CLK_DELAY_0_2_NS = 0,
	RGMII_CLK_DELAY_0_8_NS = 1,
	RGMII_CLK_DELAY_1_1_NS = 2,
	RGMII_CLK_DELAY_1_7_NS = 3,
	RGMII_CLK_DELAY_2_0_NS = 4,
	RGMII_CLK_DELAY_2_3_NS = 5,
	RGMII_CLK_DELAY_2_6_NS = 6,
	RGMII_CLK_DELAY_3_4_NS = 7
};

/* Microsemi VSC85xx PHY registers */

#define MSCC_PHY_EXT_CNTL_STATUS          22
#define SMI_BROADCAST_WR_EN		  0x0001

#define MSCC_PHY_EXT_PHY_CNTL_1           23
#define MAC_IF_SELECTION_MASK             0x1800
#define MAC_IF_SELECTION_GMII             0
#define MAC_IF_SELECTION_RMII             1
#define MAC_IF_SELECTION_RGMII            2
#define MAC_IF_SELECTION_POS              11

#define MSCC_PHY_WOL_MAC_CONTROL          27
#define EDGE_RATE_CNTL_POS                5
#define EDGE_RATE_CNTL_MASK               0x00E0

/* Std Page Register 28 - PHY AUX Control/Status */
#define MSCC_PHY_DEV_AUX_CNTL		28
#define AUX_CNTL_SPEED_100M		0x0008
#define AUX_CNTL_SPEED_1000M		0x0010
#define AUX_CNTL_F_DUPLEX		0x0020

#define MSCC_PHY_LED_MODE_SEL		  29
#define LED_MODE_SEL_POS(x)		  ((x) * 4)
#define LED_MODE_SEL_MASK(x)		  (GENMASK(3, 0) << LED_MODE_SEL_POS(x))
#define LED_MODE_SEL(x, mode)		  (((mode) << LED_MODE_SEL_POS(x)) & LED_MODE_SEL_MASK(x))

#define MSCC_EXT_PAGE_ACCESS		  31
#define MSCC_PHY_PAGE_STANDARD		  0x0000 /* Standard registers */
#define MSCC_PHY_PAGE_EXTENDED		  0x0001 /* Extended registers */
#define MSCC_PHY_PAGE_EXTENDED_2	  0x0002 /* Extended reg - page 2 */
/* Extended reg - GPIO; this is a bank of registers that are shared for all PHYs
 * in the same package.
 */
#define MSCC_PHY_PAGE_TEST		  0x2a30 /* Test reg */
#define MSCC_PHY_PAGE_TR		  0x52b5 /* Token ring registers */

/* RGMII controls at address 20E2, for VSC8502 and similar */
#define VSC8502_RGMII_CNTL		  20
#define VSC8502_RGMII_RX_DELAY_MASK	  0x0070
#define VSC8502_RGMII_TX_DELAY_MASK	  0x0007

/* x Address in range 1-4 */

/* Test page Registers */
#define MSCC_PHY_TEST_PAGE_5		  5
#define MSCC_PHY_TEST_PAGE_8		  8
#define TR_CLK_DISABLE			  0x8000
#define MSCC_PHY_TEST_PAGE_9		  9
#define MSCC_PHY_TEST_PAGE_20		  20
#define MSCC_PHY_TEST_PAGE_24		  24

/* Token ring page Registers */
#define MSCC_PHY_TR_CNTL		  16
#define TR_WRITE			  0x8000
#define TR_ADDR(x)			  (0x7fff & (x))
#define MSCC_PHY_TR_LSB			  17
#define MSCC_PHY_TR_MSB			  18

/* Microsemi PHY ID's
 *   Code assumes lowest nibble is 0
 */
#define PHY_ID_VSC8502			  0x00070630
#define PHY_ID_VSC8530			  0x00070560
#define PHY_ID_VSC8531			  0x00070570
#define PHY_ID_VSC8540			  0x00070760
#define PHY_ID_VSC8541			  0x00070770

#define MSCC_VDDMAC_1500		  1500
#define MSCC_VDDMAC_1800		  1800
#define MSCC_VDDMAC_2500		  2500
#define MSCC_VDDMAC_3300		  3300

#define MAX_LEDS			  4

#define VSC85XX_SUPP_LED_MODES (BIT(VSC8531_LINK_ACTIVITY) | \
				BIT(VSC8531_LINK_1000_ACTIVITY) | \
				BIT(VSC8531_LINK_100_ACTIVITY) | \
				BIT(VSC8531_LINK_10_ACTIVITY) | \
				BIT(VSC8531_LINK_100_1000_ACTIVITY) | \
				BIT(VSC8531_LINK_10_1000_ACTIVITY) | \
				BIT(VSC8531_LINK_10_100_ACTIVITY) | \
				BIT(VSC8531_DUPLEX_COLLISION) | \
				BIT(VSC8531_COLLISION) | \
				BIT(VSC8531_ACTIVITY) | \
				BIT(VSC8531_AUTONEG_FAULT) | \
				BIT(VSC8531_SERIAL_MODE) | \
				BIT(VSC8531_FORCE_LED_OFF) | \
				BIT(VSC8531_FORCE_LED_ON))

struct reg_val {
	u16	reg;
	u32	val;
};

struct vsc8531_private {
	int rate_magic;
	u16 supp_led_modes;
	u32 leds_mode[MAX_LEDS];
	u8 nleds;
	/* PHY address within the package. */
	u8 addr;
};

/* Shared structure between the PHYs of the same package.
 * gpio_lock: used for PHC operations. Common for all PHYs as the load/save GPIO
 * is shared.
 */
struct vsc85xx_shared_private {
	struct mutex gpio_lock;
};

struct vsc8531_edge_rate_table {
	u32 vddmac;
	u32 slowdown[8];
};

enum csr_target {
	MACRO_CTRL  = 0x07,
};

u32 vsc85xx_csr_read(struct phy_device *phydev,
		     enum csr_target target, u32 reg);

int vsc85xx_csr_write(struct phy_device *phydev,
		      enum csr_target target, u32 reg, u32 val);

int phy_base_write(struct phy_device *phydev, u32 regnum, u16 val);
int phy_base_read(struct phy_device *phydev, u32 regnum);
int phy_commit_mcb_s6g(struct phy_device *phydev, u32 reg, u8 mcb);
int vsc8584_cmd(struct phy_device *phydev, u16 val);

static const struct vsc8531_edge_rate_table edge_table[] = {
	{MSCC_VDDMAC_3300, { 0, 2,  4,  7, 10, 17, 29, 53} },
	{MSCC_VDDMAC_2500, { 0, 3,  6, 10, 14, 23, 37, 63} },
	{MSCC_VDDMAC_1800, { 0, 5,  9, 16, 23, 35, 52, 76} },
	{MSCC_VDDMAC_1500, { 0, 6, 14, 21, 29, 42, 58, 77} },
};

static int vsc85xx_phy_read_page(struct phy_device *phydev)
{
	return phy_read(phydev, MSCC_EXT_PAGE_ACCESS);
}

static int vsc85xx_phy_write_page(struct phy_device *phydev, int page)
{
	return phy_write(phydev, MSCC_EXT_PAGE_ACCESS, page);
}

static int vsc85xx_led_cntl_set(struct phy_device *phydev,
				u8 led_num,
				u8 mode)
{
	int rc;
	u16 reg_val;

	mutex_lock(&phydev->lock);
	reg_val = phy_read(phydev, MSCC_PHY_LED_MODE_SEL);
	reg_val &= ~LED_MODE_SEL_MASK(led_num);
	reg_val |= LED_MODE_SEL(led_num, (u16)mode);
	rc = phy_write(phydev, MSCC_PHY_LED_MODE_SEL, reg_val);
	mutex_unlock(&phydev->lock);

	return rc;
}

static int vsc85xx_edge_rate_magic_get(struct phy_device *phydev)
{
	u32 vdd, sd;
	int i, j;
	struct device *dev = &phydev->dev;
	struct device_node *of_node = dev->of_node;
	u8 sd_array_size = ARRAY_SIZE(edge_table[0].slowdown);

	if (!of_node)
		return -ENODEV;

	if (of_property_read_u32(of_node, "vsc8531,vddmac", &vdd))
		vdd = MSCC_VDDMAC_3300;

	if (of_property_read_u32(of_node, "vsc8531,edge-slowdown", &sd))
		sd = 0;

	for (i = 0; i < ARRAY_SIZE(edge_table); i++)
		if (edge_table[i].vddmac == vdd)
			for (j = 0; j < sd_array_size; j++)
				if (edge_table[i].slowdown[j] == sd)
					return (sd_array_size - j - 1);

	return -EINVAL;
}

static int vsc85xx_dt_led_mode_get(struct phy_device *phydev,
				   char *led,
				   u32 default_mode)
{
	struct vsc8531_private *priv = phydev->priv;
	struct device *dev = &phydev->dev;
	struct device_node *of_node = dev->of_node;
	u32 led_mode;
	int err;

	if (!of_node)
		return -ENODEV;

	led_mode = default_mode;
	err = of_property_read_u32(of_node, led, &led_mode);
	if (!err && !(BIT(led_mode) & priv->supp_led_modes)) {
		dev_err(&phydev->dev, "DT %s invalid\n", led);
		return -EINVAL;
	}

	return led_mode;
}

static int vsc85xx_dt_led_modes_get(struct phy_device *phydev,
				    u32 *default_mode)
{
	struct vsc8531_private *priv = phydev->priv;
	char led_dt_prop[28];
	int i, ret;

	for (i = 0; i < priv->nleds; i++) {
		ret = sprintf(led_dt_prop, "vsc8531,led-%d-mode", i);
		if (ret < 0)
			return ret;

		ret = vsc85xx_dt_led_mode_get(phydev, led_dt_prop,
					      default_mode[i]);
		if (ret < 0)
			return ret;
		priv->leds_mode[i] = ret;
	}

	return 0;
}

static int vsc85xx_edge_rate_cntl_set(struct phy_device *phydev, u8 edge_rate)
{
	int rc;

	mutex_lock(&phydev->lock);
	rc = phy_modify_paged(phydev, MSCC_PHY_PAGE_EXTENDED_2,
			      MSCC_PHY_WOL_MAC_CONTROL, EDGE_RATE_CNTL_MASK,
			      edge_rate << EDGE_RATE_CNTL_POS);
	mutex_unlock(&phydev->lock);

	return rc;
}

static int mscc_phy_soft_reset(struct phy_device *phydev)
{
	int oldpage, ret = 0;
	u64 start;

	oldpage = phy_select_page(phydev, MSCC_PHY_PAGE_STANDARD);
	if (oldpage < 0)
		return oldpage;

	phy_modify(phydev, MII_BMCR, BMCR_RESET, BMCR_RESET);

	start = get_time_ns();
	do {
		u16 reg_val;

		reg_val = phy_read(phydev, MII_BMCR);
		if (!(reg_val & BMCR_RESET))
			goto out;

		udelay(1000);   /* 1 ms */
	} while (!is_timeout(start, 100 * MSECOND));

	dev_warn(&phydev->dev, "soft reset failed after setting mac i/f = 0x%x\n",
		 phydev->interface);

	ret = -ETIMEDOUT;
out:
	phy_restore_page(phydev, oldpage, oldpage);
	return ret;
}

static int vsc85xx_mac_if_set(struct phy_device *phydev,
			      phy_interface_t interface)
{
	int rc;
	u16 reg_val;

	mutex_lock(&phydev->lock);
	reg_val = phy_read(phydev, MSCC_PHY_EXT_PHY_CNTL_1);
	reg_val &= ~(MAC_IF_SELECTION_MASK);
	switch (interface) {
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII:
		reg_val |= (MAC_IF_SELECTION_RGMII << MAC_IF_SELECTION_POS);
		break;
	case PHY_INTERFACE_MODE_RMII:
		reg_val |= (MAC_IF_SELECTION_RMII << MAC_IF_SELECTION_POS);
		break;
	case PHY_INTERFACE_MODE_MII:
	case PHY_INTERFACE_MODE_GMII:
		reg_val |= (MAC_IF_SELECTION_GMII << MAC_IF_SELECTION_POS);
		break;
	default:
		rc = -EINVAL;
		goto out_unlock;
	}
	rc = phy_write(phydev, MSCC_PHY_EXT_PHY_CNTL_1, reg_val);
	if (rc)
		goto out_unlock;

	rc = mscc_phy_soft_reset(phydev);

out_unlock:
	mutex_unlock(&phydev->lock);

	return rc;
}

/* Set the RGMII RX and TX clock skews individually, according to the PHY
 * interface type, to:
 *  * 0.2 ns (their default, and lowest, hardware value) if delays should
 *    not be enabled
 *  * 2.0 ns (which causes the data to be sampled at exactly half way between
 *    clock transitions at 1000 Mbps) if delays should be enabled
 */
static int vsc85xx_rgmii_set_skews(struct phy_device *phydev, u32 rgmii_cntl,
				   u16 rgmii_rx_delay_mask,
				   u16 rgmii_tx_delay_mask)
{
	u16 rgmii_rx_delay_pos = ffs(rgmii_rx_delay_mask) - 1;
	u16 rgmii_tx_delay_pos = ffs(rgmii_tx_delay_mask) - 1;
	u16 reg_val = 0;
	int rc;

	mutex_lock(&phydev->lock);

	if (phydev->interface == PHY_INTERFACE_MODE_RGMII_RXID ||
	    phydev->interface == PHY_INTERFACE_MODE_RGMII_ID)
		reg_val |= RGMII_CLK_DELAY_2_0_NS << rgmii_rx_delay_pos;
	if (phydev->interface == PHY_INTERFACE_MODE_RGMII_TXID ||
	    phydev->interface == PHY_INTERFACE_MODE_RGMII_ID)
		reg_val |= RGMII_CLK_DELAY_2_0_NS << rgmii_tx_delay_pos;

	rc = phy_modify_paged(phydev, MSCC_PHY_PAGE_EXTENDED_2,
			      rgmii_cntl,
			      rgmii_rx_delay_mask | rgmii_tx_delay_mask,
			      reg_val);

	mutex_unlock(&phydev->lock);

	return rc;
}

static int vsc85xx_default_config(struct phy_device *phydev)
{
	int rc;

	if (phy_interface_mode_is_rgmii(phydev->interface)) {
		rc = vsc85xx_rgmii_set_skews(phydev, VSC8502_RGMII_CNTL,
					     VSC8502_RGMII_RX_DELAY_MASK,
					     VSC8502_RGMII_TX_DELAY_MASK);
		if (rc)
			return rc;
	}

	return 0;
}

/* mdiobus lock should be locked when using this function */
static void vsc85xx_tr_write(struct phy_device *phydev, u16 addr, u32 val)
{
	phy_write(phydev, MSCC_PHY_TR_MSB, val >> 16);
	phy_write(phydev, MSCC_PHY_TR_LSB, val & GENMASK(15, 0));
	phy_write(phydev, MSCC_PHY_TR_CNTL, TR_WRITE | TR_ADDR(addr));
}

static int vsc8531_pre_init_seq_set(struct phy_device *phydev)
{
	int rc;
	static const struct reg_val init_seq[] = {
		{0x0f90, 0x00688980},
		{0x0696, 0x00000003},
		{0x07fa, 0x0050100f},
		{0x1686, 0x00000004},
	};
	unsigned int i;
	int oldpage;

	rc = phy_modify_paged(phydev, MSCC_PHY_PAGE_STANDARD,
			      MSCC_PHY_EXT_CNTL_STATUS, SMI_BROADCAST_WR_EN,
			      SMI_BROADCAST_WR_EN);
	if (rc < 0)
		return rc;
	rc = phy_modify_paged(phydev, MSCC_PHY_PAGE_TEST,
			      MSCC_PHY_TEST_PAGE_24, 0, 0x0400);
	if (rc < 0)
		return rc;
	rc = phy_modify_paged(phydev, MSCC_PHY_PAGE_TEST,
			      MSCC_PHY_TEST_PAGE_5, 0x0a00, 0x0e00);
	if (rc < 0)
		return rc;
	rc = phy_modify_paged(phydev, MSCC_PHY_PAGE_TEST,
			      MSCC_PHY_TEST_PAGE_8, TR_CLK_DISABLE, TR_CLK_DISABLE);
	if (rc < 0)
		return rc;

	mutex_lock(&phydev->lock);
	oldpage = phy_select_page(phydev, MSCC_PHY_PAGE_TR);
	if (oldpage < 0)
		goto out_unlock;

	for (i = 0; i < ARRAY_SIZE(init_seq); i++)
		vsc85xx_tr_write(phydev, init_seq[i].reg, init_seq[i].val);

out_unlock:
	oldpage = phy_restore_page(phydev, oldpage, oldpage);
	mutex_unlock(&phydev->lock);

	return oldpage;
}

static int vsc85xx_eee_init_seq_set(struct phy_device *phydev)
{
	static const struct reg_val init_eee[] = {
		{0x0f82, 0x0012b00a},
		{0x1686, 0x00000004},
		{0x168c, 0x00d2c46f},
		{0x17a2, 0x00000620},
		{0x16a0, 0x00eeffdd},
		{0x16a6, 0x00071448},
		{0x16a4, 0x0013132f},
		{0x16a8, 0x00000000},
		{0x0ffc, 0x00c0a028},
		{0x0fe8, 0x0091b06c},
		{0x0fea, 0x00041600},
		{0x0f80, 0x00000af4},
		{0x0fec, 0x00901809},
		{0x0fee, 0x0000a6a1},
		{0x0ffe, 0x00b01007},
		{0x16b0, 0x00eeff00},
		{0x16b2, 0x00007000},
		{0x16b4, 0x00000814},
	};
	unsigned int i;
	int oldpage;

	mutex_lock(&phydev->lock);
	oldpage = phy_select_page(phydev, MSCC_PHY_PAGE_TR);
	if (oldpage < 0)
		goto out_unlock;

	for (i = 0; i < ARRAY_SIZE(init_eee); i++)
		vsc85xx_tr_write(phydev, init_eee[i].reg, init_eee[i].val);

out_unlock:
	oldpage = phy_restore_page(phydev, oldpage, oldpage);
	mutex_unlock(&phydev->lock);

	return oldpage;
}

static int vsc85xx_config_init(struct phy_device *phydev)
{
	int rc, i, phy_id;
	struct vsc8531_private *vsc8531 = phydev->priv;
	struct phy_driver *drv = to_phy_driver(phydev->dev.driver);

	rc = vsc85xx_default_config(phydev);
	if (rc)
		return rc;

	rc = vsc85xx_mac_if_set(phydev, phydev->interface);
	if (rc)
		return rc;

	rc = vsc85xx_edge_rate_cntl_set(phydev, vsc8531->rate_magic);
	if (rc)
		return rc;

	phy_id = drv->phy_id & drv->phy_id_mask;
	if (PHY_ID_VSC8531 == phy_id || PHY_ID_VSC8541 == phy_id ||
	    PHY_ID_VSC8530 == phy_id || PHY_ID_VSC8540 == phy_id) {
		rc = vsc8531_pre_init_seq_set(phydev);
		if (rc)
			return rc;
	}

	rc = vsc85xx_eee_init_seq_set(phydev);
	if (rc)
		return rc;

	for (i = 0; i < vsc8531->nleds; i++) {
		rc = vsc85xx_led_cntl_set(phydev, i, vsc8531->leds_mode[i]);
		if (rc)
			return rc;
	}

	return 0;
}

static int vsc85xx_read_status(struct phy_device *phydev)
{
	int status;
	int ret;

	ret = genphy_update_link(phydev);
	if (ret)
		return ret;

	status = phy_read(phydev, MSCC_PHY_DEV_AUX_CNTL);
	if (status < 0)
		return status;

	phydev->speed = SPEED_10;
	phydev->duplex = DUPLEX_HALF;

	if (status & AUX_CNTL_SPEED_1000M)
		phydev->speed = SPEED_1000;
	else if (status & AUX_CNTL_SPEED_100M)
		phydev->speed = SPEED_100;

	if (status & AUX_CNTL_F_DUPLEX)
		phydev->duplex = DUPLEX_FULL;

	printf("status = %08x\n %d %d\n", status, (status & AUX_CNTL_SPEED_100M), status & AUX_CNTL_F_DUPLEX);

	return 0;
}

static int vsc85xx_probe(struct phy_device *phydev)
{
	struct vsc8531_private *vsc8531;
	int rate_magic;
	u32 default_mode[2] = {VSC8531_LINK_1000_ACTIVITY,
	   VSC8531_LINK_100_ACTIVITY};

	rate_magic = vsc85xx_edge_rate_magic_get(phydev);
	if (rate_magic < 0)
		return rate_magic;

	vsc8531 = xzalloc(sizeof(*vsc8531));

	phydev->priv = vsc8531;

	vsc8531->rate_magic = rate_magic;
	vsc8531->nleds = 2;
	vsc8531->supp_led_modes = VSC85XX_SUPP_LED_MODES;

	return vsc85xx_dt_led_modes_get(phydev, default_mode);
}

/* Microsemi VSC85xx PHYs */
static struct phy_driver vsc85xx_driver[] = {
{
	.phy_id		= PHY_ID_VSC8502,
	.drv.name	= "Microsemi GE VSC8502 SyncE",
	.phy_id_mask	= 0xfffffff0,
	/* PHY_BASIC_FEATURES */
	.config_init	= &vsc85xx_config_init,
	.read_status	= &vsc85xx_read_status,
	.probe		= &vsc85xx_probe,
	.read_page	= &vsc85xx_phy_read_page,
	.write_page	= &vsc85xx_phy_write_page,
},
{
	.phy_id		= PHY_ID_VSC8530,
	.drv.name	= "Microsemi FE VSC8530",
	.phy_id_mask	= 0xfffffff0,
	/* PHY_BASIC_FEATURES */
	.config_init	= &vsc85xx_config_init,
	.read_status	= &vsc85xx_read_status,
	.probe		= &vsc85xx_probe,
	.read_page	= &vsc85xx_phy_read_page,
	.write_page	= &vsc85xx_phy_write_page,
},
{
	.phy_id		= PHY_ID_VSC8531,
	.drv.name	= "Microsemi VSC8531",
	.phy_id_mask    = 0xfffffff0,
	/* PHY_GBIT_FEATURES */
	.config_init    = &vsc85xx_config_init,
	.read_status	= &vsc85xx_read_status,
	.probe		= &vsc85xx_probe,
	.read_page	= &vsc85xx_phy_read_page,
	.write_page	= &vsc85xx_phy_write_page,
},
{
	.phy_id		= PHY_ID_VSC8540,
	.drv.name	= "Microsemi FE VSC8540 SyncE",
	.phy_id_mask	= 0xfffffff0,
	/* PHY_BASIC_FEATURES */
	.config_init	= &vsc85xx_config_init,
	.read_status	= &vsc85xx_read_status,
	.probe		= &vsc85xx_probe,
	.read_page	= &vsc85xx_phy_read_page,
	.write_page	= &vsc85xx_phy_write_page,
},
{
	.phy_id		= PHY_ID_VSC8541,
	.drv.name	= "Microsemi VSC8541 SyncE",
	.phy_id_mask    = 0xfffffff0,
	/* PHY_GBIT_FEATURES */
	.config_init    = &vsc85xx_config_init,
	.read_status	= &vsc85xx_read_status,
	.probe		= &vsc85xx_probe,
	.read_page	= &vsc85xx_phy_read_page,
	.write_page	= &vsc85xx_phy_write_page,
},
};

device_phy_drivers(vsc85xx_driver);
