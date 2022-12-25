// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2016 Fuzhou Rockchip Electronics Co., Ltd
 *
 * Rockchip SD __sdhci Controller Interface
 */
#define DEBUG 1
#include <common.h>
#include <linux/clk.h>
#include <driver.h>
#include <clock.h>
#include <linux/err.h>
#include <of.h>
#include <linux/iopoll.h>
#include <mci.h>
#include <sdhci.h>
#include "rockchip-sdhci.h"

#define RK_CLRSETBITS(clr, set)         ((((clr) | (set)) << 16) | (set))
#define RK_SETBITS(set)                 RK_CLRSETBITS(0, set)
#define RK_CLRBITS(clr)                 RK_CLRSETBITS(clr, 0)

#define rk_clrsetreg(addr, clr, set)    \
                                writel(((clr) | (set)) << 16 | (set), addr)
#define rk_clrreg(addr, clr)            writel((clr) << 16, addr)
#define rk_setreg(addr, set)            writel((set) << 16 | (set), addr)


/* 400KHz is max freq for card ID etc. Use that as min */
#define EMMC_MIN_FREQ	400000
#define KHz	(1000)
#define MHz	(1000 * KHz)

#define PHYCTRL_CALDONE_MASK		0x1
#define PHYCTRL_CALDONE_SHIFT		0x6
#define PHYCTRL_CALDONE_DONE		0x1
#define PHYCTRL_DLLRDY_MASK		0x1
#define PHYCTRL_DLLRDY_SHIFT		0x5
#define PHYCTRL_DLLRDY_DONE		0x1
#define PHYCTRL_FREQSEL_200M		0x0
#define PHYCTRL_FREQSEL_50M		0x1
#define PHYCTRL_FREQSEL_100M		0x2
#define PHYCTRL_FREQSEL_150M		0x3
#define PHYCTRL_DLL_LOCK_WO_TMOUT(x)	\
	((((x) >> PHYCTRL_DLLRDY_SHIFT) & PHYCTRL_DLLRDY_MASK) ==\
	PHYCTRL_DLLRDY_DONE)

struct rockchip_emmc_phy {
	u32 emmcphy_con[7];
	u32 reserved;
	u32 emmcphy_status;
};

struct rockchip_sdhc {
	struct rk_sdhc_host __sdhci;
	struct rockchip_emmc_phy *phy;
#if 0
	struct sdhci		sdhci;
#endif
};

static inline
struct rk_sdhc_host *to_rk_sdhc_host(struct mci_host *mci)
{
	return container_of(mci, struct rk_sdhc_host, mci);
}

static void rk_sdhc_reset(struct rk_sdhc_host *__sdhci, u8 mask)
{
	unsigned long timeout;

	/* Wait max 100 ms */
	timeout = 100;
	rk_sdhc_writeb(__sdhci, mask, RK_SDHC_SOFTWARE_RESET);
	while (rk_sdhc_readb(__sdhci, RK_SDHC_SOFTWARE_RESET) & mask) {
		if (timeout == 0) {
			pr_warn("%s: Reset 0x%x never completed.\n",
			       __func__, (int)mask);
			return;
		}
		timeout--;
		udelay(1000);
	}
}

static void rk_sdhc_cmd_done(struct rk_sdhc_host *__sdhci, struct mci_cmd *cmd)
{
	int i;
	if (cmd->resp_type & MMC_RSP_136) {
		/* CRC is stripped so we need to do some shifting. */
		for (i = 0; i < 4; i++) {
			cmd->response[i] = rk_sdhc_readl(__sdhci,
					RK_SDHC_RESPONSE + (3-i)*4) << 8;
			if (i != 3)
				cmd->response[i] |= rk_sdhc_readb(__sdhci,
						RK_SDHC_RESPONSE + (3-i)*4-1);
		}
	} else {
		cmd->response[0] = rk_sdhc_readl(__sdhci, RK_SDHC_RESPONSE);
	}
}

static void rk_sdhc_transfer_pio(struct rk_sdhc_host *__sdhci, struct mci_data *data)
{
	int i;
	char *offs;
	for (i = 0; i < data->blocksize; i += 4) {
		offs = data->dest + i;
		if (data->flags == MMC_DATA_READ)
			*(u32 *)offs = rk_sdhc_readl(__sdhci, RK_SDHC_BUFFER);
		else
			rk_sdhc_writel(__sdhci, *(u32 *)offs, RK_SDHC_BUFFER);
	}
}

