// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021-2022 Rockchip Electronics Co., Ltd.
 * Copyright (c) 2024 Collabora Ltd.
 *
 * Author: Algea Cao <algea.cao@rock-chips.com>
 * Author: Cristian Ciocaltea <cristian.ciocaltea@collabora.com>
 */
#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/hdmi.h>
#include <i2c/i2c.h>
#include <of.h>
#include <linux/regmap.h>
#include <video/dw_hdmi.h>
#include <video/drm/drm_connector.h>
#include <video/media-bus-format.h>
#include <fb.h>
#include <video/vpl.h>
#include <video/videomode.h>
#include <video/dw_hdmi_qp.h>

#include "dw-hdmi-qp.h"

#define DDC_CI_ADDR		0x37
#define DDC_SEGMENT_ADDR	0x30

#define HDMI14_MAX_TMDSCLK	340000000

#define SCRAMB_POLL_DELAY_MS	3000

struct dw_hdmi_qp;

struct dw_hdmi_qp_i2c {
	struct i2c_adapter	adap;

	u8			stat;

	u8			slave_reg;
	bool			is_regaddr;
	bool			is_segment;

	struct dw_hdmi_qp	*hdmi;
};

struct dw_hdmi_qp {
	struct device *dev;
	struct dw_hdmi_qp_i2c *i2c;

	struct {
		const struct dw_hdmi_qp_phy_ops *ops;
		void *data;
	} phy;

	struct regmap *regm;
	struct vpl vpl;
	struct fb_videomode *mode;
};

static void dw_hdmi_qp_write(struct dw_hdmi_qp *hdmi, unsigned int val,
			     int offset)
{
	regmap_write(hdmi->regm, offset, val);
}

static unsigned int dw_hdmi_qp_read(struct dw_hdmi_qp *hdmi, int offset)
{
	unsigned int val = 0;

	regmap_read(hdmi->regm, offset, &val);

	return val;
}

static void dw_hdmi_qp_mod(struct dw_hdmi_qp *hdmi, unsigned int data,
			   unsigned int mask, unsigned int reg)
{
	regmap_update_bits(hdmi->regm, reg, mask, data);
}

static int dw_hdmi_qp_i2c_wait(struct dw_hdmi_qp *hdmi)
{
	struct dw_hdmi_qp_i2c *i2c = hdmi->i2c;
	uint64_t start = get_time_ns();
	u32 stat;

	while (1) {
		if (is_timeout(start, 100 * MSECOND))
			return -ETIMEDOUT;

		stat = dw_hdmi_qp_read(hdmi, MAINUNIT_1_INT_STATUS);

		i2c->stat = stat & (I2CM_OP_DONE_IRQ | I2CM_READ_REQUEST_IRQ |
				I2CM_NACK_RCVD_IRQ);

		if (i2c->stat) {
			dw_hdmi_qp_write(hdmi, i2c->stat, MAINUNIT_1_INT_CLEAR);
			return 0;
		}
	}
}

static int dw_hdmi_qp_i2c_read(struct dw_hdmi_qp *hdmi,
			       unsigned char *buf, unsigned int length)
{
	struct dw_hdmi_qp_i2c *i2c = hdmi->i2c;
	int ret;

	if (!i2c->is_regaddr) {
		dev_dbg(hdmi->dev, "set read register address to 0\n");
		i2c->slave_reg = 0x00;
		i2c->is_regaddr = true;
	}

	while (length--) {
		dw_hdmi_qp_mod(hdmi, i2c->slave_reg++ << 12, I2CM_ADDR,
			       I2CM_INTERFACE_CONTROL0);

		if (i2c->is_segment)
			dw_hdmi_qp_mod(hdmi, I2CM_EXT_READ, I2CM_WR_MASK,
				       I2CM_INTERFACE_CONTROL0);
		else
			dw_hdmi_qp_mod(hdmi, I2CM_FM_READ, I2CM_WR_MASK,
				       I2CM_INTERFACE_CONTROL0);

		ret = dw_hdmi_qp_i2c_wait(hdmi);
		if (ret) {
			dw_hdmi_qp_write(hdmi, 0x01, I2CM_CONTROL0);
			return ret;
		}

		/* Check for error condition on the bus */
		if (i2c->stat & I2CM_NACK_RCVD_IRQ) {
			dev_err(hdmi->dev, "i2c read error\n");
			dw_hdmi_qp_write(hdmi, 0x01, I2CM_CONTROL0);
			return -EIO;
		}

		*buf++ = dw_hdmi_qp_read(hdmi, I2CM_INTERFACE_RDDATA_0_3) & 0xff;
		dw_hdmi_qp_mod(hdmi, 0, I2CM_WR_MASK, I2CM_INTERFACE_CONTROL0);
	}

	i2c->is_segment = false;

	return 0;
}

