/*
 * ARM PrimeCell MultiMedia Card Interface - PL180
 *
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Ulf Hansson <ulf.hansson@stericsson.com>
 * Author: Martin Lundholm <martin.xa.lundholm@stericsson.com>
 * Ported to drivers/mmc/ by: Matt Waddel <matt.waddel@linaro.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
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
#include <pinctrl.h>
#include <linux/reset.h>

#include "mmci.h"

#define DRIVER_NAME "mmci-pl18x"

static void ux500v2_variant_init(struct mmci_host *host);

static unsigned long fmax = 515633;

/**
 * struct variant_data - MMCI variant-specific quirks
 * @clkreg: default value for MCICLOCK register
 * @clkreg_enable: enable value for MMCICLOCK register
 * @datalength_bits: number of bits in the MMCIDATALENGTH register
 * @fifosize: number of bytes that can be written when MMCI_TXFIFOEMPTY
 *	      is asserted (likewise for RX)
 * @fifohalfsize: number of bytes that can be written when MCI_TXFIFOHALFEMPTY
 *		  is asserted (likewise for RX)
 * @sdio: variant supports SDIO
 * @st_clkdiv: true if using a ST-specific clock divider algorithm
 * @stm32_clkdiv: true if using a STM32-specific clock divider algorithm
 * @signal_direction: input/out direction of bus signals can be indicated
 * @datactrl_first: true if data must be setup before send command
 * @pwrreg_powerup: power up value for MMCIPOWER register
 * @f_max: maximum clk frequency supported by the controller.
 * @cmdreg_cpsm_enable: enable value for CPSM
 * @cmdreg_lrsp_crc: enable value for long response with crc
 * @cmdreg_srsp_crc: enable value for short response with crc
 * @cmdreg_srsp: enable value for short response without crc
 * @cmdreg_stop: enable value for stop and abort transmission
 * @data_cmd_enable: enable value for data commands.
 * @opendrain: bitmask identifying the OPENDRAIN bit inside MMCIPOWER register
 * @init: optional callback for device specific initialization
 */
struct variant_data {
	unsigned int		clkreg;
	unsigned int		clkreg_enable;
	unsigned int		datalength_bits;
	unsigned int		fifosize;
	unsigned int		fifohalfsize;
	bool			sdio;
	bool			st_clkdiv;
	bool			stm32_clkdiv;
	bool			signal_direction;
	bool			datactrl_first;
	u32			pwrreg_powerup;
	u32			f_max;
	u32			cmdreg_cpsm_enable;
	u32			cmdreg_lrsp_crc;
	u32			cmdreg_srsp_crc;
	u32			cmdreg_srsp;
	u32			cmdreg_stop;
	u32			data_cmd_enable;
	u32			opendrain;
	void			(*init)(struct mmci_host *);
};

static struct variant_data variant_arm = {
	.fifosize		= 16 * 4,
	.fifohalfsize		= 8 * 4,
	.datalength_bits	= 16,
	.pwrreg_powerup		= MCI_PWR_UP,
	.opendrain		= MCI_ROD,
	.f_max			= 100000000,
	.cmdreg_cpsm_enable	= MCI_CPSM_ENABLE,
	.cmdreg_lrsp_crc	= MCI_CPSM_RESPONSE | MCI_CPSM_LONGRSP,
	.cmdreg_srsp_crc	= MCI_CPSM_RESPONSE,
	.cmdreg_srsp		= MCI_CPSM_RESPONSE,
};

static struct variant_data variant_arm_extended_fifo = {
	.fifosize		= 128 * 4,
	.fifohalfsize		= 64 * 4,
	.datalength_bits	= 16,
	.pwrreg_powerup		= MCI_PWR_UP,
	.opendrain		= MCI_ROD,
	.f_max			= 100000000,
	.cmdreg_cpsm_enable	= MCI_CPSM_ENABLE,
	.cmdreg_lrsp_crc	= MCI_CPSM_RESPONSE | MCI_CPSM_LONGRSP,
	.cmdreg_srsp_crc	= MCI_CPSM_RESPONSE,
	.cmdreg_srsp		= MCI_CPSM_RESPONSE,
};

