// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021-2022 Rockchip Electronics Co., Ltd.
 * Copyright (c) 2024 Collabora Ltd.
 *
 * Author: Algea Cao <algea.cao@rock-chips.com>
 * Author: Cristian Ciocaltea <cristian.ciocaltea@collabora.com>
 */
#include <linux/clk.h>
#include <driver.h>
#include <mfd/syscon.h>
#include <regulator.h>
#include <linux/bits.h>
#include <linux/regmap.h>
#include <video/dw_hdmi.h>
#include <linux/phy/phy.h>
#include <linux/math.h>
#include <video/drm/drm_connector.h>
#include <video/drm/drm_modes.h>
#include <fb.h>
#include <linux/kernel.h>
#include <video/dw_hdmi_qp.h>
#include <linux/gpio/consumer.h>

#include "rockchip_drm_drv.h"

#define RK3588_GRF_SOC_CON2		0x0308
#define RK3588_HDMI0_HPD_INT_MSK	BIT(13)
#define RK3588_HDMI0_HPD_INT_CLR	BIT(12)
#define RK3588_HDMI1_HPD_INT_MSK	BIT(15)
#define RK3588_HDMI1_HPD_INT_CLR	BIT(14)
#define RK3588_GRF_SOC_CON7		0x031c
#define RK3588_SET_HPD_PATH_MASK	GENMASK(13, 12)
#define RK3588_GRF_SOC_STATUS1		0x0384
#define RK3588_HDMI0_LEVEL_INT		BIT(16)
#define RK3588_HDMI1_LEVEL_INT		BIT(24)
#define RK3588_GRF_VO1_CON3		0x000c
#define RK3588_GRF_VO1_CON6		0x0018
#define RK3588_SCLIN_MASK		BIT(9)
#define RK3588_SDAIN_MASK		BIT(10)
#define RK3588_MODE_MASK		BIT(11)
#define RK3588_I2S_SEL_MASK		BIT(13)
#define RK3588_GRF_VO1_CON9		0x0024
#define RK3588_HDMI0_GRANT_SEL		BIT(10)
#define RK3588_HDMI1_GRANT_SEL		BIT(12)

#define HIWORD_UPDATE(val, mask)	((val) | (mask) << 16)
#define HOTPLUG_DEBOUNCE_MS		150

#define MAX_HDMI_PORT_NUM		2

struct rockchip_hdmi_qp {
	struct device *dev;
	struct regmap *regmap;
	struct regmap *vo_regmap;
	struct dw_hdmi_qp *hdmi;
	struct phy *phy;
	struct gpio_desc *enable_gpio;
	int port_id;
};

static int dw_hdmi_qp_rk3588_mode_set(struct dw_hdmi_qp *dw_hdmi, void *data,
				      const struct drm_display_mode *mode)
{
	struct rockchip_hdmi_qp *hdmi = data;

	dev_err(hdmi->dev, "dw_hdmi_qp_rk3588_mode_set mode->clock: %d\n", mode->clock);

	/* Unconditionally switch to TMDS as FRL is not yet supported */
	gpiod_set_value(hdmi->enable_gpio, 1);

	/*
	 * FIXME: Temporary workaround to pass pixel clock rate
	 * to the PHY driver until phy_configure_opts_hdmi
	 * becomes available in the PHY API. See also the related
	 * comment in rk_hdptx_phy_power_on() from
	 * drivers/phy/rockchip/phy-rockchip-samsung-hdptx.c
	 */
	phy_set_bus_width(hdmi->phy, mode->clock * 10);

	return 0;
}

static int dw_hdmi_qp_rk3588_phy_init(struct dw_hdmi_qp *dw_hdmi, void *data)
{
	struct rockchip_hdmi_qp *hdmi = (struct rockchip_hdmi_qp *)data;

	return phy_power_on(hdmi->phy);
}

static void dw_hdmi_qp_rk3588_phy_disable(struct dw_hdmi_qp *dw_hdmi,
					  void *data)
{
	struct rockchip_hdmi_qp *hdmi = (struct rockchip_hdmi_qp *)data;

	phy_power_off(hdmi->phy);
}