static int rk_sdhc_transfer_data(struct rk_sdhc_host *__sdhci, struct mci_data *data)
{
	unsigned int stat, rdy, mask, timeout, block = 0;
	bool transfer_done = false;

	timeout = 1000000;
	rdy = RK_SDHC_INT_SPACE_AVAIL | RK_SDHC_INT_DATA_AVAIL;
	mask = RK_SDHC_DATA_AVAILABLE | RK_SDHC_SPACE_AVAILABLE;
	do {
		stat = rk_sdhc_readl(__sdhci, RK_SDHC_INT_STATUS);
		if (stat & RK_SDHC_INT_ERROR) {
			pr_debug("%s: Error detected in status(0x%X)!\n",
				 __func__, stat);
			return -EIO;
		}
		if (!transfer_done && (stat & rdy)) {
			if (!(rk_sdhc_readl(__sdhci, RK_SDHC_PRESENT_STATE) & mask))
				continue;
			rk_sdhc_writel(__sdhci, rdy, RK_SDHC_INT_STATUS);
			rk_sdhc_transfer_pio(__sdhci, data);
			data->dest += data->blocksize;
			if (++block >= data->blocks) {
				/* Keep looping until the RK_SDHC_INT_DATA_END is
				 * cleared, even if we finished sending all the
				 * blocks.
				 */
				transfer_done = true;
				continue;
			}
		}
		if (timeout-- > 0)
			udelay(10);
		else {
			pr_warn("%s: Transfer data timeout\n", __func__);
			return -ETIMEDOUT;
		}
	} while (!(stat & RK_SDHC_INT_DATA_END));

	return 0;
}

/*
 * No command will be sent by driver if card is busy, so driver must wait
 * for card ready state.
 * Every time when card is busy after timeout then (last) timeout value will be
 * increased twice but only if it doesn't exceed global defined maximum.
 * Each function call will use last timeout value.
 */
#define RK_SDHC_CMD_MAX_TIMEOUT			3200
#define RK_SDHC_CMD_DEFAULT_TIMEOUT		100
#define RK_SDHC_READ_STATUS_TIMEOUT		SECOND

static int rk_sdhc_send_command(struct mci_host *mci, struct mci_cmd *cmd,
			      struct mci_data *data)
{
	struct rk_sdhc_host *__sdhci = to_rk_sdhc_host(mci);

	unsigned int stat = 0;
	int ret = 0;
	int trans_bytes = 0;
	u32 mask, flags, mode = 0;
	unsigned int time = 0;
	u64 start = get_time_ns();

	/* Timeout unit - ms */
	static unsigned int cmd_timeout = RK_SDHC_CMD_DEFAULT_TIMEOUT;

	mask = RK_SDHC_CMD_INHIBIT | RK_SDHC_DATA_INHIBIT;

	/* We shouldn't wait for data inihibit for stop commands, even
	   though they might use busy signaling */
	if (cmd->cmdidx == MMC_CMD_STOP_TRANSMISSION)
		mask &= ~RK_SDHC_DATA_INHIBIT;

	while (rk_sdhc_readl(__sdhci, RK_SDHC_PRESENT_STATE) & mask) {
		if (time >= cmd_timeout) {
			pr_notice("%s: MMC: %d busy ", __func__, 0);
			if (2 * cmd_timeout <= RK_SDHC_CMD_MAX_TIMEOUT) {
				cmd_timeout += cmd_timeout;
				pr_warn("timeout increasing to: %u ms.\n",
				       cmd_timeout);
			} else {
				puts("timeout.\n");
				return -ECOMM;
			}
		}
		time++;
		udelay(1000);
	}

	rk_sdhc_writel(__sdhci, RK_SDHC_INT_ALL_MASK, RK_SDHC_INT_STATUS);

	mask = RK_SDHC_INT_RESPONSE;