static struct variant_data variant_ux500 = {
	.fifosize		= 30 * 4,
	.fifohalfsize		= 8 * 4,
	.clkreg			= MCI_CLK_ENABLE,
	.cmdreg_cpsm_enable	= MCI_CPSM_ENABLE,
	.clkreg_enable		= MCI_ST_UX500_HWFCEN,
	.datalength_bits	= 24,
	.sdio			= true,
	.st_clkdiv		= true,
	.pwrreg_powerup		= MCI_PWR_ON,
	.signal_direction	= true,
	.opendrain		= MCI_OD,
	.f_max			= 100000000,
	.cmdreg_cpsm_enable	= MCI_CPSM_ENABLE,
	.cmdreg_lrsp_crc	= MCI_CPSM_RESPONSE | MCI_CPSM_LONGRSP,
	.cmdreg_srsp_crc	= MCI_CPSM_RESPONSE,
	.cmdreg_srsp		= MCI_CPSM_RESPONSE,
};

static struct variant_data variant_ux500v2 = {
	.fifosize		= 30 * 4,
	.fifohalfsize		= 8 * 4,
	.clkreg			= MCI_CLK_ENABLE,
	.clkreg_enable		= MCI_ST_UX500_HWFCEN,
	.datalength_bits	= 24,
	.sdio			= true,
	.st_clkdiv		= true,
	.pwrreg_powerup		= MCI_PWR_ON,
	.signal_direction	= true,
	.opendrain		= MCI_OD,
	.f_max			= 100000000,
	.cmdreg_cpsm_enable	= MCI_CPSM_ENABLE,
	.cmdreg_lrsp_crc	= MCI_CPSM_RESPONSE | MCI_CPSM_LONGRSP,
	.cmdreg_srsp_crc	= MCI_CPSM_RESPONSE,
	.cmdreg_srsp		= MCI_CPSM_RESPONSE,
	.init			= ux500v2_variant_init,
};

static struct variant_data variant_stm32_sdmmc = {
	.fifosize		= 16 * 4,
	.fifohalfsize		= 8 * 4,
	.f_max			= 208000000,
	.stm32_clkdiv		= true,
	.cmdreg_cpsm_enable     = MCI_CPSM_STM32_ENABLE,
	.cmdreg_lrsp_crc	= MCI_CPSM_STM32_LRSP_CRC,
	.cmdreg_srsp_crc	= MCI_CPSM_STM32_SRSP_CRC,
	.cmdreg_srsp		= MCI_CPSM_STM32_SRSP,
	.cmdreg_stop		= MCI_CPSM_STM32_CMDSTOP,
	.data_cmd_enable	= MCI_CPSM_STM32_CMDTRANS,
	.datactrl_first         = true,
	.datalength_bits	= 25,
	.signal_direction	= true,
	.init			= sdmmc_variant_init,
};

#define to_mci_host(mci)	container_of(mci, struct mmci_host, mci)

static inline u32 mmci_readl(struct mmci_host *host, u32 offset)
{
	return readl(host->base + offset);
}

static inline void mmci_writel(struct mmci_host *host, u32 offset,
				    u32 value)
{
	writel(value, host->base + offset);
}

static inline int mmci_set_opendrain(struct mmci_host *host, u32 *pwr, bool enable)
{
	int ret = 0;
	u32 opendrain = host->variant->opendrain;

	if (opendrain) {
		if (enable)
			*pwr |= opendrain;
		else
			*pwr &= ~opendrain;
	} else {
		if (enable)
			ret = pinctrl_select_state(host->hw_dev, "opendrain");
		else
			ret = pinctrl_select_state(host->hw_dev, "default");
	}

	return ret;
}