static int dw_hdmi_qp_i2c_write(struct dw_hdmi_qp *hdmi,
				unsigned char *buf, unsigned int length)
{
	struct dw_hdmi_qp_i2c *i2c = hdmi->i2c;
	int ret;

	if (!i2c->is_regaddr) {
		/* Use the first write byte as register address */
		i2c->slave_reg = buf[0];
		length--;
		buf++;
		i2c->is_regaddr = true;
	}

	while (length--) {
		dw_hdmi_qp_write(hdmi, *buf++, I2CM_INTERFACE_WRDATA_0_3);
		dw_hdmi_qp_mod(hdmi, i2c->slave_reg++ << 12, I2CM_ADDR,
			       I2CM_INTERFACE_CONTROL0);
		dw_hdmi_qp_mod(hdmi, I2CM_FM_WRITE, I2CM_WR_MASK,
			       I2CM_INTERFACE_CONTROL0);

		ret = dw_hdmi_qp_i2c_wait(hdmi);
		if (ret) {
			dw_hdmi_qp_write(hdmi, 0x01, I2CM_CONTROL0);
			return ret;
		}

		/* Check for error condition on the bus */
		if (i2c->stat & I2CM_NACK_RCVD_IRQ) {
			dev_err(hdmi->dev, "i2c write nack!\n");
			dw_hdmi_qp_write(hdmi, 0x01, I2CM_CONTROL0);
			return -EIO;
		}

		dw_hdmi_qp_mod(hdmi, 0, I2CM_WR_MASK, I2CM_INTERFACE_CONTROL0);
	}

	return 0;
}

#define DDC_ADDR       0x50

static int dw_hdmi_qp_i2c_xfer(struct i2c_adapter *adap,
			       struct i2c_msg *msgs, int num)
{
	struct dw_hdmi_qp_i2c *i2c = container_of(adap, struct dw_hdmi_qp_i2c, adap);
	struct dw_hdmi_qp *hdmi = i2c->hdmi;
	u8 addr = msgs[0].addr;
	int i, ret = 0;

	if (addr == DDC_CI_ADDR)
		/*
		 * The internal I2C controller does not support the multi-byte
		 * read and write operations needed for DDC/CI.
		 * FIXME: Blacklist the DDC/CI address until we filter out
		 * unsupported I2C operations.
		 */
		return -EOPNOTSUPP;

	for (i = 0; i < num; i++) {
		if (msgs[i].len == 0) {
			dev_err(hdmi->dev,
				"unsupported transfer %d/%d, no data\n",
				i + 1, num);
			return -EOPNOTSUPP;
		}
	}

	/* Unmute DONE and ERROR interrupts */
	dw_hdmi_qp_mod(hdmi, I2CM_NACK_RCVD_MASK_N | I2CM_OP_DONE_MASK_N,
		       I2CM_NACK_RCVD_MASK_N | I2CM_OP_DONE_MASK_N,
		       MAINUNIT_1_INT_MASK_N);

	/* Set slave device address taken from the first I2C message */
	if (addr == DDC_SEGMENT_ADDR && msgs[0].len == 1)
		addr = DDC_ADDR;

	dw_hdmi_qp_mod(hdmi, addr << 5, I2CM_SLVADDR, I2CM_INTERFACE_CONTROL0);

	/* Set slave device register address on transfer */
	i2c->is_regaddr = false;

	/* Set segment pointer for I2C extended read mode operation */
	i2c->is_segment = false;

	for (i = 0; i < num; i++) {
		if (msgs[i].addr == DDC_SEGMENT_ADDR && msgs[i].len == 1) {
			i2c->is_segment = true;
			dw_hdmi_qp_mod(hdmi, DDC_SEGMENT_ADDR, I2CM_SEG_ADDR,
				       I2CM_INTERFACE_CONTROL1);
			dw_hdmi_qp_mod(hdmi, *msgs[i].buf << 7, I2CM_SEG_PTR,
				       I2CM_INTERFACE_CONTROL1);
		} else {
			if (msgs[i].flags & I2C_M_RD)
				ret = dw_hdmi_qp_i2c_read(hdmi, msgs[i].buf,
							  msgs[i].len);
			else
				ret = dw_hdmi_qp_i2c_write(hdmi, msgs[i].buf,
							   msgs[i].len);
		}
		if (ret < 0)
			break;
	}

