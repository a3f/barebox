// SPDX-License-Identifier: GPL-2.0-only
/*
 * Rockchip OTP Driver
 *
 * Copyright (c) 2018 Rockchip Electronics Co. Ltd.
 * Author: Finley Xiao <finley.xiao@rock-chips.com>
 */

#include <common.h>
#include <linux/clk.h>
#include <linux/overflow.h>
#include <clock.h>
#include <driver.h>
#include <linux/reset.h>
#include <io.h>
#include <linux/nvmem-provider.h>
#include <linux/iopoll.h>
#include <soc/rockchip/cpuid.h>
#include <of.h>
#include <of_device.h>
#include <init.h>

/* OTP Register Offsets */
#define OTPC_SBPI_CTRL			0x0020
#define OTPC_SBPI_CMD_VALID_PRE		0x0024
#define OTPC_SBPI_CS_VALID_PRE		0x0028
#define OTPC_SBPI_STATUS		0x002C
#define OTPC_USER_CTRL			0x0100
#define OTPC_USER_ADDR			0x0104
#define OTPC_USER_ENABLE		0x0108
#define OTPC_USER_Q			0x0124
#define OTPC_INT_STATUS			0x0304
#define OTPC_SBPI_CMD0_OFFSET		0x1000
#define OTPC_SBPI_CMD1_OFFSET		0x1004

/* OTP Register bits and masks */
#define OTPC_USER_ADDR_MASK		GENMASK(31, 16)
#define OTPC_USE_USER			BIT(0)
#define OTPC_USE_USER_MASK		GENMASK(16, 16)
#define OTPC_USER_FSM_ENABLE		BIT(0)
#define OTPC_USER_FSM_ENABLE_MASK	GENMASK(16, 16)
#define OTPC_SBPI_DONE			BIT(1)
#define OTPC_USER_DONE			BIT(2)

#define SBPI_DAP_ADDR			0x02
#define SBPI_DAP_ADDR_SHIFT		8
#define SBPI_DAP_ADDR_MASK		GENMASK(31, 24)
#define SBPI_CMD_VALID_MASK		GENMASK(31, 16)
#define SBPI_DAP_CMD_WRF		0xC0
#define SBPI_DAP_REG_ECC		0x3A
#define SBPI_ECC_ENABLE			0x00
#define SBPI_ECC_DISABLE		0x09
#define SBPI_ENABLE			BIT(0)
#define SBPI_ENABLE_MASK		GENMASK(16, 16)

#define OTPC_TIMEOUT			10000

struct rockchip_otp {
	struct device *dev;
	void __iomem *base;
	struct clk_bulk_data	*clks;
	int num_clks;
	struct reset_control *rst;
};

/* list of required clocks */
static const char * const rockchip_otp_clocks[] = {
	"otp", "apb_pclk", "phy",
};

struct rockchip_data {
	int size;
};

static int rockchip_otp_reset(struct rockchip_otp *otp)
{
	int ret;

	ret = reset_control_assert(otp->rst);
	if (ret) {
		dev_err(otp->dev, "failed to assert otp phy %d\n", ret);
		return ret;
	}

	udelay(2);

	ret = reset_control_deassert(otp->rst);
	if (ret) {
		dev_err(otp->dev, "failed to deassert otp phy %d\n", ret);
		return ret;
	}

	return 0;
}

static int rockchip_otp_wait_status(struct rockchip_otp *otp, u32 flag)
{
	u32 status = 0;
	int ret;

	ret = readl_poll_timeout(otp->base + OTPC_INT_STATUS, status,
				 (status & flag), OTPC_TIMEOUT);
	if (ret)
		return ret;

	/* clean int status */
	writel(flag, otp->base + OTPC_INT_STATUS);

	return 0;
}

static int rockchip_otp_ecc_enable(struct rockchip_otp *otp, bool enable)
{
	int ret = 0;

	writel(SBPI_DAP_ADDR_MASK | (SBPI_DAP_ADDR << SBPI_DAP_ADDR_SHIFT),
	       otp->base + OTPC_SBPI_CTRL);

	writel(SBPI_CMD_VALID_MASK | 0x1, otp->base + OTPC_SBPI_CMD_VALID_PRE);
	writel(SBPI_DAP_CMD_WRF | SBPI_DAP_REG_ECC,
	       otp->base + OTPC_SBPI_CMD0_OFFSET);
	if (enable)
		writel(SBPI_ECC_ENABLE, otp->base + OTPC_SBPI_CMD1_OFFSET);
	else
		writel(SBPI_ECC_DISABLE, otp->base + OTPC_SBPI_CMD1_OFFSET);

	writel(SBPI_ENABLE_MASK | SBPI_ENABLE, otp->base + OTPC_SBPI_CTRL);

	ret = rockchip_otp_wait_status(otp, OTPC_SBPI_DONE);
	if (ret < 0)
		dev_err(otp->dev, "timeout during ecc_enable\n");

	return ret;
}