static int wait_for_command_end(struct mci_host *mci, struct mci_cmd *cmd)
{
	u32 hoststatus, statusmask;
	struct mmci_host *host = to_mci_host(mci);

	statusmask = MCI_CMDTIMEOUT | MCI_CMDCRCFAIL;
	if ((cmd->resp_type & MMC_RSP_PRESENT))
		statusmask |= MCI_CMDRESPEND;
	else
		statusmask |= MCI_CMDSENT;

	do
		hoststatus = mmci_readl(host, MMCISTATUS) & statusmask;
	while (!hoststatus);

	dev_dbg(host->hw_dev, "SDI_ICR <= 0x%08X\n", statusmask);
	dev_dbg(host->hw_dev, "status <= 0x%08X\n", hoststatus);
	mmci_writel(host, MMCICLEAR, statusmask);
	if (hoststatus & MCI_CMDTIMEOUT) {
		dev_dbg(host->hw_dev, "CMD%d time out\n", cmd->cmdidx);
		return -ETIMEDOUT;
	} else if ((hoststatus & MCI_CMDCRCFAIL) &&
		   (cmd->resp_type & MMC_RSP_CRC)) {
		dev_err(host->hw_dev, "CMD%d CRC error\n", cmd->cmdidx);
		return -EILSEQ;
	}

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		cmd->response[0] = mmci_readl(host, MMCIRESPONSE0);
		cmd->response[1] = mmci_readl(host, MMCIRESPONSE1);
		cmd->response[2] = mmci_readl(host, MMCIRESPONSE2);
		cmd->response[3] = mmci_readl(host, MMCIRESPONSE3);
		dev_dbg(host->hw_dev, "CMD%d response[0]:0x%08X, response[1]:0x%08X, "
			"response[2]:0x%08X, response[3]:0x%08X\n",
			cmd->cmdidx, cmd->response[0], cmd->response[1],
			cmd->response[2], cmd->response[3]);
	}

	return 0;
}

/* send command to the mmc card and wait for results */
static int do_command(struct mci_host *mci, struct mci_cmd *cmd, bool adtc)
{
	int result;
	u32 sdi_cmd = 0;
	struct mmci_host *host = to_mci_host(mci);
	struct variant_data *variant = host->variant;

	dev_dbg(host->hw_dev, "Request to do CMD%d\n", cmd->cmdidx);

	if (mmci_readl(host, MMCICOMMAND) & variant->cmdreg_cpsm_enable) {
		mmci_writel(host, MMCICOMMAND, 0);
		udelay(COMMAND_REG_DELAY);
	}

	if (variant->cmdreg_stop && cmd->cmdidx == MMC_CMD_STOP_TRANSMISSION)
		sdi_cmd |= variant->cmdreg_stop;

	sdi_cmd |= ((cmd->cmdidx & MCI_CMDINDEXMASK) | variant->cmdreg_cpsm_enable);

	if (cmd->resp_type) {
		if (cmd->resp_type & MMC_RSP_136)
			sdi_cmd |= variant->cmdreg_lrsp_crc;
		else if (cmd->resp_type & MMC_RSP_CRC)
			sdi_cmd |= variant->cmdreg_srsp_crc;
		else
			sdi_cmd |= variant->cmdreg_srsp;
	}

	dev_dbg(host->hw_dev, "SDI_ARG <= 0x%08X\n", cmd->cmdarg);
	mmci_writel(host, MMCIARGUMENT, (u32)cmd->cmdarg);
	udelay(COMMAND_REG_DELAY);
	dev_dbg(host->hw_dev, "SDI_CMD <= 0x%08X\n", sdi_cmd);

	if (adtc)
		sdi_cmd |= variant->data_cmd_enable;

	mmci_writel(host, MMCICOMMAND, sdi_cmd);
	result = wait_for_command_end(mci, cmd);

	/* After CMD3 open drain is switched off and push pull is used. */
	if ((result == 0) && (cmd->cmdidx == MMC_CMD_SET_RELATIVE_ADDR)) {
		u32 sdi_pwr = host->pwr_reg;
		mmci_set_opendrain(host, &sdi_pwr, false);
		/* no-op if there's no MCI_(OD|ROD) bit */
		mmci_write_pwrreg(host, sdi_pwr);
	}

	return result;
}

