// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 STMicroelectronics
 * Copyright (C) 2019 Ahmad Fatoum, Pengutronix
 */

#include <common.h>
#include <init.h>
#include <mci.h>
#include <io.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <errno.h>
#include <malloc.h>
#include <linux/amba/bus.h>
#include <linux/amba/mmci.h>

#include "mmci.h"

#define DRIVER_NAME "mmci-pl18x"

static void mmci_sdmmc_set_clkreg(struct mmci_host *host, struct mci_ios *ios)
{
	struct mci_host *mci = &host->mci;
	unsigned int clk = 0, ddr = 0;

	if (ios && (ios->timing == MMC_TIMING_MMC_DDR52 ||
		    ios->timing == MMC_TIMING_UHS_DDR50))
		ddr = MCI_STM32_CLK_DDR;

	/*
	 * clock = mclk / (2 * clkdiv)
	 * clkdiv 0 => bypass
	 * in ddr mode bypass is not possible
	 */
	if (mci->clock) {
		if (mci->clock >= host->mclk && !ddr) {
			mci->clock = host->mclk;
		} else {
			clk = DIV_ROUND_UP(host->mclk, 2 * mci->clock);
			if (clk > MCI_STM32_CLK_CLKDIV_MSK)
				clk = MCI_STM32_CLK_CLKDIV_MSK;
			mci->clock = host->mclk / (2 * clk);
		}
	} else {
		/*
		 * while power-on phase the clock can't be defined to 0,
		 * Only power-off and power-cyc deactivate the clock.
		 * if mci->clock is 0, set max divider
		 */
		clk = MCI_STM32_CLK_CLKDIV_MSK;
		mci->clock = host->mclk / (2 * clk);
	}

	if (mci->bus_width == MMC_BUS_WIDTH_4)
		clk |= MCI_STM32_CLK_WIDEBUS_4;
	if (mci->bus_width == MMC_BUS_WIDTH_8)
		clk |= MCI_STM32_CLK_WIDEBUS_8;

	clk |= MCI_STM32_CLK_HWFCEN;
	clk |= host->plat->clk_reg_add;
	clk |= ddr;

	/*
	 * SDMMC_FBCK is selected when an external Delay Block is needed
	 * with SDR104.
	 */
	if (ios && ios->timing >= MMC_TIMING_UHS_SDR50) {
		clk |= MCI_STM32_CLK_BUSSPEED;
		if (ios->timing == MMC_TIMING_UHS_SDR104) {
			clk &= ~MCI_STM32_CLK_SEL_MSK;
			clk |= MCI_STM32_CLK_SELFBCK;
		}
	}

	mmci_write_clkreg(host, clk);
}

static void mmci_sdmmc_set_pwrreg(struct mmci_host *host, unsigned int pwr)
{
	/*
	 * After a power-cycle state, we must set the SDMMC in
	 * Power-off. The SDMMC_D[7:0], SDMMC_CMD and SDMMC_CK are
	 * driven high. Then we can set the SDMMC to Power-on state
	 */
	mmci_write_pwrreg(host, MCI_PWR_OFF | pwr);
	mdelay(1);
	mmci_write_pwrreg(host, MCI_PWR_ON | pwr);
}

static u32 sdmmc_get_dctrl_cfg(struct mmci_host *host, unsigned int blocksize)
{
        u32 datactrl;

        datactrl = mmci_dctrl_blksz(host, blocksize);

        if (/* stop command */0) // FIXME is this ok?
                datactrl |= MCI_DPSM_STM32_MODE_BLOCK_STOP;
        else
                datactrl |= MCI_DPSM_STM32_MODE_BLOCK;

        return datactrl;
}

struct mmci_host_ops sdmmc_variant_ops = {
        .get_datactrl_cfg = sdmmc_get_dctrl_cfg,
        .set_clkreg = mmci_sdmmc_set_clkreg,
        .set_pwrreg = mmci_sdmmc_set_pwrreg,
};

void sdmmc_variant_init(struct mmci_host *host)
{
	host->ops = &sdmmc_variant_ops;
}