	if (!(cmd->resp_type & MMC_RSP_PRESENT))
		flags = RK_SDHC_CMD_RESP_NONE;
	else if (cmd->resp_type & MMC_RSP_136)
		flags = RK_SDHC_CMD_RESP_LONG;
	else if (cmd->resp_type & MMC_RSP_BUSY) {
		flags = RK_SDHC_CMD_RESP_SHORT_BUSY;
		mask |= RK_SDHC_INT_DATA_END;
	} else
		flags = RK_SDHC_CMD_RESP_SHORT;

	if (cmd->resp_type & MMC_RSP_CRC)
		flags |= RK_SDHC_CMD_CRC;
	if (cmd->resp_type & MMC_RSP_OPCODE)
		flags |= RK_SDHC_CMD_INDEX;
	if (data)
		flags |= RK_SDHC_CMD_DATA;

	/* Set Transfer mode regarding to data flag */
	if (data) {
		rk_sdhc_writeb(__sdhci, 0xe, RK_SDHC_TIMEOUT_CONTROL);

		mode = RK_SDHC_TRNS_BLK_CNT_EN;
		trans_bytes = data->blocks * data->blocksize;
		if (data->blocks > 1)
			mode |= RK_SDHC_TRNS_MULTI | RK_SDHC_TRNS_BLK_CNT_EN;

		if (data->flags == MMC_DATA_READ)
			mode |= RK_SDHC_TRNS_READ;

		rk_sdhc_writew(__sdhci, RK_SDHC_MAKE_BLKSZ(RK_SDHC_DEFAULT_BOUNDARY_ARG,
				data->blocksize),
				RK_SDHC_BLOCK_SIZE);
		rk_sdhc_writew(__sdhci, data->blocks, RK_SDHC_BLOCK_COUNT);
		rk_sdhc_writew(__sdhci, mode, RK_SDHC_TRANSFER_MODE);
	} else if (cmd->resp_type & MMC_RSP_BUSY) {
		rk_sdhc_writeb(__sdhci, 0xe, RK_SDHC_TIMEOUT_CONTROL);
	}

	rk_sdhc_writel(__sdhci, cmd->cmdarg, RK_SDHC_ARGUMENT);
	rk_sdhc_writew(__sdhci, RK_SDHC_MAKE_CMD(cmd->cmdidx, flags), RK_SDHC_COMMAND);
	start = get_time_ns();
	do {
		stat = rk_sdhc_readl(__sdhci, RK_SDHC_INT_STATUS);
		if (stat & RK_SDHC_INT_ERROR)
			break;

		if (is_timeout(start, RK_SDHC_READ_STATUS_TIMEOUT)) {
			pr_warn("%s: Timeout for status update!\n",
			       __func__);
			return -ETIMEDOUT;
		}
	} while ((stat & mask) != mask);

	if ((stat & (RK_SDHC_INT_ERROR | mask)) == mask) {
		rk_sdhc_cmd_done(__sdhci, cmd);
		rk_sdhc_writel(__sdhci, mask, RK_SDHC_INT_STATUS);
	} else
		ret = -1;

	if (!ret && data)
		ret = rk_sdhc_transfer_data(__sdhci, data);

	udelay(1000);

	stat = rk_sdhc_readl(__sdhci, RK_SDHC_INT_STATUS);
	rk_sdhc_writel(__sdhci, RK_SDHC_INT_ALL_MASK, RK_SDHC_INT_STATUS);
	if (!ret)
		return 0;

	rk_sdhc_reset(__sdhci, RK_SDHC_RESET_CMD);
	rk_sdhc_reset(__sdhci, RK_SDHC_RESET_DATA);
	if (stat & RK_SDHC_INT_TIMEOUT)
		return -ETIMEDOUT;
	else
		return -ECOMM;
}