static u64 mmci_pio_read(struct mmci_host *host, char *buffer, unsigned int host_remain)
{
	void __iomem *base = host->base;
	char *ptr = buffer;
	u32 status;
	struct variant_data *variant = host->variant;

	do {
		int count = host_remain - (readl(base + MMCIFIFOCNT) << 2);

		if (count > host_remain)
			count = host_remain;

		if (count > variant->fifosize)
			count = variant->fifosize;

		if (count <= 0)
			break;

		/*
		 * SDIO especially may want to send something that is
		 * not divisible by 4 (as opposed to card sectors
		 * etc). Therefore make sure to always read the last bytes
		 * while only doing full 32-bit reads towards the FIFO.
		 */
		if (unlikely(count & 0x3)) {
			if (count < 4) {
				unsigned char buf[4];
				readsl(base + MMCIFIFO, buf, 1);
				memcpy(ptr, buf, count);
			} else {
				readsl(base + MMCIFIFO, ptr, count >> 2);
				count &= ~0x3;
			}
		} else {
			readsl(base + MMCIFIFO, ptr, count >> 2);
		}

		ptr += count;
		host_remain -= count;

		if (host_remain == 0)
			break;

		status = readl(base + MMCISTATUS);
	} while (status & MCI_RXDATAAVLBL);

	return ptr - buffer;
}

static int read_bytes(struct mci_host *mci, char *dest, unsigned int blkcount, unsigned int blksize)
{
	unsigned int xfercount = blkcount * blksize;
	struct mmci_host *host = to_mci_host(mci);
	u32 status, status_err;
	int len;

	dev_dbg(host->hw_dev, "read_bytes: blkcount=%u blksize=%u\n", blkcount, blksize);

	do {
		len = mmci_pio_read(host, dest, xfercount);
		xfercount -= len;
		dest += len;
		status = mmci_readl(host, MMCISTATUS);
		status_err = status & (MCI_DATACRCFAIL | MCI_DATATIMEOUT |
			     MCI_RXOVERRUN);
	} while(xfercount && !status_err);

	status_err = status &
		(MCI_DATACRCFAIL | MCI_DATATIMEOUT | MCI_DATABLOCKEND |
		 MCI_RXOVERRUN);

	while (!status_err) {
		status = mmci_readl(host, MMCISTATUS);
		status_err = status &
			(MCI_DATACRCFAIL | MCI_DATATIMEOUT | MCI_DATABLOCKEND |
			 MCI_RXOVERRUN);
	}

	if (status & MCI_DATATIMEOUT) {
		dev_err(host->hw_dev, "Read data timed out, xfercount: %u, status: 0x%08X\n",
			xfercount, status);
		return -ETIMEDOUT;
	} else if (status & MCI_DATACRCFAIL) {
		dev_err(host->hw_dev, "Read data bytes CRC error: 0x%x\n", status);
		return -EILSEQ;
	} else if (status & MCI_RXOVERRUN) {
		dev_err(host->hw_dev, "Read data RX overflow error\n");
		return -EIO;
	}

	mmci_writel(host, MMCICLEAR, MCI_ICR_MASK);

	if (xfercount) {
		dev_err(host->hw_dev, "Read data error, xfercount: %u\n", xfercount);
		return -ENOBUFS;
	}

	return 0;
}