static enum drm_connector_status
dw_hdmi_qp_rk3588_read_hpd(struct dw_hdmi_qp *dw_hdmi, void *data)
{
	struct rockchip_hdmi_qp *hdmi = (struct rockchip_hdmi_qp *)data;
	u32 val;

	regmap_read(hdmi->regmap, RK3588_GRF_SOC_STATUS1, &val);
	val &= hdmi->port_id ? RK3588_HDMI1_LEVEL_INT : RK3588_HDMI0_LEVEL_INT;

	return val ? connector_status_connected : connector_status_disconnected;
}

static void dw_hdmi_qp_rk3588_setup_hpd(struct dw_hdmi_qp *dw_hdmi, void *data)
{
	struct rockchip_hdmi_qp *hdmi = (struct rockchip_hdmi_qp *)data;
	u32 val;

	if (hdmi->port_id)
		val = HIWORD_UPDATE(RK3588_HDMI1_HPD_INT_CLR,
				    RK3588_HDMI1_HPD_INT_CLR | RK3588_HDMI1_HPD_INT_MSK);
	else
		val = HIWORD_UPDATE(RK3588_HDMI0_HPD_INT_CLR,
				    RK3588_HDMI0_HPD_INT_CLR | RK3588_HDMI0_HPD_INT_MSK);

	regmap_write(hdmi->regmap, RK3588_GRF_SOC_CON2, val);
}

static const struct dw_hdmi_qp_phy_ops rk3588_hdmi_phy_ops = {
	.mode_set	= dw_hdmi_qp_rk3588_mode_set,
	.init		= dw_hdmi_qp_rk3588_phy_init,
	.disable	= dw_hdmi_qp_rk3588_phy_disable,
	.read_hpd	= dw_hdmi_qp_rk3588_read_hpd,
	.setup_hpd	= dw_hdmi_qp_rk3588_setup_hpd,
};

struct rockchip_hdmi_qp_cfg {
	unsigned int num_ports;
	unsigned int port_ids[MAX_HDMI_PORT_NUM];
	const struct dw_hdmi_qp_phy_ops *phy_ops;
};

static const struct rockchip_hdmi_qp_cfg rk3588_hdmi_cfg = {
	.num_ports = 2,
	.port_ids = {
		0xfde80000,
		0xfdea0000,
	},
	.phy_ops = &rk3588_hdmi_phy_ops,
};

static const struct of_device_id dw_hdmi_qp_rockchip_dt_ids[] = {
	{ .compatible = "rockchip,rk3588-dw-hdmi-qp",
	  .data = &rk3588_hdmi_cfg },
	{},
};
MODULE_DEVICE_TABLE(of, dw_hdmi_qp_rockchip_dt_ids);

