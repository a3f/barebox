// SPDX-License-Identifier: GPL-2.0-only
/*
 * Rockchip eFuse Driver
 *
 * Copyright (c) 2015 Rockchip Electronics Co. Ltd.
 * Author: Caesar Wang <wxt@rock-chips.com>
 */

#include <common.h>
#include <linux/clk.h>
#include <linux/overflow.h>
#include <clock.h>
#include <driver.h>
#include <io.h>
#include <linux/nvmem-provider.h>
#include <soc/rockchip/cpuid.h>
#include <of.h>
#include <init.h>

#define RK3288_A_SHIFT		6
#define RK3288_A_MASK		0x3ff
#define RK3288_PGENB		BIT(3)
#define RK3288_LOAD		BIT(2)
#define RK3288_STROBE		BIT(1)
#define RK3288_CSB		BIT(0)

#define RK3328_SECURE_SIZES	96
#define RK3328_INT_STATUS	0x0018
#define RK3328_DOUT		0x0020
#define RK3328_AUTO_CTRL	0x0024
#define RK3328_INT_FINISH	BIT(0)
#define RK3328_AUTO_ENB		BIT(0)
#define RK3328_AUTO_RD		BIT(1)

#define RK3399_A_SHIFT		16
#define RK3399_A_MASK		0x3ff
#define RK3399_NBYTES		4
#define RK3399_STROBSFTSEL	BIT(9)
#define RK3399_RSB		BIT(7)
#define RK3399_PD		BIT(5)
#define RK3399_PGENB		BIT(3)
#define RK3399_LOAD		BIT(2)
#define RK3399_STROBE		BIT(1)
#define RK3399_CSB		BIT(0)

#define REG_EFUSE_CTRL		0x0000
#define REG_EFUSE_DOUT		0x0004

struct rockchip_efuse_chip {
	struct device *dev;
	void __iomem *base;
	struct clk *clk;
};

static int rockchip_rk3288_efuse_read(void *context, unsigned int offset,
				      void *val, size_t bytes)
{
	struct rockchip_efuse_chip *efuse = context;
	u8 *buf = val;
	int ret;

	ret = clk_enable(efuse->clk);
	if (ret < 0) {
		dev_err(efuse->dev, "failed to prepare/enable efuse clk\n");
		return ret;
	}

	writel(RK3288_LOAD | RK3288_PGENB, efuse->base + REG_EFUSE_CTRL);
	udelay(1);
	while (bytes--) {
		writel(readl(efuse->base + REG_EFUSE_CTRL) &
			     (~(RK3288_A_MASK << RK3288_A_SHIFT)),
			     efuse->base + REG_EFUSE_CTRL);
		writel(readl(efuse->base + REG_EFUSE_CTRL) |
			     ((offset++ & RK3288_A_MASK) << RK3288_A_SHIFT),
			     efuse->base + REG_EFUSE_CTRL);
		udelay(1);
		writel(readl(efuse->base + REG_EFUSE_CTRL) |
			     RK3288_STROBE, efuse->base + REG_EFUSE_CTRL);
		udelay(1);
		*buf++ = readb(efuse->base + REG_EFUSE_DOUT);
		writel(readl(efuse->base + REG_EFUSE_CTRL) &
		       (~RK3288_STROBE), efuse->base + REG_EFUSE_CTRL);
		udelay(1);
	}

	/* Switch to standby mode */
	writel(RK3288_PGENB | RK3288_CSB, efuse->base + REG_EFUSE_CTRL);

	clk_disable(efuse->clk);

	return 0;
}

static int rockchip_rk3328_efuse_read(void *context, unsigned int offset,
				      void *val, size_t bytes)
{
	struct rockchip_efuse_chip *efuse = context;
	unsigned int addr_start, addr_end, addr_offset, addr_len;
	u32 out_value, status;
	u8 *buf;
	int ret, i = 0;

	ret = clk_enable(efuse->clk);
	if (ret < 0) {
		dev_err(efuse->dev, "failed to prepare/enable efuse clk\n");
		return ret;
	}

	/* 128 Byte efuse, 96 Byte for secure, 32 Byte for non-secure */
	offset += RK3328_SECURE_SIZES;
	addr_start = rounddown(offset, RK3399_NBYTES) / RK3399_NBYTES;
	addr_end = roundup(offset + bytes, RK3399_NBYTES) / RK3399_NBYTES;
	addr_offset = offset % RK3399_NBYTES;
	addr_len = addr_end - addr_start;

	buf = kzalloc(array3_size(addr_len, RK3399_NBYTES, sizeof(*buf)),
		      GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto nomem;
	}

	while (addr_len--) {
		writel(RK3328_AUTO_RD | RK3328_AUTO_ENB |
		       ((addr_start++ & RK3399_A_MASK) << RK3399_A_SHIFT),
		       efuse->base + RK3328_AUTO_CTRL);
		udelay(4);
		status = readl(efuse->base + RK3328_INT_STATUS);
		if (!(status & RK3328_INT_FINISH)) {
			ret = -EIO;
			goto err;
		}
		out_value = readl(efuse->base + RK3328_DOUT);
		writel(RK3328_INT_FINISH, efuse->base + RK3328_INT_STATUS);

		memcpy(&buf[i], &out_value, RK3399_NBYTES);
		i += RK3399_NBYTES;
	}

	memcpy(val, buf + addr_offset, bytes);
err:
	kfree(buf);
nomem:
	clk_disable(efuse->clk);

	return ret;
}