static int mmci_pio_write(struct mmci_host *host, char *buffer, unsigned int remain, u32 status)
{
	struct variant_data *variant = host->variant;
	void __iomem *base = host->base;
	char *ptr = buffer;

	do {
		unsigned int count, maxcnt;

		maxcnt = status & MCI_TXFIFOEMPTY ?
			 variant->fifosize : variant->fifohalfsize;
		count = min(remain, maxcnt);

		/*
		 * SDIO especially may want to send something that is
		 * not divisible by 4 (as opposed to card sectors
		 * etc), and the FIFO only accept full 32-bit writes.
		 * So compensate by adding +3 on the count, a single
		 * byte become a 32bit write, 7 bytes will be two
		 * 32bit writes etc.
		 */
		writesl(base + MMCIFIFO, ptr, (count + 3) >> 2);

		ptr += count;
		remain -= count;

		if (remain == 0)
			break;

		status = readl(base + MMCISTATUS);
	} while (status & MCI_TXFIFOHALFEMPTY);

	return ptr - buffer;
}

static int write_bytes(struct mci_host *mci, char *dest, unsigned int blkcount, unsigned int blksize)
{
	unsigned int xfercount = blkcount * blksize;
	struct mmci_host *host = to_mci_host(mci);
	u32 status, status_err;
	int len;

	dev_dbg(host->hw_dev, "write_bytes: blkcount=%u blksize=%u\n", blkcount, blksize);

	status = mmci_readl(host, MMCISTATUS);
	status_err = status & (MCI_DATACRCFAIL | MCI_DATATIMEOUT);

	do {
		len = mmci_pio_write(host, dest, xfercount, status);
		xfercount -= len;
		dest += len;

		status = mmci_readl(host, MMCISTATUS);
		status_err = status & (MCI_DATACRCFAIL | MCI_DATATIMEOUT);
	} while (!status_err && xfercount);

	status_err = status &
		(MCI_DATACRCFAIL | MCI_DATATIMEOUT | MCI_DATABLOCKEND);
	while (!status_err) {
		status = mmci_readl(host, MMCISTATUS);
		status_err = status &
			(MCI_CMDCRCFAIL | MCI_DATATIMEOUT | MCI_DATABLOCKEND);
	}

	if (status & MCI_DATATIMEOUT) {
		dev_err(host->hw_dev, "Write data timed out, xfercount:%u,status:0x%08X\n",
		       xfercount, status);
		return -ETIMEDOUT;
	} else if (status & MCI_DATACRCFAIL) {
		dev_err(host->hw_dev, "Write data CRC error\n");
		return -EILSEQ;
	}

	mmci_writel(host, MMCICLEAR, MCI_ICR_MASK);

	if (xfercount) {
		dev_err(host->hw_dev, "Write data error, xfercount:%u", xfercount);
		return -ENOBUFS;
	}

	return 0;
}

static int do_data_transfer(struct mci_host *mci, struct mci_cmd *cmd, struct mci_data *data)
{
	int error = -ETIMEDOUT;
	struct mmci_host *host = to_mci_host(mci);
	u32 data_ctrl;
	u32 data_len = (u32) (data->blocks * data->blocksize);
	bool adtc = cmd->resp_type != 0;


	data_ctrl = host->ops->get_datactrl_cfg(host, data->blocksize);

	if (data_ctrl & MCI_ST_DPSM_DDRMODE)
		dev_dbg(host->hw_dev, "MCI_ST_DPSM_DDRMODE\n");

	mmci_writel(host, MMCIDATATIMER, MCI_DTIMER_DEFAULT);
	mmci_writel(host, MMCIDATALENGTH, data_len);
	udelay(DATA_REG_DELAY);

	if (!host->variant->datactrl_first) {
		error = do_command(mci, cmd, adtc);
		if (error)
			return error;
	}

	if (data->flags & MMC_DATA_READ)
		data_ctrl |= MCI_DPSM_DIRECTION;

	mmci_writel(host, MMCIDATACTRL, data_ctrl);

	if (data->flags & MMC_DATA_READ)
		error = read_bytes(mci, data->dest, data->blocks,
				   data->blocksize);
	else if (data->flags & MMC_DATA_WRITE)
		error = write_bytes(mci, (char *)data->src, data->blocks,
				    data->blocksize);

	if (error)
		return error;

	if (host->variant->datactrl_first)
		error = do_command(mci, cmd, adtc);

	return error;
}

