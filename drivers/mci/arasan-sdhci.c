// SPDX-License-Identifier: GPL-2.0-or-later

#include <clock.h>
#include <common.h>
#include <driver.h>
#include <init.h>
#include <linux/clk.h>
#include <linux/log2.h>
#include <linux/phy/phy.h>
#include <mci.h>

#include "sdhci.h"

#define SDHCI_ARASAN_HCAP_CLK_FREQ_MASK		0xFF00
#define SDHCI_ARASAN_HCAP_CLK_FREQ_SHIFT	8
#define SDHCI_INT_ADMAE				BIT(29)
#define SDHCI_ARASAN_INT_DATA_MASK		(SDHCI_INT_XFER_COMPLETE | \
						SDHCI_INT_DMA | \
						SDHCI_INT_SPACE_AVAIL | \
						SDHCI_INT_DATA_AVAIL | \
						SDHCI_INT_DATA_TIMEOUT | \
						SDHCI_INT_DATA_CRC | \
						SDHCI_INT_DATA_END_BIT | \
						SDHCI_INT_ADMAE)

#define SDHCI_ARASAN_INT_CMD_MASK		(SDHCI_INT_CMD_COMPLETE | \
						SDHCI_INT_TIMEOUT | \
						SDHCI_INT_CRC | \
						SDHCI_INT_END_BIT | \
						SDHCI_INT_INDEX)

#define SDHCI_ARASAN_BUS_WIDTH			4
#define TIMEOUT_VAL				0xE

#define PHY_CLK_TOO_SLOW_HZ		400000

/**
 * struct sdhci_arasan_clk_data - Arasan Controller Clock Data.
 *
 * @sdcardclk_hw:	Struct for the clock we might provide to a PHY.
 * @sdcardclk:		Pointer to normal 'struct clock' for sdcardclk_hw.
 */
struct sdhci_arasan_clk_data {
	struct clk_hw	sdcardclk_hw;
	struct clk      *sdcardclk;
};

struct arasan_sdhci_host {
	struct mci_host		mci;
	struct sdhci		sdhci;
	struct phy		*phy;
	bool			is_phy_on;
	struct clk		*clk;	/* clk_xin */
	unsigned int		quirks; /* Arasan deviations from spec */
	struct sdhci_arasan_clk_data clk_data;
/* Controller does not have CD wired and will not function normally without */
#define SDHCI_ARASAN_QUIRK_FORCE_CDTEST		BIT(0)
#define SDHCI_ARASAN_QUIRK_NO_1_8_V		BIT(1)
};

static inline
struct arasan_sdhci_host *to_arasan_sdhci_host(struct mci_host *mci)
{
	return container_of(mci, struct arasan_sdhci_host, mci);
}

static inline
struct arasan_sdhci_host *sdhci_to_arasan(struct sdhci *sdhci)
{
	return container_of(sdhci, struct arasan_sdhci_host, sdhci);
}

static int arasan_sdhci_card_present(struct mci_host *mci)
{
	struct arasan_sdhci_host *host = to_arasan_sdhci_host(mci);
	u32 val;

	val = sdhci_read32(&host->sdhci, SDHCI_PRESENT_STATE);

	return !!(val & SDHCI_CARD_DETECT);
}

static int arasan_sdhci_card_write_protected(struct mci_host *mci)
{
	struct arasan_sdhci_host *host = to_arasan_sdhci_host(mci);
	u32 val;

	val = sdhci_read32(&host->sdhci, SDHCI_PRESENT_STATE);

	return !(val & SDHCI_WRITE_PROTECT);
}

static int arasan_sdhci_reset(struct arasan_sdhci_host *host, u8 mask)
{
	int ret;

	ret = sdhci_reset(&host->sdhci, mask);
	if (ret)
		return ret;

	if (host->quirks & SDHCI_ARASAN_QUIRK_FORCE_CDTEST) {
		u8 ctrl;

		ctrl = sdhci_read8(&host->sdhci, SDHCI_HOST_CONTROL);
		ctrl |= SDHCI_CTRL_CDTEST_INS | SDHCI_CTRL_CDTEST_INS;
		sdhci_write8(&host->sdhci, ctrl, SDHCI_HOST_CONTROL);
	}

	return 0;
}

