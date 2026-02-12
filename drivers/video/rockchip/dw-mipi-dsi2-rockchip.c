// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 * Author:
 *      Guochun Huang <hero.huang@rock-chips.com>
 *      Heiko Stuebner <heiko.stuebner@cherry.de>
 */
 

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <video/media-bus-format.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <of.h>
#include <of_device.h>
#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <mfd/syscon.h>
#include <linux/phy/phy.h>

#include <video/mipi_dsi.h> 
#include <video/dw_mipi_dsi2.h>

#define DRM_DEV_ERROR dev_err
#define DRM_DEV_DEBUG dev_dbg

#define PSEC_PER_SEC			1000000000000LL

struct dsigrf_reg {
	u16 offset;
	u16 lsb;
	u16 msb;
};

enum grf_reg_fields {
	TXREQCLKHS_EN,
	GATING_EN,
	IPI_SHUTDN,
	IPI_COLORM,
	IPI_COLOR_DEPTH,
	IPI_FORMAT,
	MAX_FIELDS,
};

#define IPI_DEPTH_5_6_5_BITS		0x02
#define IPI_DEPTH_6_BITS		0x03
#define IPI_DEPTH_8_BITS		0x05
#define IPI_DEPTH_10_BITS		0x06

struct rockchip_dw_dsi2_chip_data {
	u32 reg;
	const struct dsigrf_reg *grf_regs;
	unsigned long long max_bit_rate_per_lane;
};

struct dw_mipi_dsi2_rockchip {
	struct device *dev;
	struct regmap *regmap;

	unsigned int lane_mbps; /* per lane */
	u32 format;

	struct regmap *grf_regmap;
	struct phy *phy;
	union phy_configure_opts phy_opts;

	struct dw_mipi_dsi2 *dmd;
	struct dw_mipi_dsi2_plat_data pdata;
	const struct rockchip_dw_dsi2_chip_data *cdata;
};

static void grf_field_write(struct dw_mipi_dsi2_rockchip *dsi2, enum grf_reg_fields index,
			    unsigned int val)
{
	const struct dsigrf_reg *field = &dsi2->cdata->grf_regs[index];

	if (!field)
		return;

	regmap_write(dsi2->grf_regmap, field->offset,
		     (val << field->lsb) | (GENMASK(field->msb, field->lsb) << 16));
}

static void dw_mipi_dsi2_configure_color_depth(struct dw_mipi_dsi2_rockchip *dsi2) {
	u32 color_depth;

	switch (dsi2->format) {
	case MIPI_DSI_FMT_RGB666:
	case MIPI_DSI_FMT_RGB666_PACKED:
		color_depth = IPI_DEPTH_6_BITS;
		break;
	case MIPI_DSI_FMT_RGB565:
		color_depth = IPI_DEPTH_5_6_5_BITS;
		break;
	case MIPI_DSI_FMT_RGB888:
		color_depth = IPI_DEPTH_8_BITS;
		break;
	default:
		dev_err(dsi2->dev, "unknown format for dsi2: %d", dsi2->format);
		return;
	}

	/* Only used if DSI host is operating in auto mode */
	grf_field_write(dsi2, IPI_COLOR_DEPTH, color_depth);
}

static int dw_mipi_dsi2_phy_init(void *priv_data)
{
	return 0;
}

static void dw_mipi_dsi2_phy_power_on(void *priv_data)
{
	struct dw_mipi_dsi2_rockchip *dsi2 = priv_data;
	int ret;

	ret = phy_set_mode(dsi2->phy, PHY_MODE_MIPI_DPHY);
	if (ret) {
		dev_err(dsi2->dev, "Failed to set phy mode: %d\n", ret);
		return;
	}

	phy_configure(dsi2->phy, &dsi2->phy_opts);
	phy_power_on(dsi2->phy);

	/*
	 * In linux, this is configured in the encoder's atomic_enable
	 * method, but as we're not using drm_encoder, this can 
	 * be configured here instead.
	 */
	dw_mipi_dsi2_configure_color_depth(dsi2);
}

static void dw_mipi_dsi2_phy_power_off(void *priv_data)
{
	struct dw_mipi_dsi2_rockchip *dsi2 = priv_data;

	phy_power_off(dsi2->phy);
}

