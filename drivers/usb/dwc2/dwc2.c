// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2012 Oleksandr Tymoshenko <gonzo@freebsd.org>
 * Copyright (C) 2014 Marek Vasut <marex@denx.de>
 *
 * Copied from u-Boot
 */
#include <common.h>
#include <of.h>
#include <dma.h>
#include <init.h>
#include <errno.h>
#include <driver.h>
#include <linux/clk.h>
#include <linux/phy/phy.h>
#include <linux/reset.h>

#include "dwc2.h"

static void dwc2_uninit_common(struct dwc2 *dwc2)
{
	uint32_t hprt0;

	hprt0 = dwc2_readl(dwc2, HPRT0);

	/* Put everything in reset. */
	hprt0 &= ~(HPRT0_ENA | HPRT0_ENACHG | HPRT0_CONNDET | HPRT0_OVRCURRCHG);
	hprt0 |= HPRT0_RST;

	dwc2_writel(dwc2, hprt0, HPRT0);
}

static int dwc2_set_mode(void *ctx, enum usb_dr_mode mode)
{
	struct dwc2 *dwc2 = ctx;
	int ret = -ENODEV;

	if (mode == USB_DR_MODE_HOST || mode == USB_DR_MODE_OTG) {
		if (IS_ENABLED(CONFIG_USB_DWC2_HOST))
			ret = dwc2_register_host(dwc2);
		else
			dwc2_err(dwc2, "Host support not available\n");
	}
	if (mode == USB_DR_MODE_PERIPHERAL || mode == USB_DR_MODE_OTG) {
		if (IS_ENABLED(CONFIG_USB_DWC2_GADGET))
			ret = dwc2_gadget_init(dwc2);
		else
			dwc2_err(dwc2, "Peripheral support not available\n");
	}

	return ret;
}

static int dwc2_probe(struct device_d *dev)
{
	struct resource *iores;
	struct dwc2 *dwc2;
	struct reset_control *rsts;
	struct phy *phy;
	struct clk *clk;
	int ret;

	dwc2 = xzalloc(sizeof(struct dwc2));
	dev->priv = dwc2;
	dwc2->dev = dev;

	rsts = of_reset_control_array_get_optional(dev->device_node);
	if (IS_ERR(rsts))
		return PTR_ERR(rsts);

	reset_control_deassert(rsts);

	reset_control_put(rsts);

	phy = phy_get_by_index(dev, 0);
	if (IS_ERR(phy) && PTR_ERR(phy) != -ENODEV) {
		return PTR_ERR(phy);
	} else {
		ret = phy_init(phy);
		if (ret)
			return ret;

		phy_power_on(phy);
	}

	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return PTR_ERR(iores);
	dwc2->regs = IOMEM(iores->start);

	clk = of_clk_get(dwc2->dev->device_node, 0);
	if (!IS_ERR(clk))
		clk_enable(clk);

	ret = dwc2_core_snpsid(dwc2);
	if (ret)
		goto error;

	/*
	 * Reset before dwc2_get_hwparams() then it could get power-on real
	 * reset value form registers.
	 */
	ret = dwc2_core_reset(dwc2);
	if (ret)
		goto error;

	/* Detect config values from hardware */
	dwc2_get_hwparams(dwc2);

	ret = dwc2_get_dr_mode(dwc2);
	if (ret)
		return ret;

	dwc2_set_default_params(dwc2);

	dma_set_mask(dev, DMA_BIT_MASK(32));

	pr_info("MODE = %u\n", dwc2->dr_mode);

	if (dwc2->dr_mode == USB_DR_MODE_OTG)
		ret = usb_register_otg_device(dwc2->dev, dwc2_set_mode, dwc2);
	else
		ret = dwc2_set_mode(dwc2, dwc2->dr_mode);

error:
	return ret;
}

static void dwc2_remove(struct device_d *dev)
{
	struct dwc2 *dwc2 = dev->priv;

	dwc2_uninit_common(dwc2);
}

static void dwc2_set_stm32mp15_hsotg_params(struct dwc2 *dwc2)
{
	struct dwc2_core_params *p = &dwc2->params;

	p->otg_cap = DWC2_CAP_PARAM_NO_HNP_SRP_CAPABLE;
	//p->activate_stm_id_vb_detection = true;
	p->host_rx_fifo_size = 440;
	p->host_nperio_tx_fifo_size = 256;
	p->host_perio_tx_fifo_size = 256;
	p->power_down = DWC2_POWER_DOWN_PARAM_NONE;
}

static const struct of_device_id dwc2_platform_dt_ids[] = {
	{ .compatible = "brcm,bcm2835-usb", },
	{ .compatible = "brcm,bcm2708-usb", },
	{ .compatible = "snps,dwc2", },
	{ .compatible = "st,stm32mp15-hsotg",
	  .data = dwc2_set_stm32mp15_hsotg_params },
	{ }
};

static struct driver_d dwc2_driver = {
	.name	= "dwc2",
	.probe	= dwc2_probe,
	.remove = dwc2_remove,
	.of_compatible = DRV_OF_COMPAT(dwc2_platform_dt_ids),
};
device_platform_driver(dwc2_driver);