static void arasan_sdhci_set_power(struct sdhci *sdhci, unsigned vdd)
{
	u8 pwr = 0;

	switch (1 << vdd) {
	case MMC_VDD_165_195:
		pwr = SDHCI_POWER_180;
		break;
	case MMC_VDD_29_30:
	case MMC_VDD_30_31:
		pwr = SDHCI_POWER_300;
		break;
	case MMC_VDD_32_33:
	case MMC_VDD_33_34:
		pwr = SDHCI_POWER_330;
		break;
	}

	if (pwr == 0) {
		sdhci_write8(sdhci, SDHCI_POWER_CONTROL, 0);
		return;
	}

	pwr |= SDHCI_POWER_ON;

	sdhci_write8(sdhci, SDHCI_POWER_CONTROL, pwr);
}

static int arasan_sdhci_init(struct mci_host *mci, struct device *dev)
{
	u32 voltages = mci->voltages;
	struct arasan_sdhci_host *host = to_arasan_sdhci_host(mci);
	int ret;

	ret = arasan_sdhci_reset(host, SDHCI_RESET_ALL);
	if (ret)
		return ret;

	if (host->quirks & SDHCI_ARASAN_QUIRK_NO_1_8_V)
		voltages &= ~MMC_VDD_165_195;

	/* If multiple voltages are supported, we just take the highest */
	arasan_sdhci_set_power(&host->sdhci, ilog2(voltages));
	udelay(400);

	sdhci_write32(&host->sdhci, SDHCI_INT_ENABLE,
		      SDHCI_ARASAN_INT_DATA_MASK | SDHCI_ARASAN_INT_CMD_MASK);
	sdhci_write32(&host->sdhci, SDHCI_SIGNAL_ENABLE, 0);

	return 0;
}

static void sdhci_arasan_set_clock(struct arasan_sdhci_host *host, unsigned int clock)
{
	struct device *dev = host->mci.hw_dev;
	int max_clk = clk_get_rate(host->clk);
	bool ctrl_phy = false;

	if (!IS_ERR(host->phy)) {
		if (!host->is_phy_on && clock <= PHY_CLK_TOO_SLOW_HZ) {
			/*
			 * If PHY off, set clock to max speed and power PHY on.
			 *
			 * Although PHY docs apparently suggest power cycling
			 * when changing the clock the PHY doesn't like to be
			 * powered on while at low speeds like those used in ID
			 * mode.  Even worse is powering the PHY on while the
			 * clock is off.
			 *
			 * To workaround the PHY limitations, the best we can
			 * do is to power it on at a faster speed and then slam
			 * through low speeds without power cycling.
			 */
			sdhci_set_clock(&host->sdhci, max_clk, max_clk);
			if (phy_power_on(host->phy)) {
				dev_err(dev, "cannot power on phy\n");
				return;
			}

			host->is_phy_on = true;

			/*
			 * We'll now fall through to the below case with
			 * ctrl_phy = false (so we won't turn off/on).  The
			 * sdhci_set_clock() will set the real clock.
			 */
		} else if (clock > PHY_CLK_TOO_SLOW_HZ) {
			/*
			 * At higher clock speeds the PHY is fine being power
			 * cycled and docs say you _should_ power cycle when
			 * changing clock speeds.
			 */
			ctrl_phy = true;
		}
	}

	if (ctrl_phy && host->is_phy_on) {
		phy_power_off(host->phy);
		host->is_phy_on = false;
	}

	BUG_ON(clk_set_rate(host->clk, clock));

	sdhci_set_clock(&host->sdhci, clock, max_clk);

	if (ctrl_phy) {
		if (phy_power_on(host->phy)) {
			dev_err(dev, "cannot power on phy\n");
			return;
		}

		host->is_phy_on = true;
	}
}

static void arasan_sdhci_set_ios(struct mci_host *mci, struct mci_ios *ios)
{
	struct arasan_sdhci_host *host = to_arasan_sdhci_host(mci);
	u16 val;

	/* stop clock */
	sdhci_write16(&host->sdhci, SDHCI_CLOCK_CONTROL, 0);

	if (ios->clock)
		sdhci_arasan_set_clock(host, ios->clock);

	sdhci_set_bus_width(&host->sdhci, ios->bus_width);

	val = sdhci_read8(&host->sdhci, SDHCI_HOST_CONTROL);

	if (ios->clock > 26000000)
		val |= SDHCI_CTRL_HISPD;
	else
		val &= ~SDHCI_CTRL_HISPD;

	sdhci_write8(&host->sdhci, SDHCI_HOST_CONTROL, val);
}