	if (!ret)
		ret = num;

	/* Mute DONE and ERROR interrupts */
	dw_hdmi_qp_mod(hdmi, 0, I2CM_OP_DONE_MASK_N | I2CM_NACK_RCVD_MASK_N,
		       MAINUNIT_1_INT_MASK_N);

	return ret;
}

static struct i2c_adapter *dw_hdmi_qp_i2c_adapter(struct dw_hdmi_qp *hdmi)
{
	struct i2c_adapter *adap;
	struct dw_hdmi_qp_i2c *i2c;
	int ret;

	i2c = xzalloc(sizeof(*i2c));

	adap = &i2c->adap;
	adap->dev.parent = hdmi->dev;
	adap->master_xfer = dw_hdmi_qp_i2c_xfer;
	adap->nr = -1;

	ret = i2c_add_numbered_adapter(adap);
	if (ret) {
		dev_warn(hdmi->dev, "cannot add I2C adapter\n");
		free(i2c);
		return ERR_PTR(ret);
	}

	hdmi->i2c = i2c;
	i2c->hdmi = hdmi;

	dev_info(hdmi->dev, "registered I2C bus driver\n");
	return adap;
}

static void dw_hdmi_qp_enable(struct dw_hdmi_qp *hdmi)
{
	unsigned int op_mode;
	bool is_hdmi = false;

	if (is_hdmi) {
		op_mode = 0;
	} else {
		op_mode = OPMODE_DVI;
	}

	hdmi->phy.ops->init(hdmi, hdmi->phy.data);

	dw_hdmi_qp_mod(hdmi, HDCP2_BYPASS, HDCP2_BYPASS, HDCP2LOGIC_CONFIG0);
	dw_hdmi_qp_mod(hdmi, op_mode, OPMODE_DVI, LINK_CONFIG0);
}

static const struct regmap_config dw_hdmi_qp_regmap_config = {
	.reg_bits	= 32,
	.val_bits	= 32,
	.reg_stride	= 4,
	.max_register	= EARCRX_1_INT_FORCE,
};

static void dw_hdmi_qp_init_hw(struct dw_hdmi_qp *hdmi)
{
	dw_hdmi_qp_write(hdmi, 0, MAINUNIT_0_INT_MASK_N);
	dw_hdmi_qp_write(hdmi, 0, MAINUNIT_1_INT_MASK_N);
	dw_hdmi_qp_write(hdmi, 428571429, TIMER_BASE_CONFIG0);

	/* Software reset */
	dw_hdmi_qp_write(hdmi, 0x01, I2CM_CONTROL0);

	dw_hdmi_qp_write(hdmi, 0x085c085c, I2CM_FM_SCL_CONFIG0);

	dw_hdmi_qp_mod(hdmi, 0, I2CM_FM_EN, I2CM_INTERFACE_CONTROL0);

	/* Clear DONE and ERROR interrupts */
	dw_hdmi_qp_write(hdmi, I2CM_OP_DONE_CLEAR | I2CM_NACK_RCVD_CLEAR,
			 MAINUNIT_1_INT_CLEAR);

	if (hdmi->phy.ops->setup_hpd)
		hdmi->phy.ops->setup_hpd(hdmi, hdmi->phy.data);
}

static const struct drm_display_mode fallback_1920x1080_50hz = {
	.clock = 148500,
	.hdisplay = 1920,
	.hsync_start = 2008,
	.hsync_end = 2052,
	.htotal = 2200,
	.vdisplay = 1080,
	.vsync_start = 1084,
	.vsync_end = 1089,
	.vtotal = 1125,
	.flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
};