static int rk_sdhc_set_clock(struct rk_sdhc_host *__sdhci, unsigned int clock)
{
	unsigned int div, clk = 0, timeout;

	/* Wait max 20 ms */
	timeout = 200;
	while (rk_sdhc_readl(__sdhci, RK_SDHC_PRESENT_STATE) &
			   (RK_SDHC_CMD_INHIBIT | RK_SDHC_DATA_INHIBIT)) {
		if (timeout == 0) {
			pr_warn("%s: Timeout to wait cmd & data inhibit\n",
			       __func__);
			return -EBUSY;
		}

		timeout--;
		udelay(100);
	}

	rk_sdhc_writew(__sdhci, 0, RK_SDHC_CLOCK_CONTROL);

	if (clock == 0)
		return 0;

	if (RK_SDHC_GET_VERSION(__sdhci) >= RK_SDHC_SPEC_300) {
		/*
		 * Check if the __sdhci Controller supports Programmable Clock
		 * Mode.
		 */
		if (__sdhci->clk_mul) {
			for (div = 1; div <= 1024; div++) {
				if ((__sdhci->max_clk / div) <= clock)
					break;
			}

			/*
			 * Set Programmable Clock Mode in the Clock
			 * Control register.
			 */
			clk = RK_SDHC_PROG_CLOCK_MODE;
			div--;
		} else {
			/* Version 3.00 divisors must be a multiple of 2. */
			if (__sdhci->max_clk <= clock) {
				div = 1;
			} else {
				for (div = 2;
				     div < RK_SDHC_MAX_DIV_SPEC_300;
				     div += 2) {
					if ((__sdhci->max_clk / div) <= clock)
						break;
				}
			}
			div >>= 1;
		}
	} else {
		/* Version 2.00 divisors must be a power of 2. */
		for (div = 1; div < RK_SDHC_MAX_DIV_SPEC_200; div *= 2) {
			if ((__sdhci->max_clk / div) <= clock)
				break;
		}
		div >>= 1;
	}

	clk |= (div & RK_SDHC_DIV_MASK) << RK_SDHC_DIVIDER_SHIFT;
	clk |= ((div & RK_SDHC_DIV_HI_MASK) >> RK_SDHC_DIV_MASK_LEN)
		<< RK_SDHC_DIVIDER_HI_SHIFT;
	clk |= RK_SDHC_CLOCK_INT_EN;
	rk_sdhc_writew(__sdhci, clk, RK_SDHC_CLOCK_CONTROL);

	/* Wait max 20 ms */
	timeout = 20;
	while (!((clk = rk_sdhc_readw(__sdhci, RK_SDHC_CLOCK_CONTROL))
		& RK_SDHC_CLOCK_INT_STABLE)) {
		if (timeout == 0) {
			pr_warn("%s: Internal clock never stabilised.\n",
			       __func__);
			return -EBUSY;
		}
		timeout--;
		udelay(1000);
	}

	clk |= RK_SDHC_CLOCK_CARD_EN;
	rk_sdhc_writew(__sdhci, clk, RK_SDHC_CLOCK_CONTROL);
	return 0;
}

static void rk_sdhc_set_power(struct rk_sdhc_host *__sdhci, unsigned short power)
{
	u8 pwr = 0;

	if (power != (unsigned short)-1) {
		switch (1 << power) {
		case MMC_VDD_165_195:
			pwr = RK_SDHC_POWER_180;
			break;
		case MMC_VDD_29_30:
		case MMC_VDD_30_31:
			pwr = RK_SDHC_POWER_300;
			break;
		case MMC_VDD_32_33:
		case MMC_VDD_33_34:
			pwr = RK_SDHC_POWER_330;
			break;
		}
	}

	if (pwr == 0) {
		rk_sdhc_writeb(__sdhci, 0, RK_SDHC_POWER_CONTROL);
		return;
	}

	pwr |= RK_SDHC_POWER_ON;

	rk_sdhc_writeb(__sdhci, pwr, RK_SDHC_POWER_CONTROL);
}

static void rk_sdhc_set_uhs_timing(struct rk_sdhc_host *__sdhci,
				 struct mci_ios *ios)
{
	u32 reg;

	reg = rk_sdhc_readw(__sdhci, RK_SDHC_HOST_CONTROL2);
	reg &= ~RK_SDHC_CTRL_UHS_MASK;


	switch (ios->timing) {
	case MMC_TIMING_UHS_DDR50:
	case MMC_TIMING_MMC_DDR52:
		reg |= RK_SDHC_CTRL_UHS_DDR50;
		break;
	case MMC_TIMING_UHS_SDR104:
	case MMC_TIMING_MMC_HS200:
		reg |= RK_SDHC_CTRL_UHS_SDR104;
		break;
	case MMC_TIMING_MMC_HS400:
		reg |= RK_SDHC_CTRL_HS400;
		break;
	default:
		reg |= RK_SDHC_CTRL_UHS_SDR12;
	}

	rk_sdhc_writew(__sdhci, reg, RK_SDHC_HOST_CONTROL2);
}