static int
dw_mipi_dsi2_get_lane_mbps(void *priv_data, const struct drm_display_mode *mode,
			   unsigned long mode_flags, u32 lanes, u32 format,
			   unsigned int *lane_mbps)
{
	struct dw_mipi_dsi2_rockchip *dsi2 = priv_data;
	u64 max_lane_rate, target_phyclk;
	unsigned int lane_rate_kbps;
	int bpp;

	max_lane_rate = dsi2->cdata->max_bit_rate_per_lane;

	dsi2->format = format;
	bpp = mipi_dsi_pixel_format_to_bpp(format);
	if (bpp < 0) {
		dev_err(dsi2->dev, "failed to get bpp for pixel format %d\n", format);
		return bpp;
	}

	lane_rate_kbps = mode->clock * bpp / lanes;

	/*
	 * Set BW a little larger only in video burst mode in
	 * consideration of the protocol overhead and HS mode
	 * switching to BLLP mode, take 1 / 0.9, since Mbps must
	 * big than bandwidth of RGB
	 */
	if (mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
		lane_rate_kbps = (lane_rate_kbps * 10) / 9;

	if (lane_rate_kbps > max_lane_rate) {
		dev_err(dsi2->dev, "DPHY clock frequency is out of range\n");
		return -ERANGE;
	}

	dsi2->lane_mbps = lane_rate_kbps / 1000;
	*lane_mbps = dsi2->lane_mbps;

	if (dsi2->phy) {
		target_phyclk = DIV_ROUND_CLOSEST_ULL(lane_rate_kbps * lanes * 1000, bpp);
		phy_mipi_dphy_get_default_config(target_phyclk, bpp, lanes,
						 &dsi2->phy_opts.mipi_dphy);
	}

	return 0;
}

static void dw_mipi_dsi2_phy_get_iface(void *priv_data, struct dw_mipi_dsi2_phy_iface *iface)
{
	/* PPI width is fixed to 16 bits in DCPHY */
	iface->ppi_width = 16;
	iface->phy_type = DW_MIPI_DSI2_DPHY;
}

static int
dw_mipi_dsi2_phy_get_timing(void *priv_data, unsigned int lane_mbps,
			    struct dw_mipi_dsi2_phy_timing *timing)
{
	struct dw_mipi_dsi2_rockchip *dsi2 = priv_data;
	struct phy_configure_opts_mipi_dphy *cfg = &dsi2->phy_opts.mipi_dphy;
	unsigned long long tmp, ui;
	unsigned long long hstx_clk;

	hstx_clk = DIV_ROUND_CLOSEST_ULL(dsi2->lane_mbps * USEC_PER_SEC, 16);

	ui = ALIGN(PSEC_PER_SEC, hstx_clk);
	do_div(ui, hstx_clk);

	/* PHY_LP2HS_TIME = (TLPX + THS-PREPARE + THS-ZERO) / Tphy_hstx_clk */
	tmp = cfg->lpx + cfg->hs_prepare + cfg->hs_zero;
	tmp = DIV_ROUND_CLOSEST_ULL(tmp << 16, ui);
	timing->data_lp2hs = tmp;

	/* PHY_HS2LP_TIME = (THS-TRAIL + THS-EXIT) / Tphy_hstx_clk */
	tmp = cfg->hs_trail + cfg->hs_exit;
	tmp = DIV_ROUND_CLOSEST_ULL(tmp << 16, ui);
	timing->data_hs2lp = tmp;

	return 0;
}

static const struct dw_mipi_dsi2_phy_ops dw_mipi_dsi2_rockchip_phy_ops = {
	.init = dw_mipi_dsi2_phy_init,
	.power_on = dw_mipi_dsi2_phy_power_on,
	.power_off = dw_mipi_dsi2_phy_power_off,
	.get_interface = dw_mipi_dsi2_phy_get_iface,
	.get_lane_mbps = dw_mipi_dsi2_get_lane_mbps,
	.get_timing = dw_mipi_dsi2_phy_get_timing,
};

static const struct dw_mipi_dsi2_host_ops dw_mipi_dsi2_rockchip_host_ops = {
};

static const struct regmap_config dw_mipi_dsi2_rockchip_regmap_config = {
	.name = "dsi2-host",
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static int dw_mipi_dsi2_rockchip_probe(struct device *dev)
{
	struct device_node *np = dev->of_node;
	const struct rockchip_dw_dsi2_chip_data *cdata =
						of_device_get_match_data(dev);
	struct dw_mipi_dsi2_rockchip *dsi2;
	struct resource *res;
	void __iomem *base;
	int i;

	dsi2 = calloc(sizeof(*dsi2), 1);
	if (!dsi2)
		return -ENOMEM;

	base = dev_platform_get_and_ioremap_resource(dev, 0, &res);
	if (IS_ERR(base)) {
		return dev_err_probe(dev, PTR_ERR(base), "unable to get dsi registers\n");
	}

	dsi2->regmap = regmap_init_mmio(dev, base, &dw_mipi_dsi2_rockchip_regmap_config);
	if (IS_ERR(dsi2->regmap))
		return dev_err_probe(dev, PTR_ERR(dsi2->regmap), "failed to init register map\n");

	i = 0;
	while (cdata[i].reg) {
		if (cdata[i].reg == res->start) {
			dsi2->cdata = &cdata[i];
			break;
		}

		i++;
	}

	if (!dsi2->cdata)
		return dev_err_probe(dev, -EINVAL, "No dsi-config for %s node\n", np->name);

	dsi2->grf_regmap = syscon_regmap_lookup_by_phandle(dev->of_node, "rockchip,grf");
	if (IS_ERR(dsi2->grf_regmap))
		return dev_err_probe(dsi2->dev, PTR_ERR(dsi2->grf_regmap), "Unable to get grf\n");

	dsi2->phy = phy_optional_get(dev, "dcphy");
	if (IS_ERR(dsi2->phy))
		return dev_err_probe(dev, PTR_ERR(dsi2->phy), "failed to get mipi phy\n");

	dsi2->dev = dev;
	dsi2->pdata.regmap = dsi2->regmap;
	dsi2->pdata.max_data_lanes = 4;
	dsi2->pdata.phy_ops = &dw_mipi_dsi2_rockchip_phy_ops;
	dsi2->pdata.host_ops = &dw_mipi_dsi2_rockchip_host_ops;
	dsi2->pdata.priv_data = dsi2;
	dev_set_drvdata(dev, dsi2);

	dsi2->dmd = dw_mipi_dsi2_probe(dev, &dsi2->pdata);
	if (IS_ERR(dsi2->dmd))
		return dev_err_probe(dev, PTR_ERR(dsi2->dmd), "Failed to probe dw_mipi_dsi2\n");

	return 0;
}

static const struct dsigrf_reg rk3588_dsi0_grf_reg_fields[MAX_FIELDS] = {
	[TXREQCLKHS_EN]		= { 0x0000, 11, 11 },
	[GATING_EN]		= { 0x0000, 10, 10 },
	[IPI_SHUTDN]		= { 0x0000,  9,  9 },
	[IPI_COLORM]		= { 0x0000,  8,  8 },
	[IPI_COLOR_DEPTH]	= { 0x0000,  4,  7 },
	[IPI_FORMAT]		= { 0x0000,  0,  3 },
};

static const struct dsigrf_reg rk3588_dsi1_grf_reg_fields[MAX_FIELDS] = {
	[TXREQCLKHS_EN]		= { 0x0004, 11, 11 },
	[GATING_EN]		= { 0x0004, 10, 10 },
	[IPI_SHUTDN]		= { 0x0004,  9,  9 },
	[IPI_COLORM]		= { 0x0004,  8,  8 },
	[IPI_COLOR_DEPTH]	= { 0x0004,  4,  7 },
	[IPI_FORMAT]		= { 0x0004,  0,  3 },
};

static const struct rockchip_dw_dsi2_chip_data rk3588_chip_data[] = {
	{
		.reg = 0xfde20000,
		.grf_regs = rk3588_dsi0_grf_reg_fields,
		.max_bit_rate_per_lane = 4500000ULL,
	},
	{
		.reg = 0xfde30000,
		.grf_regs = rk3588_dsi1_grf_reg_fields,
		.max_bit_rate_per_lane = 4500000ULL,
	}
};

static const struct of_device_id dw_mipi_dsi2_rockchip_dt_ids[] = {
	{
		.compatible = "rockchip,rk3588-mipi-dsi2",
		.data = &rk3588_chip_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, dw_mipi_dsi2_rockchip_dt_ids);

struct driver dw_mipi_dsi2_rockchip_driver = {
	.probe = dw_mipi_dsi2_rockchip_probe,
	.name = "dw-mipi-dsi2",
	.of_match_table = dw_mipi_dsi2_rockchip_dt_ids,
};

device_platform_driver(dw_mipi_dsi2_rockchip_driver);