static void print_error(struct arasan_sdhci_host *host, int cmdidx, int ret)
{
	if (ret == -ETIMEDOUT)
		return;

	dev_err(host->mci.hw_dev,
		"error while transferring data for command %d\n", cmdidx);
	dev_err(host->mci.hw_dev, "state = 0x%08x , interrupt = 0x%08x\n",
		sdhci_read32(&host->sdhci, SDHCI_PRESENT_STATE),
		sdhci_read32(&host->sdhci, SDHCI_INT_NORMAL_STATUS));
}

static int arasan_sdhci_send_cmd(struct mci_host *mci, struct mci_cmd *cmd,
				 struct mci_data *data)
{
	struct arasan_sdhci_host *host = to_arasan_sdhci_host(mci);
	u32 mask, command, xfer;
	int ret;

	/* Wait for idle before next command */
	mask = SDHCI_CMD_INHIBIT_CMD;
	if (cmd->cmdidx != MMC_CMD_STOP_TRANSMISSION)
		mask |= SDHCI_CMD_INHIBIT_DATA;

	ret = wait_on_timeout(10 * MSECOND,
			      !(sdhci_read32(&host->sdhci, SDHCI_PRESENT_STATE) & mask));
	if (ret) {
		dev_err(host->mci.hw_dev,
			"SDHCI timeout while waiting for idle\n");
		return ret;
	}

	sdhci_write32(&host->sdhci, SDHCI_INT_STATUS, ~0);

	mask = SDHCI_INT_CMD_COMPLETE;
	if (data && data->flags == MMC_DATA_READ)
		mask |= SDHCI_INT_DATA_AVAIL;
	if (cmd->resp_type & MMC_RSP_BUSY)
		mask |= SDHCI_INT_XFER_COMPLETE;

	sdhci_set_cmd_xfer_mode(&host->sdhci,
				cmd, data, false, &command, &xfer);

	sdhci_write8(&host->sdhci, SDHCI_TIMEOUT_CONTROL, TIMEOUT_VAL);
	if (data) {
		sdhci_write16(&host->sdhci, SDHCI_TRANSFER_MODE, xfer);
		sdhci_write16(&host->sdhci, SDHCI_BLOCK_SIZE,
			      SDHCI_DMA_BOUNDARY_512K | SDHCI_TRANSFER_BLOCK_SIZE(data->blocksize));
		sdhci_write16(&host->sdhci, SDHCI_BLOCK_COUNT, data->blocks);
	}
	sdhci_write32(&host->sdhci, SDHCI_ARGUMENT, cmd->cmdarg);
	sdhci_write16(&host->sdhci, SDHCI_COMMAND, command);

	ret = sdhci_wait_for_done(&host->sdhci, mask);
	if (ret)
		goto error;

	sdhci_read_response(&host->sdhci, cmd);
	sdhci_write32(&host->sdhci, SDHCI_INT_STATUS, mask);

	if (data)
		ret = sdhci_transfer_data_pio(&host->sdhci, data);

	udelay(1000);

error:
	if (ret) {
		print_error(host, cmd->cmdidx, ret);
		arasan_sdhci_reset(host, SDHCI_RESET_CMD);
		arasan_sdhci_reset(host, SDHCI_RESET_DATA);
	}

	sdhci_write32(&host->sdhci, SDHCI_INT_STATUS, ~0);

	return ret;
}

/**
 * sdhci_arasan_sdcardclk_recalc_rate - Return the card clock rate
 *
 * @hw:			Pointer to the hardware clock structure.
 * @parent_rate:		The parent rate (should be rate of clk_xin).
 *
 * Return the current actual rate of the SD card clock.  This can be used
 * to communicate with out PHY.
 *
 * Return: The card clock rate.
 */
static unsigned long sdhci_arasan_sdcardclk_recalc_rate(struct clk_hw *hw,
						      unsigned long parent_rate)
{
	struct sdhci_arasan_clk_data *clk_data =
		container_of(hw, struct sdhci_arasan_clk_data, sdcardclk_hw);
	struct arasan_sdhci_host *host =
		container_of(clk_data, struct arasan_sdhci_host, clk_data);

	return host->sdhci.mci->actual_clock;
}