static void rk3399_emmc_phy_power_on(struct rockchip_emmc_phy *phy, u32 clock)
{
	u32 caldone, dllrdy, freqsel;

	writel(RK_CLRSETBITS(7 << 4, 0), &phy->emmcphy_con[6]);
	writel(RK_CLRSETBITS(1 << 11, 1 << 11), &phy->emmcphy_con[0]);
	writel(RK_CLRSETBITS(0xf << 7, 6 << 7), &phy->emmcphy_con[0]);

	/*
	 * According to the user manual, calpad calibration
	 * cycle takes more than 2us without the minimal recommended
	 * value, so we may need a little margin here
	 */
	udelay(3);
	writel(RK_CLRSETBITS(1, 1), &phy->emmcphy_con[6]);

	/*
	 * According to the user manual, it asks driver to
	 * wait 5us for calpad busy trimming. But it seems that
	 * 5us of caldone isn't enough for all cases.
	 */
	udelay(500);
	caldone = readl(&phy->emmcphy_status);
	caldone = (caldone >> PHYCTRL_CALDONE_SHIFT) & PHYCTRL_CALDONE_MASK;
	if (caldone != PHYCTRL_CALDONE_DONE) {
		pr_warn("%s: caldone timeout.\n", __func__);
		return;
	}

	/* Set the frequency of the DLL operation */
	if (clock < 75 * MHz)
		freqsel = PHYCTRL_FREQSEL_50M;
	else if (clock < 125 * MHz)
		freqsel = PHYCTRL_FREQSEL_100M;
	else if (clock < 175 * MHz)
		freqsel = PHYCTRL_FREQSEL_150M;
	else
		freqsel = PHYCTRL_FREQSEL_200M;

	/* Set the frequency of the DLL operation */
	writel(RK_CLRSETBITS(3 << 12, freqsel << 12), &phy->emmcphy_con[0]);
	writel(RK_CLRSETBITS(1 << 1, 1 << 1), &phy->emmcphy_con[6]);

	/* REN Enable on STRB Line for HS400 */
	writel(RK_CLRSETBITS(0, 1 << 9), &phy->emmcphy_con[2]);

	read_poll_timeout(readl, dllrdy, PHYCTRL_DLL_LOCK_WO_TMOUT(dllrdy),
			  5000, &phy->emmcphy_status);
}

static void rk3399_emmc_phy_power_off(struct rockchip_emmc_phy *phy)
{
	writel(RK_CLRSETBITS(1, 0), &phy->emmcphy_con[6]);
	writel(RK_CLRSETBITS(1 << 1, 0), &phy->emmcphy_con[6]);
}

static void rk3399_rk_sdhc_set_control_reg(struct rk_sdhc_host *__sdhci,
					 struct mci_ios *ios)
{
	struct rockchip_sdhc *priv = container_of(__sdhci, struct rockchip_sdhc, __sdhci);
	uint clock = __sdhci->clock;
	int cycle_phy = clock > EMMC_MIN_FREQ;

	if (cycle_phy)
		rk3399_emmc_phy_power_off(priv->phy);

	rk_sdhc_set_uhs_timing(__sdhci, ios);
};

static void rk3399_rk_sdhc_set_ios_post(struct rk_sdhc_host *__sdhci,
				      struct mci_ios *ios)
{
	struct rockchip_sdhc *priv = container_of(__sdhci, struct rockchip_sdhc, __sdhci);
	uint clock = __sdhci->clock;
	int cycle_phy = clock > EMMC_MIN_FREQ;

	if (!clock)
		clock = ios->clock;

	if (cycle_phy)
		rk3399_emmc_phy_power_on(priv->phy, clock);
}