static int mci_request(struct mci_host *mci, struct mci_cmd *cmd, struct mci_data *data)
{
	int result;

	if (data)
		result = do_data_transfer(mci, cmd, data);
	else
		result = do_command(mci, cmd, false);

	return result;
}

/* MMC uses open drain drivers in the enumeration phase */
static int mci_reset(struct mci_host *mci, struct device_d *mci_dev)
{
	struct mmci_host *host = to_mci_host(mci);
	struct variant_data *variant = host->variant;

	u32 pwr = variant->pwrreg_powerup;

	if (variant->signal_direction) {
		/*
		 * The ST Micro variant has some additional bits
		 * indicating signal direction for the signals in
		 * the SD/MMC bus and feedback-clock usage.
		 */
		pwr |= host->plat->sigdir;
	}

	mmci_set_opendrain(host, &pwr, true);

	if (host->rst) {
		reset_control_assert(host->rst);
		udelay(2);
		reset_control_deassert(host->rst);
	}

	host->ops->set_pwrreg(host, pwr);

	return 0;
}

void mmci_write_clkreg(struct mmci_host *host, u32 clk)
{
	if (host->clk_reg != clk) {
		host->clk_reg = clk;
		mmci_writel(host, MMCICLOCK, clk);
	}
}

static void mmci_set_clkreg(struct mmci_host *host, struct mci_ios *ios)
{
	struct mci_host *mci = &host->mci;
	u32 sdi_clkcr;

	sdi_clkcr = host->clk_reg;

	/* Ramp up the clock rate */
	if (mci->clock) {
		u32 clkdiv = 0;
		u32 tmp_clock;

		dev_dbg(host->hw_dev, "setting clock and bus width in the host:");
		if (mci->clock >= mci->f_max) {
			clkdiv = 0;
			mci->clock = mci->f_max;
		} else {
			clkdiv = (host->mclk / mci->clock) - 2;
		}
		tmp_clock = host->mclk / (clkdiv + 2);
		while (tmp_clock > mci->clock) {
			clkdiv++;
			tmp_clock = host->mclk / (clkdiv + 2);
		}
		if (clkdiv > MCI_CLK_CLKDIV_MASK)
			clkdiv = MCI_CLK_CLKDIV_MASK;
		tmp_clock = host->mclk / (clkdiv + 2);
		mci->clock = tmp_clock;
		sdi_clkcr &= ~(MCI_CLK_CLKDIV_MASK);
		sdi_clkcr |= clkdiv;
	}

	/* Set the bus width */
	if (mci->bus_width) {
		u32 buswidth = 0;

		switch (mci->bus_width) {
		case MMC_BUS_WIDTH_1:
			buswidth |= MCI_1BIT_BUS;
			break;
		case MMC_BUS_WIDTH_4:
			buswidth |= MCI_4BIT_BUS;
			break;
		case MMC_BUS_WIDTH_8:
			buswidth |= MCI_ST_8BIT_BUS;
			break;
		default:
			dev_err(host->hw_dev, "Invalid bus width (%d)\n", mci->bus_width);
			break;
		}
		sdi_clkcr &= ~(MCI_xBIT_BUS_MASK);
		sdi_clkcr |= buswidth;
	}

	mmci_write_clkreg(host, sdi_clkcr);
}

static void mci_set_ios(struct mci_host *mci, struct mci_ios *ios)
{
	struct mmci_host *host = to_mci_host(mci);
	host->ops->set_clkreg(host, ios);
	udelay(CLK_CHANGE_DELAY);
}


void mmci_write_pwrreg(struct mmci_host *host, u32 pwr)
{
	if (host->pwr_reg != pwr) {
		host->pwr_reg = pwr;
		mmci_writel(host, MMCIPOWER, pwr);
	}
}

static u32 mmci_get_dctrl_cfg(struct mmci_host *host, unsigned int blocksize)
{
        return MCI_DPSM_ENABLE | mmci_dctrl_blksz(host, blocksize);
}