static int rockchip_otp_read(void *context, unsigned int offset,
			     void *val, size_t bytes)
{
	struct rockchip_otp *otp = context;
	u8 *buf = val;
	int ret = 0;

	ret = clk_bulk_enable(otp->num_clks, otp->clks);
	if (ret < 0) {
		dev_err(otp->dev, "failed to prepare/enable clks\n");
		return ret;
	}

	ret = rockchip_otp_reset(otp);
	if (ret) {
		dev_err(otp->dev, "failed to reset otp phy\n");
		goto disable_clks;
	}

	ret = rockchip_otp_ecc_enable(otp, false);
	if (ret < 0) {
		dev_err(otp->dev, "rockchip_otp_ecc_enable err\n");
		goto disable_clks;
	}

	writel(OTPC_USE_USER | OTPC_USE_USER_MASK, otp->base + OTPC_USER_CTRL);
	udelay(5);
	while (bytes--) {
		writel(offset++ | OTPC_USER_ADDR_MASK,
		       otp->base + OTPC_USER_ADDR);
		writel(OTPC_USER_FSM_ENABLE | OTPC_USER_FSM_ENABLE_MASK,
		       otp->base + OTPC_USER_ENABLE);
		ret = rockchip_otp_wait_status(otp, OTPC_USER_DONE);
		if (ret < 0) {
			dev_err(otp->dev, "timeout during read setup\n");
			goto read_end;
		}
		*buf++ = readb(otp->base + OTPC_USER_Q);
	}

read_end:
	writel(0x0 | OTPC_USE_USER_MASK, otp->base + OTPC_USER_CTRL);
disable_clks:
	clk_bulk_disable(otp->num_clks, otp->clks);

	return ret;
}

static struct nvmem_config otp_config = {
	.name = "rockchip_otp",
	.read_only = true,
	.stride = 1,
	.word_size = 1,
	.reg_read = rockchip_otp_read,
};

static const struct rockchip_data px30_data = {
	.size = 0x40,
};

static const struct of_device_id rockchip_otp_match[] = {
	{
		.compatible = "rockchip,px30-otp",
		.data = (void *)&px30_data,
	},
	{
		.compatible = "rockchip,rk3308-otp",
		.data = (void *)&px30_data,
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, rockchip_otp_match);

static int rockchip_otp_probe(struct device *dev)
{
	struct rockchip_otp *otp;
	const struct rockchip_data *data;
	struct nvmem_device *nvmem;
	struct resource *res;
	u8 cpuid[16];
	int ret, i;

	data = of_device_get_match_data(dev);
	if (!data) {
		dev_err(dev, "failed to get match data\n");
		return -EINVAL;
	}

	otp = xzalloc(sizeof(*data));

	otp->dev = dev;
	res = dev_request_mem_resource(dev, 0);
	if (IS_ERR(res))
		return PTR_ERR(res);

	otp->base = IOMEM(res->start);

	otp->num_clks = ARRAY_SIZE(rockchip_otp_clocks);
	otp->clks = calloc(otp->num_clks, sizeof(*otp->clks));
	if (!otp->clks)
		return -ENOMEM;

	for (i = 0; i < otp->num_clks; ++i)
		otp->clks[i].id = rockchip_otp_clocks[i];

	ret = clk_bulk_get(dev, otp->num_clks, otp->clks);
	if (ret)
		return ret;

	otp->rst = reset_control_get(dev, "phy");
	if (IS_ERR(otp->rst))
		return PTR_ERR(otp->rst);

	otp_config.priv = otp;
	otp_config.dev = otp->dev;
	nvmem = nvmem_register(&otp_config);
	if (IS_ERR(nvmem))
		return PTR_ERR(nvmem);

	otp_config.reg_read(otp, 0x07, cpuid, sizeof(cpuid));

	rockchip_cpuid_set(nvmem, cpuid);

	return 0;
}

static struct driver rockchip_otp_driver = {
	.probe = rockchip_otp_probe,
	.name = "rockchip_otp",
	.of_compatible = rockchip_otp_match,
};

device_platform_driver(rockchip_otp_driver);
MODULE_DESCRIPTION("Rockchip OTP driver");
MODULE_LICENSE("GPL v2");