static const struct clk_ops arasan_sdcardclk_ops = {
	.recalc_rate = sdhci_arasan_sdcardclk_recalc_rate,
};

/**
 * sdhci_arasan_register_sdcardclk - Register the sdcardclk for a PHY to use
 *
 * @host:		Our private data structure.
 * @clk_xin:		Pointer to the functional clock
 * @dev:		Pointer to our struct device.
 *
 * Some PHY devices need to know what the actual card clock is.  In order for
 * them to find out, we'll provide a clock through the common clock framework
 * for them to query.
 *
 * Return: 0 on success and error value on error
 */
static int
sdhci_arasan_register_sdcardclk(struct arasan_sdhci_host *host,
				struct clk *clk_xin,
				struct device *dev)
{
	struct sdhci_arasan_clk_data *clk_data = &host->clk_data;
	struct device_node *np = dev->of_node;
	struct clk_init_data sdcardclk_init;
	const char *parent_clk_name;
	int ret;

	ret = of_property_read_string_index(np, "clock-output-names", 0,
					    &sdcardclk_init.name);
	if (ret) {
		dev_err(dev, "DT has #clock-cells but no clock-output-names\n");
		return ret;
	}

	parent_clk_name = __clk_get_name(clk_xin);
	sdcardclk_init.parent_names = &parent_clk_name;
	sdcardclk_init.num_parents = 1;
	sdcardclk_init.flags = CLK_GET_RATE_NOCACHE;
	sdcardclk_init.ops = &arasan_sdcardclk_ops;

	clk_data->sdcardclk_hw.init = &sdcardclk_init;
	clk_data->sdcardclk = clk_register(dev, &clk_data->sdcardclk_hw);
	if (IS_ERR(clk_data->sdcardclk))
		return PTR_ERR(clk_data->sdcardclk);
	clk_data->sdcardclk_hw.init = NULL;

	ret = of_clk_add_provider(np, of_clk_src_simple_get,
				  clk_data->sdcardclk);
	if (ret)
		dev_err(dev, "Failed to add sdcard clock provider\n");

	return ret;
}

/**
 * sdhci_arasan_register_sdclk - Register the sdcardclk for a PHY to use
 *
 * @host:		Our private data structure.
 * @clk_xin:		Pointer to the functional clock
 * @dev:		Pointer to our struct device.
 *
 * Some PHY devices need to know what the actual card clock is.  In order for
 * them to find out, we'll provide a clock through the common clock framework
 * for them to query.
 *
 * Note: without seriously re-architecting SDHCI's clock code and testing on
 * all platforms, there's no way to create a totally beautiful clock here
 * with all clock ops implemented.  Instead, we'll just create a clock that can
 * be queried and set the CLK_GET_RATE_NOCACHE attribute to tell common clock
 * framework that we're doing things behind its back.  This should be sufficient
 * to create nice clean device tree bindings and later (if needed) we can try
 * re-architecting SDHCI if we see some benefit to it.
 *
 * Return: 0 on success and error value on error
 */
static int sdhci_arasan_register_sdclk(struct arasan_sdhci_host *host,
				       struct clk *clk_xin,
				       struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 num_clks = 0;

	/* Providing a clock to the PHY is optional; no error if missing */
	if (of_property_read_u32(np, "#clock-cells", &num_clks) < 0)
		return 0;

	if (num_clks) {
		dev_err(dev, "#clock-cells != 0 not yet supported\n");
		return -EINVAL;
	}

	return sdhci_arasan_register_sdcardclk(host, clk_xin, dev);
}

/**
 * sdhci_arasan_unregister_sdclk - Undoes sdhci_arasan_register_sdclk()
 *
 * @dev:		Pointer to our struct device.
 *
 * Should be called any time we're exiting and sdhci_arasan_register_sdclk()
 * returned success.
 */
static void sdhci_arasan_unregister_sdclk(struct device *dev)
{
	struct device_node *np = dev->of_node;

	if (!of_find_property(np, "#clock-cells", NULL))
		return;

	of_clk_del_provider(dev->of_node);
}