static u32 ux500v2_get_dctrl_cfg(struct mmci_host *host, unsigned int blocksize)
{
        return MCI_DPSM_ENABLE | (blocksize << 16);
}

static struct mmci_host_ops mmci_default_ops = {
        .get_datactrl_cfg = mmci_get_dctrl_cfg,
	.set_clkreg = mmci_set_clkreg,
	.set_pwrreg = mmci_write_pwrreg,
};

void ux500v2_variant_init(struct mmci_host *host)
{
        host->ops->get_datactrl_cfg = ux500v2_get_dctrl_cfg;
}

static int mmci_of_parse(struct device_node *np,
			 struct mmci_platform_data *plat)
{
	if (!IS_ENABLED(CONFIG_OFDEVICE))
		return 0;

	if (of_get_property(np, "st,sig-dir-dat0", NULL))
		plat->sigdir |= MCI_ST_DATA0DIREN;
	if (of_get_property(np, "st,sig-dir-dat2", NULL))
		plat->sigdir |= MCI_ST_DATA2DIREN;
	if (of_get_property(np, "st,sig-dir-dat31", NULL))
		plat->sigdir |= MCI_ST_DATA31DIREN;
	if (of_get_property(np, "st,sig-dir-dat74", NULL))
		plat->sigdir |= MCI_ST_DATA74DIREN;
	if (of_get_property(np, "st,sig-dir-cmd", NULL))
		plat->sigdir |= MCI_ST_CMDDIREN;
	if (of_get_property(np, "st,sig-pin-fbclk", NULL))
		plat->sigdir |= MCI_ST_FBCLKEN;
	if (of_get_property(np, "st,sig-dir", NULL))
		plat->sigdir |= MCI_STM32_DIRPOL;
	if (of_get_property(np, "st,neg-edge", NULL))
		plat->clk_reg_add |= MCI_STM32_CLK_NEGEDGE;
	if (of_get_property(np, "st,use-ckin", NULL))
		plat->clk_reg_add |= MCI_STM32_CLK_SELCKIN;


	if (of_get_property(np, "mmc-cap-mmc-highspeed", NULL))
		plat->capabilities |= MMC_CAP_MMC_HIGHSPEED;
	if (of_get_property(np, "mmc-cap-sd-highspeed", NULL))
		plat->capabilities |= MMC_CAP_SD_HIGHSPEED;

	return 0;
}