static int dw_hdmi_qp_rockchip_probe(struct device *dev)
{
	static const char * const clk_names[] = {
		"pclk", "earc", "aud", "hdp", "hclk_vo1",
		"ref" /* keep "ref" last */
	};
	struct dw_hdmi_qp_plat_data plat_data;
	struct rockchip_hdmi_qp *hdmi;
	struct clk *clk;
	const struct rockchip_hdmi_qp_cfg *cfg;
	struct resource *res;
	int ret, i;
	u32 val;

	if (!dev->of_node)
		return -ENODEV;

	hdmi = xzalloc(sizeof(*hdmi));

	res = dev_get_resource(dev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	cfg = device_get_match_data(dev);
	if (!cfg)
		return dev_err_probe(dev, -EINVAL, "No match data\n");

	plat_data.phy_ops = cfg->phy_ops;
	if (!plat_data.phy_ops)
		return dev_err_probe(dev, -EINVAL, "No phy ops\n");

	plat_data.phy_data = hdmi;
	hdmi->dev = dev;

	hdmi->port_id = -ENODEV;

	/* Identify port ID by matching base IO address */
	for (i = 0; i < cfg->num_ports; i++) {
		if (res->start == cfg->port_ids[i]) {
			hdmi->port_id = i;
			break;
		}
	}
	if (hdmi->port_id < 0) {
		dev_err(hdmi->dev, "Failed to match HDMI port ID\n");
		return hdmi->port_id;
	}

	hdmi->regmap = syscon_regmap_lookup_by_phandle(dev->of_node,
						       "rockchip,grf");
	if (IS_ERR(hdmi->regmap)) {
		dev_err(dev, "Unable to get rockchip,grf\n");
		return PTR_ERR(hdmi->regmap);
	}

	hdmi->vo_regmap = syscon_regmap_lookup_by_phandle(dev->of_node,
							  "rockchip,vo-grf");
	if (IS_ERR(hdmi->vo_regmap)) {
		dev_err(dev, "Unable to get rockchip,vo-grf\n");
		return PTR_ERR(hdmi->vo_regmap);
	}

	for (i = 0; i < ARRAY_SIZE(clk_names); i++) {
		clk = clk_get_enabled(hdmi->dev, clk_names[i]);

		if (IS_ERR(clk)) {
			ret = PTR_ERR(clk);
			if (ret != -EPROBE_DEFER)
				dev_err(dev, "Failed to get %s clock: %d\n",
					clk_names[i], ret);
			return ret;
		}
	}

	hdmi->enable_gpio = gpiod_get_optional(hdmi->dev, "enable",
						    GPIOD_OUT_HIGH);
	if (IS_ERR(hdmi->enable_gpio)) {
		ret = PTR_ERR(hdmi->enable_gpio);
		dev_err(dev, "Failed to request enable GPIO: %d\n", ret);
		return ret;
	}

	hdmi->phy = of_phy_get(dev->of_node, NULL);
	if (IS_ERR(hdmi->phy)) {
		ret = PTR_ERR(hdmi->phy);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get phy: %d\n", ret);
		return ret;
	}

	val = HIWORD_UPDATE(RK3588_SCLIN_MASK, RK3588_SCLIN_MASK) |
	      HIWORD_UPDATE(RK3588_SDAIN_MASK, RK3588_SDAIN_MASK) |
	      HIWORD_UPDATE(RK3588_MODE_MASK, RK3588_MODE_MASK) |
	      HIWORD_UPDATE(RK3588_I2S_SEL_MASK, RK3588_I2S_SEL_MASK);
	regmap_write(hdmi->vo_regmap,
		     hdmi->port_id ? RK3588_GRF_VO1_CON6 : RK3588_GRF_VO1_CON3,
		     val);

	val = HIWORD_UPDATE(RK3588_SET_HPD_PATH_MASK,
			    RK3588_SET_HPD_PATH_MASK);
	regmap_write(hdmi->regmap, RK3588_GRF_SOC_CON7, val);

  if (hdmi->port_id) {
    val = HIWORD_UPDATE(RK3588_HDMI0_GRANT_SEL,
                        RK3588_HDMI0_GRANT_SEL);
    regmap_write(hdmi->vo_regmap, RK3588_GRF_VO1_CON9, val);

    val = HIWORD_UPDATE(RK3588_HDMI0_HPD_INT_MSK, RK3588_HDMI0_HPD_INT_MSK);
    regmap_write(hdmi->regmap, RK3588_GRF_SOC_CON2, val);
  } else {
    val = HIWORD_UPDATE(RK3588_HDMI1_GRANT_SEL,
                        RK3588_HDMI1_GRANT_SEL);
    regmap_write(hdmi->vo_regmap, RK3588_GRF_VO1_CON9, val);

    val = HIWORD_UPDATE(RK3588_HDMI1_HPD_INT_MSK, RK3588_HDMI1_HPD_INT_MSK);
    regmap_write(hdmi->regmap, RK3588_GRF_SOC_CON2, val);
  }
  
	hdmi->hdmi = dw_hdmi_qp_bind(dev, &plat_data);
	if (IS_ERR(hdmi->hdmi)) {
		ret = PTR_ERR(hdmi->hdmi);
		return ret;
	}

	return 0;
}

struct driver dw_hdmi_qp_rockchip_driver = {
	.probe  = dw_hdmi_qp_rockchip_probe,
	.name = "dwhdmiqp-rockchip",
	.of_compatible = dw_hdmi_qp_rockchip_dt_ids,
};
device_platform_driver(dw_hdmi_qp_rockchip_driver);