static int arasan_sdhci_probe(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct arasan_sdhci_host *arasan_sdhci;
	enum { CLK_AHB, CLK_XIN };
	struct clk_bulk_data clks[] = {
		[CLK_AHB] = { .id = "clk_ahb" },
		[CLK_XIN] = { .id = "clk_xin" },
	};
	struct resource *iores;
	struct mci_host *mci;
	int ret;

	arasan_sdhci = xzalloc(sizeof(*arasan_sdhci));

	mci = &arasan_sdhci->mci;
	mci->hw_dev = dev;

	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return PTR_ERR(iores);

	ret = clk_bulk_get(dev, ARRAY_SIZE(clks), clks);
	if (ret) {
		dev_err(dev, "getting clocks failed: %pe\n", ERR_PTR(ret));
		goto release_region;
	}

	ret = clk_bulk_enable(ARRAY_SIZE(clks), clks);
	if (ret) {
		dev_err(dev, "enabling clocks failed: %pe\n", ERR_PTR(ret));
		goto clk_put;
	}

	arasan_sdhci->clk = clks[CLK_XIN].clk;

	if (of_property_read_bool(np, "xlnx,fails-without-test-cd"))
		arasan_sdhci->quirks |= SDHCI_ARASAN_QUIRK_FORCE_CDTEST;

	if (of_property_read_bool(np, "no-1-8-v"))
		arasan_sdhci->quirks |= SDHCI_ARASAN_QUIRK_NO_1_8_V;

	ret = sdhci_arasan_register_sdclk(arasan_sdhci, arasan_sdhci->clk, dev);
	if (ret)
		goto clk_disable;

	arasan_sdhci->sdhci.base = IOMEM(iores->start);
	arasan_sdhci->sdhci.mci = mci;
	mci->send_cmd = arasan_sdhci_send_cmd;
	mci->set_ios = arasan_sdhci_set_ios;
	mci->init = arasan_sdhci_init;
	mci->card_present = arasan_sdhci_card_present;
	mci->card_write_protected = arasan_sdhci_card_write_protected;

	arasan_sdhci->sdhci.max_clk = clk_get_rate(arasan_sdhci->clk);
	mci->f_max = arasan_sdhci->sdhci.max_clk;
	mci->f_min = 50000000 / 256;

	/* parse board supported bus width capabilities */
	mci_of_parse(&arasan_sdhci->mci);

	dev->priv = arasan_sdhci;

	arasan_sdhci->phy = ERR_PTR(-ENODEV);
	if (of_device_is_compatible(np, "arasan,sdhci-5.1")) {
		arasan_sdhci->phy = phy_get(dev, "phy_arasan");
		if (IS_ERR(arasan_sdhci->phy)) {
			ret = dev_err_probe(dev, PTR_ERR(arasan_sdhci->phy),
					    "no phy for arasan,sdhci-5.1\n");
			goto clk_unreg;
		}

		ret = phy_init(arasan_sdhci->phy);
		if (ret < 0) {
			dev_err(dev, "phy_init err\n");
			goto clk_unreg;
		}
	}

	arasan_sdhci->sdhci.quirks2 |= SDHCI_QUIRK2_CLOCK_DIV_ZERO_BROKEN;
	sdhci_setup_host(&arasan_sdhci->sdhci);

	ret = mci_register(&arasan_sdhci->mci);
	if (ret)
		goto phy_exit;
	return 0;

phy_exit:
	if (!IS_ERR(arasan_sdhci->phy))
		phy_exit(arasan_sdhci->phy);
clk_unreg:
	sdhci_arasan_unregister_sdclk(dev);
clk_disable:
	clk_bulk_disable(ARRAY_SIZE(clks), clks);
clk_put:
	clk_bulk_put(ARRAY_SIZE(clks), clks);
release_region:
	release_region(iores);

	return ret;
}

static __maybe_unused struct of_device_id arasan_sdhci_compatible[] = {
	{ .compatible = "arasan,sdhci-8.9a" },
	{ .compatible = "arasan,sdhci-5.1" },
	{ /* sentinel */ }
};

static struct driver arasan_sdhci_driver = {
	.name = "arasan-sdhci",
	.probe = arasan_sdhci_probe,
	.of_compatible = DRV_OF_COMPAT(arasan_sdhci_compatible),
};
device_platform_driver(arasan_sdhci_driver);