static int dw_hdmi_qp_get_modes(struct dw_hdmi_qp *hdmi, struct display_timings *timings)
{
	struct fb_videomode *modes;
	int ret = -ENOENT;
	int i, j = 0;
	unsigned int native_mode = 0;

  dev_err(hdmi->dev, "DEBUG dw_hdmi_qp_get_modes\n");

  timings->edid = edid_read_i2c(&hdmi->i2c->adap);
  if (!timings->edid)
		return -EINVAL;

  ret = edid_to_display_timings(timings, timings->edid);
  if (ret)
    return ret;

	modes = xzalloc(sizeof(*modes) * timings->num_modes);

	for (i = 0; i < timings->num_modes; i++) {
		struct drm_display_mode dmode;
		bool valid;

		fb_videomode_to_drm_display_mode(&timings->modes[i], &dmode);

		valid = true;
//		valid = hdmi->plat_data->mode_valid(hdmi, hdmi->plat_data->priv_data,
//						    NULL, &dmode);

		if (i == timings->native_mode) {
			if (valid)
				native_mode = j;
			else
				dev_err(hdmi->dev, "native mode is invalid\n");
		}

		if (valid) {
			modes[j] = timings->modes[i];
			j++;
		}
	}

	free(timings->modes);
	timings->modes = modes;
	timings->num_modes = j;

	return 0;
}

static int dw_hdmi_qp_ioctl(struct vpl *vpl, unsigned int port,
                         unsigned int cmd, void *data)
{
	struct dw_hdmi_qp *hdmi = container_of(vpl, struct dw_hdmi_qp, vpl);
	struct drm_display_mode mode = {};
//	int ret;

//	if (hdmi->plat_data->vpl_ioctl) {
//		ret = hdmi->plat_data->vpl_ioctl(hdmi, hdmi->plat_data->priv_data, port, cmd, data);
//		if (ret)
//			return ret;
//	}

	switch (cmd) {
	case VPL_ENABLE:
//		hdmi->hdmi_data.enc_in_bus_format = MEDIA_BUS_FMT_FIXED;
//		hdmi->hdmi_data.enc_out_bus_format = MEDIA_BUS_FMT_FIXED;
		fb_videomode_to_drm_display_mode(hdmi->mode, &mode);

		dw_hdmi_qp_enable(hdmi);
		return 0;
	case VPL_DISABLE:
		hdmi->phy.ops->disable(hdmi, hdmi->phy.data);
		return 0;
	case VPL_PREPARE:
		hdmi->mode = data;
		fb_videomode_to_drm_display_mode(hdmi->mode, &mode);
		if (hdmi->phy.ops->mode_set)
			hdmi->phy.ops->mode_set(hdmi, hdmi->phy.data, &mode);
		return 0;
	case VPL_GET_VIDEOMODES:
		return dw_hdmi_qp_get_modes(hdmi, data);
	}

        return 0;
}

struct dw_hdmi_qp *dw_hdmi_qp_bind(struct device *dev,
				   const struct dw_hdmi_qp_plat_data *plat_data)
{
	struct device_node *np = dev->of_node;
	struct dw_hdmi_qp *hdmi;
	struct resource *iores;
	void __iomem *regs;
	int ret;

	if (!plat_data->phy_ops || !plat_data->phy_ops->init ||
	    !plat_data->phy_ops->disable || !plat_data->phy_ops->read_hpd) {
		dev_err(dev, "Missing platform PHY ops\n");
		return ERR_PTR(-ENODEV);
	}

	hdmi = xzalloc(sizeof(*hdmi));

	hdmi->dev = dev;

	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return ERR_CAST(iores);
	regs = IOMEM(iores->start);

	hdmi->regm = regmap_init_mmio(dev, regs, &dw_hdmi_qp_regmap_config);
	if (IS_ERR(hdmi->regm)) {
		dev_err(dev, "Failed to configure regmap\n");
		return ERR_CAST(hdmi->regm);
	}

	hdmi->phy.ops = plat_data->phy_ops;
	hdmi->phy.data = plat_data->phy_data;

	dw_hdmi_qp_init_hw(hdmi);

	dw_hdmi_qp_i2c_adapter(hdmi);

	hdmi->vpl.node = np;
	hdmi->vpl.ioctl = dw_hdmi_qp_ioctl;

	ret = vpl_register(&hdmi->vpl);
	if (ret)
		return ERR_PTR(ret);

	return hdmi;
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_bind);

MODULE_AUTHOR("Algea Cao <algea.cao@rock-chips.com>");
MODULE_AUTHOR("Cristian Ciocaltea <cristian.ciocaltea@collabora.com>");
MODULE_DESCRIPTION("DW HDMI QP transmitter library");
MODULE_LICENSE("GPL");