static void rk_sdhc_set_ios(struct mci_host *mci, struct mci_ios *ios)
{
	struct rk_sdhc_host *__sdhci = to_rk_sdhc_host(mci);
	u32 ctrl;

	rk3399_rk_sdhc_set_control_reg(__sdhci, ios);

	if (ios->clock != __sdhci->clock)
		rk_sdhc_set_clock(__sdhci, ios->clock);

	if (!ios->clock)
		rk_sdhc_set_clock(__sdhci, 0);

	/* Set bus width */
	ctrl = rk_sdhc_readb(__sdhci, RK_SDHC_HOST_CONTROL);
	if (ios->bus_width == MMC_BUS_WIDTH_8) {
		ctrl &= ~RK_SDHC_CTRL_4BITBUS;
		if (RK_SDHC_GET_VERSION(__sdhci) >= RK_SDHC_SPEC_300)
			ctrl |= RK_SDHC_CTRL_8BITBUS;
	} else {
		if (RK_SDHC_GET_VERSION(__sdhci) >= RK_SDHC_SPEC_300)
			ctrl &= ~RK_SDHC_CTRL_8BITBUS;
		if (ios->bus_width == MMC_BUS_WIDTH_4)
			ctrl |= RK_SDHC_CTRL_4BITBUS;
		else
			ctrl &= ~RK_SDHC_CTRL_4BITBUS;
	}

	rk_sdhc_writeb(__sdhci, ctrl, RK_SDHC_HOST_CONTROL);

	/* If available, call the driver specific "post" set_ios() function */
	rk3399_rk_sdhc_set_ios_post(__sdhci, ios);

	__sdhci->clock = ios->clock;
}

static int rk_sdhc_setup_cfg(struct mci_host *mci, struct rk_sdhc_host *__sdhci,
		u32 f_max, u32 f_min)
{
	u32 caps, caps_1 = 0;

	caps = rk_sdhc_readl(__sdhci, RK_SDHC_CAPABILITIES);
	pr_debug("%s, caps: 0x%x\n", __func__, caps);

	__sdhci->version = rk_sdhc_readw(__sdhci, RK_SDHC_HOST_VERSION);

	/* Check whether the clock multiplier is supported or not */
	if (RK_SDHC_GET_VERSION(__sdhci) >= RK_SDHC_SPEC_300) {
		caps_1 = rk_sdhc_readl(__sdhci, RK_SDHC_CAPABILITIES_1);
		pr_debug("%s, caps_1: 0x%x\n", __func__, caps_1);
		__sdhci->clk_mul = (caps_1 & RK_SDHC_CLOCK_MUL_MASK) >>
				RK_SDHC_CLOCK_MUL_SHIFT;
	}

	if (__sdhci->max_clk == 0) {
		if (RK_SDHC_GET_VERSION(__sdhci) >= RK_SDHC_SPEC_300)
			__sdhci->max_clk = (caps & RK_SDHC_CLOCK_V3_BASE_MASK) >>
				RK_SDHC_CLOCK_BASE_SHIFT;
		else
			__sdhci->max_clk = (caps & RK_SDHC_CLOCK_BASE_MASK) >>
				RK_SDHC_CLOCK_BASE_SHIFT;
		__sdhci->max_clk *= 1000000;
		if (__sdhci->clk_mul)
			__sdhci->max_clk *= __sdhci->clk_mul;
	}
	if (__sdhci->max_clk == 0) {
		pr_warn("%s: Hardware doesn't specify base clock frequency\n",
		       __func__);
		return -EINVAL;
	}
	if (f_max && (f_max < __sdhci->max_clk))
		mci->f_max = f_max;
	else
		mci->f_max = __sdhci->max_clk;
	if (f_min)
		mci->f_min = f_min;
	else {
		if (RK_SDHC_GET_VERSION(__sdhci) >= RK_SDHC_SPEC_300)
			mci->f_min = mci->f_max / RK_SDHC_MAX_DIV_SPEC_300;
		else
			mci->f_min = mci->f_max / RK_SDHC_MAX_DIV_SPEC_200;
	}
	mci->voltages = 0;
	if (caps & RK_SDHC_CAN_VDD_330)
		mci->voltages |= MMC_VDD_32_33 | MMC_VDD_33_34;
	if (caps & RK_SDHC_CAN_VDD_300)
		mci->voltages |= MMC_VDD_29_30 | MMC_VDD_30_31;
	if (caps & RK_SDHC_CAN_VDD_180)
		mci->voltages |= MMC_VDD_165_195;

	if (caps & RK_SDHC_CAN_DO_HISPD)
		mci->host_caps |= MMC_CAP_MMC_HIGHSPEED | MMC_CAP_MMC_HIGHSPEED_52MHZ;

	mci->host_caps |= MMC_CAP_4_BIT_DATA;

	/* Since __sdhci Controller Version3.0 */
	if (RK_SDHC_GET_VERSION(__sdhci) >= RK_SDHC_SPEC_300) {
		if (!(caps & RK_SDHC_CAN_DO_8BIT))
			mci->host_caps &= ~MMC_CAP_8_BIT_DATA;
	}

	if (!(mci->voltages & MMC_VDD_165_195))
		caps_1 &= ~(RK_SDHC_SUPPORT_SDR104 | RK_SDHC_SUPPORT_SDR50 |
			    RK_SDHC_SUPPORT_DDR50);

	if (caps_1 & (RK_SDHC_SUPPORT_SDR104 | RK_SDHC_SUPPORT_SDR50 |
		      RK_SDHC_SUPPORT_DDR50))
		mci->host_caps |= MMC_CAP_SD_HIGHSPEED;

	if (caps_1 & RK_SDHC_SUPPORT_SDR104) {
		/* TODO */
	} else if (caps_1 & RK_SDHC_SUPPORT_SDR50) {
		mci->host_caps |= MMC_CAP_SD_HIGHSPEED;
	}

	if (caps_1 & RK_SDHC_SUPPORT_DDR50)
		mci->host_caps |= MMC_CAP_SD_HIGHSPEED;

	if (__sdhci->host_caps)
		mci->host_caps |= __sdhci->host_caps;

	//mci->b_max = 65535; FIXME

	return 0;
}