static int mmci_probe(struct amba_device *dev, const struct amba_id *id)
{
	struct device_d *hw_dev = &dev->dev;
	struct device_node *np = hw_dev->device_node;
	struct mmci_platform_data *plat = hw_dev->platform_data;
	struct variant_data *variant = id->data;
	u32 sdi_u32, pwrreg, clkreg;
	struct mmci_host *host;
	struct clk *clk;
	int ret;

	if (!plat && !np) {
		dev_err(hw_dev, "missing platform data or DT node\n");
		return -EINVAL;
	}

	if (!plat)
		plat = xzalloc(sizeof(*plat));

	mmci_of_parse(np, plat);

	host = xzalloc(sizeof(*host));

	host->base = amba_get_mem_region(dev);
	host->mci.send_cmd = mci_request;
	host->mci.set_ios = mci_set_ios;
	host->mci.init = mci_reset;
	host->hw_dev = host->mci.hw_dev = hw_dev;

	clk = clk_get(hw_dev, NULL);
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		goto host_free;
	}

	ret = clk_enable(clk);
	if (ret)
		goto host_free;

	host->hw_designer = amba_manf(dev);
	host->hw_revision = amba_rev(dev);

	dev_dbg(hw_dev, "hw_designer = 0x%x\n", host->hw_designer);
	dev_dbg(hw_dev, "hw_revision = 0x%x\n", host->hw_revision);

	host->variant = variant;
	host->plat = plat;

	host->ops = &mmci_default_ops;

	if (variant->init)
		variant->init(host);

	host->rst = reset_control_get(hw_dev, NULL);
	if (IS_ERR(host->rst))
		host->rst = NULL;

	if (host->rst) {
		reset_control_assert(host->rst);
		udelay(2);
		reset_control_deassert(host->rst);
	}

	pwrreg = plat->sigdir | variant->pwrreg_powerup;
	host->ops->set_pwrreg(host, pwrreg);

	clkreg = plat->clkdiv_init | variant->clkreg_enable | variant->clkreg;
	mmci_write_clkreg(host, clkreg);

	udelay(CLK_CHANGE_DELAY);

	/* Disable mmc interrupts */
	sdi_u32 = mmci_readl(host, MMCIMASK0) & ~MCI_MASK0_MASK;
	mmci_writel(host, MMCIMASK0, sdi_u32);

	host->mclk = clk_get_rate(clk);

	/*
	 * According to the spec, mclk is max 100 MHz,
	 * so we try to adjust the clock down to this,
	 * (if possible).
	 */
	if (host->mclk > variant->f_max) {
		ret = clk_set_rate(clk, variant->f_max);
		if (ret < 0)
			goto clk_disable;
		host->mclk = clk_get_rate(clk);
		dev_dbg(hw_dev, "eventual mclk rate: %lu Hz\n", host->mclk);
	}

	/*
	 * The ARM and ST versions of the block have slightly different
	 * clock divider equations which means that the minimum divider
	 * differs too.
	 */
	if (variant->st_clkdiv)
		host->mci.f_min = DIV_ROUND_UP(host->mclk, 257);
	else if (variant->stm32_clkdiv)
		host->mci.f_min = DIV_ROUND_UP(host->mclk, 2046);
	else
		host->mci.f_min = DIV_ROUND_UP(host->mclk, 512);
	/*
	 * If the platform data supplies a maximum operating
	 * frequency, this takes precedence. Else, we fall back
	 * to using the module parameter, which has a (low)
	 * default value in case it is not specified. Either
	 * value must not exceed the clock rate into the block,
	 * of course.
	 */
	if (plat->f_max)
		host->mci.f_max = min(host->mclk, plat->f_max);
	else
		host->mci.f_max = min(host->mclk, fmax);
	dev_dbg(hw_dev, "clocking block at %u Hz\n", host->mci.f_max);

	host->mci.max_req_size = (1 << variant->datalength_bits) - 1;

	host->mci.host_caps = plat->capabilities;
	host->mci.voltages = MMC_VDD_32_33 | MMC_VDD_33_34 | plat->ocr_mask;

	mci_register(&host->mci);

	return 0;

clk_disable:
	clk_disable(clk);
host_free:
	free(host);
	return ret;
}

static struct amba_id mmci_ids[] = {
	{
		.id	= 0x00041180,
		.mask	= 0xff0fffff,
		.data	= &variant_arm,
	},
	{
		.id	= 0x01041180,
		.mask	= 0xff0fffff,
		.data	= &variant_arm_extended_fifo,
	},
	{
		.id	= 0x00041181,
		.mask	= 0x000fffff,
		.data	= &variant_arm,
	},
	/* ST Micro variants */
	{
		.id	= 0x00480180,
		.mask	= 0xf0ffffff,
		.data	= &variant_ux500,
	},
	{
		.id	= 0x10480180,
		.mask	= 0xf0ffffff,
		.data	= &variant_ux500v2,
	},
	{
		.id     = 0x10153180,
		.mask	= 0xf0ffffff,
		.data	= &variant_stm32_sdmmc,
	},
	{ 0, 0 },
};

static struct amba_driver mmci_driver = {
	.drv		= {
		.name	= DRIVER_NAME,
	},
	.probe		= mmci_probe,
	.id_table	= mmci_ids,
};

static int mmci_init(void)
{
	amba_driver_register(&mmci_driver);
	return 0;
}
device_initcall(mmci_init);