static int rockchip_rk3399_efuse_read(void *context, unsigned int offset,
				      void *val, size_t bytes)
{
	struct rockchip_efuse_chip *efuse = context;
	unsigned int addr_start, addr_end, addr_offset, addr_len;
	u32 out_value;
	u8 *buf;
	int ret, i = 0;

	ret = clk_enable(efuse->clk);
	if (ret < 0) {
		dev_err(efuse->dev, "failed to prepare/enable efuse clk\n");
		return ret;
	}

	addr_start = rounddown(offset, RK3399_NBYTES) / RK3399_NBYTES;
	addr_end = roundup(offset + bytes, RK3399_NBYTES) / RK3399_NBYTES;
	addr_offset = offset % RK3399_NBYTES;
	addr_len = addr_end - addr_start;

	buf = kzalloc(array3_size(addr_len, RK3399_NBYTES, sizeof(*buf)),
		      GFP_KERNEL);
	if (!buf) {
		clk_disable(efuse->clk);
		return -ENOMEM;
	}

	writel(RK3399_LOAD | RK3399_PGENB | RK3399_STROBSFTSEL | RK3399_RSB,
	       efuse->base + REG_EFUSE_CTRL);
	udelay(1);
	while (addr_len--) {
		writel(readl(efuse->base + REG_EFUSE_CTRL) | RK3399_STROBE |
		       ((addr_start++ & RK3399_A_MASK) << RK3399_A_SHIFT),
		       efuse->base + REG_EFUSE_CTRL);
		udelay(1);
		out_value = readl(efuse->base + REG_EFUSE_DOUT);
		writel(readl(efuse->base + REG_EFUSE_CTRL) & (~RK3399_STROBE),
		       efuse->base + REG_EFUSE_CTRL);
		udelay(1);

		memcpy(&buf[i], &out_value, RK3399_NBYTES);
		i += RK3399_NBYTES;
	}

	/* Switch to standby mode */
	writel(RK3399_PD | RK3399_CSB, efuse->base + REG_EFUSE_CTRL);

	memcpy(val, buf + addr_offset, bytes);

	kfree(buf);

	clk_disable(efuse->clk);

	return 0;
}

static struct nvmem_config econfig = {
	.name = "rockchip_efuse",
	.stride = 1,
	.word_size = 1,
	.read_only = true,
};

static const struct of_device_id rockchip_efuse_match[] = {
	/* deprecated but kept around for dts binding compatibility */
	{
		.compatible = "rockchip,rockchip-efuse",
		.data = (void *)&rockchip_rk3288_efuse_read,
	},
	{
		.compatible = "rockchip,rk3066a-efuse",
		.data = (void *)&rockchip_rk3288_efuse_read,
	},
	{
		.compatible = "rockchip,rk3188-efuse",
		.data = (void *)&rockchip_rk3288_efuse_read,
	},
	{
		.compatible = "rockchip,rk3228-efuse",
		.data = (void *)&rockchip_rk3288_efuse_read,
	},
	{
		.compatible = "rockchip,rk3288-efuse",
		.data = (void *)&rockchip_rk3288_efuse_read,
	},
	{
		.compatible = "rockchip,rk3368-efuse",
		.data = (void *)&rockchip_rk3288_efuse_read,
	},
	{
		.compatible = "rockchip,rk3328-efuse",
		.data = (void *)&rockchip_rk3328_efuse_read,
	},
	{
		.compatible = "rockchip,rk3399-efuse",
		.data = (void *)&rockchip_rk3399_efuse_read,
	},
	{ /* sentinel */},
};

static int rockchip_efuse_probe(struct device *dev)
{
	struct resource *res;
	struct nvmem_device *nvmem;
	struct rockchip_efuse_chip *efuse;
	const void *data;
	u8 cpuid[16];

	data = device_get_match_data(dev);
	if (!data) {
		dev_err(dev, "failed to get match data\n");
		return -EINVAL;
	}

	efuse = kzalloc(sizeof(struct rockchip_efuse_chip), GFP_KERNEL);
	if (!efuse)
		return -ENOMEM;

	res = dev_request_mem_resource(dev, 0);
	if (IS_ERR(res))
		return PTR_ERR(res);

	efuse->base = IOMEM(res->start);

	efuse->clk = clk_get(dev, "pclk_efuse");
	if (IS_ERR(efuse->clk))
		return PTR_ERR(efuse->clk);

	efuse->dev = dev;

	/*
	 * While unusual, the binding defines the the size of
	 * provided nvmem space as the second word of reg in
	 * case the rockchip,efuse-size property is missing
	 */
	if (of_property_read_u32(dev->of_node, "rockchip,efuse-size",
				 &econfig.size))
		econfig.size = resource_size(res);
	econfig.reg_read = data;
	econfig.priv = efuse;
	econfig.dev = efuse->dev;
	nvmem = nvmem_register(&econfig);
	if (IS_ERR(nvmem))
		return PTR_ERR(nvmem);

	econfig.reg_read(efuse, 0x07, cpuid, sizeof(cpuid));

	rockchip_cpuid_set(nvmem, cpuid);

	return 0;
}

static struct driver rockchip_efuse_driver = {
	.name = "rockchip_efuse",
	.of_compatible = rockchip_efuse_match,
	.probe = rockchip_efuse_probe,
};
device_platform_driver(rockchip_efuse_driver);
MODULE_DESCRIPTION("rockchip_efuse driver");
MODULE_LICENSE("GPL v2");