static int rk_sdhc_init(struct mci_host *mci, struct device *dev)
{
	struct rk_sdhc_host *__sdhci = to_rk_sdhc_host(mci);

	rk_sdhc_reset(__sdhci, RK_SDHC_RESET_ALL);

	rk_sdhc_set_power(__sdhci, fls(mci->voltages) - 1);

	/* Enable only interrupts served by the SD controller */
	rk_sdhc_writel(__sdhci, RK_SDHC_INT_DATA_MASK | RK_SDHC_INT_CMD_MASK,
		     RK_SDHC_INT_ENABLE);
	/* Mask all sdhci interrupt sources */
	rk_sdhc_writel(__sdhci, 0x0, RK_SDHC_SIGNAL_ENABLE);

	return 0;
}

static int rockchip_rk_sdhc_probe(struct device *dev)
{
	struct rockchip_sdhc *prv;
	struct rk_sdhc_host *__sdhci;
	struct resource *iores;
	struct mci_host *mci;
	struct clk *clk;
	int ret;

	prv = xzalloc(sizeof(*prv));

	__sdhci = &prv->__sdhci;

	mci = &__sdhci->mci;
	mci->hw_dev = dev;

	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return PTR_ERR(iores);

	__sdhci->ioaddr = IOMEM(iores->start);

	mci_of_parse(mci);

	__sdhci->max_clk = mci->f_max;
	clk = clk_get(dev, 0);
	// FIXME enable?
	if (IS_ERR(clk)) {
		pr_err("%s fail to get clk\n", __func__);
		return PTR_ERR(clk);
	}

	ret = clk_set_rate(clk, __sdhci->max_clk);
	if (ret) {
		pr_warn("%s clk set rate fail!\n", __func__);
		return ret;
	}

	prv->phy = (struct rockchip_emmc_phy *)0xff77f780;

#if 0
	// TODO
	prv->sdhci.base = IOMEM(iores->start);
	prv->sdhci.mci = mci;
#endif

	mci->send_cmd = rk_sdhc_send_command;
	mci->set_ios = rk_sdhc_set_ios;
	mci->init = rk_sdhc_init;

	ret = rk_sdhc_setup_cfg(mci, __sdhci, mci->f_max, EMMC_MIN_FREQ);
	if (ret)
		return ret;

	return mci_register(mci);
}

static __maybe_unused struct of_device_id arasan_rk_sdhc_compatible[] = {
	{ .compatible = "rockchip,rk3399-sdhci-5.1", },
	{ /* sentinel */ }
};

static struct driver rockchip_arasan_rk_sdhc_driver = {
	.name = "rockchiparasan-sdhci",
	.probe = rockchip_rk_sdhc_probe,
	.of_compatible = DRV_OF_COMPAT(arasan_rk_sdhc_compatible),
};
device_platform_driver(rockchip_arasan_rk_sdhc_driver);
