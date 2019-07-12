/*
 *  linux/drivers/mmc/host/mmci.h - ARM PrimeCell MMCI PL180/1 driver
 *
 *  Copyright (C) 2003 Deep Blue Solutions, Ltd, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define COMMAND_REG_DELAY	300
#define DATA_REG_DELAY		1000
#define CLK_CHANGE_DELAY	2000

#define MMCIPOWER		0x000
#define MCI_PWR_OFF		0x00
#define MCI_PWR_UP		0x02
#define MCI_PWR_ON		0x03
#define MCI_OD			(1 << 6)
#define MCI_ROD			(1 << 7)

/*
 * The STM32 sdmmc does not have PWR_UP/OD/ROD
 * and uses the power register for
 */
#define MCI_STM32_PWR_CYC       0x02
#define MCI_STM32_VSWITCH       BIT(2)
#define MCI_STM32_VSWITCHEN     BIT(3)
#define MCI_STM32_DIRPOL        BIT(4)

#define MMCICLOCK		0x004
#define MCI_CLK_CLKDIV_MASK	0x000000FF
#define MCI_CLK_ENABLE		(1 << 8)
#define MCI_CLK_PWRSAVE		(1 << 9)
#define MCI_CLK_BYPASS		(1 << 10)
#define MCI_xBIT_BUS_MASK	0x00001800
#define MCI_1BIT_BUS		(0 << 0)
#define MCI_4BIT_BUS		(1 << 11)
/*
 * 8bit wide buses, hardware flow contronl, negative edges and clock inversion
 * supported in ST Micro U300 and Ux500 versions
 */
#define MCI_ST_8BIT_BUS		(1 << 12)
#define MCI_ST_U300_HWFCEN	(1 << 13)
#define MCI_ST_UX500_NEG_EDGE	(1 << 13)
#define MCI_ST_UX500_HWFCEN	(1 << 14)
#define MCI_ST_UX500_CLK_INV	(1 << 15)

/* Modified on STM32 sdmmc */
#define MCI_STM32_CLK_CLKDIV_MSK        GENMASK(9, 0)
#define MCI_STM32_CLK_WIDEBUS_4         BIT(14)
#define MCI_STM32_CLK_WIDEBUS_8         BIT(15)
#define MCI_STM32_CLK_NEGEDGE           BIT(16)
#define MCI_STM32_CLK_HWFCEN            BIT(17)
#define MCI_STM32_CLK_DDR               BIT(18)
#define MCI_STM32_CLK_BUSSPEED          BIT(19)
#define MCI_STM32_CLK_SEL_MSK           GENMASK(21, 20)
#define MCI_STM32_CLK_SELCK             (0 << 20)
#define MCI_STM32_CLK_SELCKIN           (1 << 20)
#define MCI_STM32_CLK_SELFBCK           (2 << 20)

#define MMCIARGUMENT		0x008
#define MMCICOMMAND		0x00c
#define MCI_CPSM_RESPONSE	(1 << 6)
#define MCI_CPSM_LONGRSP	(1 << 7)
#define MCI_CPSM_INTERRUPT	(1 << 8)
#define MCI_CPSM_PENDING	(1 << 9)
#define MCI_CPSM_ENABLE		(1 << 10)
#define MCI_SDIO_SUSP		(1 << 11)
#define MCI_ENCMD_COMPL		(1 << 12)
#define MCI_NIEN		(1 << 13)
#define MCI_CE_ATACMD		(1 << 14)

/* Command register in STM32 sdmmc versions */
#define MCI_CPSM_STM32_CMDTRANS         BIT(6)
#define MCI_CPSM_STM32_CMDSTOP          BIT(7)
#define MCI_CPSM_STM32_WAITRESP_MASK    GENMASK(9, 8)
#define MCI_CPSM_STM32_NORSP            (0 << 8)
#define MCI_CPSM_STM32_SRSP_CRC         (1 << 8)
#define MCI_CPSM_STM32_SRSP             (2 << 8)
#define MCI_CPSM_STM32_LRSP_CRC         (3 << 8)
#define MCI_CPSM_STM32_ENABLE           BIT(12)

#define MMCIRESPCMD		0x010
#define MMCIRESPONSE0		0x014
#define MMCIRESPONSE1		0x018
#define MMCIRESPONSE2		0x01c
#define MMCIRESPONSE3		0x020
#define MMCIDATATIMER		0x024
#define MMCIDATALENGTH		0x028
#define MMCIDATACTRL		0x02c
#define MCI_DPSM_ENABLE		(1 << 0)
#define MCI_DPSM_DIRECTION	(1 << 1)
#define MCI_DPSM_MODE		(1 << 2)
#define MCI_DPSM_DMAENABLE	(1 << 3)
#define MCI_DPSM_BLOCKSIZE	(1 << 4)
/* Control register extensions in the ST Micro U300 and Ux500 versions */
#define MCI_ST_DPSM_RWSTART	(1 << 8)
#define MCI_ST_DPSM_RWSTOP	(1 << 9)
#define MCI_ST_DPSM_RWMOD	(1 << 10)
#define MCI_ST_DPSM_SDIOEN	(1 << 11)
/* Control register extensions in the ST Micro Ux500 versions */
#define MCI_ST_DPSM_DMAREQCTL	(1 << 12)
#define MCI_ST_DPSM_DBOOTMODEEN	(1 << 13)
#define MCI_ST_DPSM_BUSYMODE	(1 << 14)
#define MCI_ST_DPSM_DDRMODE	(1 << 15)

#define MCI_DTIMER_DEFAULT	0xFFFF0000

/* Control register extensions in STM32 versions */
#define MCI_DPSM_STM32_MODE_BLOCK       (0 << 2)
#define MCI_DPSM_STM32_MODE_SDIO        (1 << 2)
#define MCI_DPSM_STM32_MODE_STREAM      (2 << 2)
#define MCI_DPSM_STM32_MODE_BLOCK_STOP  (3 << 2)

#define MMCIDATACNT		0x030
#define MMCISTATUS		0x034
#define MCI_CMDCRCFAIL		(1 << 0)
#define MCI_DATACRCFAIL		(1 << 1)
#define MCI_CMDTIMEOUT		(1 << 2)
#define MCI_DATATIMEOUT		(1 << 3)
#define MCI_TXUNDERRUN		(1 << 4)
#define MCI_RXOVERRUN		(1 << 5)
#define MCI_CMDRESPEND		(1 << 6)
#define MCI_CMDSENT		(1 << 7)
#define MCI_DATAEND		(1 << 8)
#define MCI_STARTBITERR		(1 << 9)
#define MCI_DATABLOCKEND	(1 << 10)
#define MCI_CMDACTIVE		(1 << 11)
#define MCI_TXACTIVE		(1 << 12)
#define MCI_RXACTIVE		(1 << 13)
#define MCI_TXFIFOHALFEMPTY	(1 << 14)
#define MCI_RXFIFOHALFFULL	(1 << 15)
#define MCI_TXFIFOFULL		(1 << 16)
#define MCI_RXFIFOFULL		(1 << 17)
#define MCI_TXFIFOEMPTY		(1 << 18)
#define MCI_RXFIFOEMPTY		(1 << 19)
#define MCI_TXDATAAVLBL		(1 << 20)
#define MCI_RXDATAAVLBL		(1 << 21)
/* Extended status bits for the ST Micro variants */
#define MCI_ST_SDIOIT		(1 << 22)
#define MCI_ST_CEATAEND		(1 << 23)

#define MMCICLEAR		0x038
#define MCI_CMDCRCFAILCLR	(1 << 0)
#define MCI_DATACRCFAILCLR	(1 << 1)
#define MCI_CMDTIMEOUTCLR	(1 << 2)
#define MCI_DATATIMEOUTCLR	(1 << 3)
#define MCI_TXUNDERRUNCLR	(1 << 4)
#define MCI_RXOVERRUNCLR	(1 << 5)
#define MCI_CMDRESPENDCLR	(1 << 6)
#define MCI_CMDSENTCLR		(1 << 7)
#define MCI_DATAENDCLR		(1 << 8)
#define MCI_STARTBITERRCLR	(1 << 9)
#define MCI_DATABLOCKENDCLR	(1 << 10)
/* Extended status bits for the ST Micro variants */
#define MCI_ST_SDIOITC		(1 << 22)
#define MCI_ST_CEATAENDC	(1 << 23)

#define MMCIMASK0		0x03c
#define MCI_MASK0_MASK		0x1FFFFFFF
#define MCI_CMDINDEXMASK	0xFF
#define MCI_ICR_MASK		0x1DC007FF

#define MCI_CMDCRCFAILMASK	(1 << 0)
#define MCI_DATACRCFAILMASK	(1 << 1)
#define MCI_CMDTIMEOUTMASK	(1 << 2)
#define MCI_DATATIMEOUTMASK	(1 << 3)
#define MCI_TXUNDERRUNMASK	(1 << 4)
#define MCI_RXOVERRUNMASK	(1 << 5)
#define MCI_CMDRESPENDMASK	(1 << 6)
#define MCI_CMDSENTMASK		(1 << 7)
#define MCI_DATAENDMASK		(1 << 8)
#define MCI_STARTBITERRMASK	(1 << 9)
#define MCI_DATABLOCKENDMASK	(1 << 10)
#define MCI_CMDACTIVEMASK	(1 << 11)
#define MCI_TXACTIVEMASK	(1 << 12)
#define MCI_RXACTIVEMASK	(1 << 13)
#define MCI_TXFIFOHALFEMPTYMASK	(1 << 14)
#define MCI_RXFIFOHALFFULLMASK	(1 << 15)
#define MCI_TXFIFOFULLMASK	(1 << 16)
#define MCI_RXFIFOFULLMASK	(1 << 17)
#define MCI_TXFIFOEMPTYMASK	(1 << 18)
#define MCI_RXFIFOEMPTYMASK	(1 << 19)
#define MCI_TXDATAAVLBLMASK	(1 << 20)
#define MCI_RXDATAAVLBLMASK	(1 << 21)
/* Extended status bits for the ST Micro variants */
#define MCI_ST_SDIOITMASK	(1 << 22)
#define MCI_ST_CEATAENDMASK	(1 << 23)

#define MMCIMASK1		0x040
#define MMCIFIFOCNT		0x048
#define MMCIFIFO		0x080 /* to 0x0bc */

#define MCI_IRQENABLE	\
	(MCI_CMDCRCFAILMASK|MCI_DATACRCFAILMASK|MCI_CMDTIMEOUTMASK|	\
	MCI_DATATIMEOUTMASK|MCI_TXUNDERRUNMASK|MCI_RXOVERRUNMASK|	\
	MCI_CMDRESPENDMASK|MCI_CMDSENTMASK|MCI_STARTBITERRMASK)

/* These interrupts are directed to IRQ1 when two IRQ lines are available */
#define MCI_IRQ1MASK \
	(MCI_RXFIFOHALFFULLMASK | MCI_RXDATAAVLBLMASK | \
	 MCI_TXFIFOHALFEMPTYMASK)

#define NR_SG		128

struct mmci_host {
	struct mci_host		mci;
	void __iomem		*base;
	struct device_d		*hw_dev;
	struct mmci_platform_data *plat;
	struct clk		*clk;
	unsigned long		mclk;

	int			hw_revision;
	int			hw_designer;
	struct variant_data	*variant;
	struct mmci_host_ops	*ops;
	struct reset_control	*rst;
	u32			clk_reg;
	u32			pwr_reg;
};

struct mci_ios;

/* mmci variant callbacks */
struct mmci_host_ops {
        u32 (*get_datactrl_cfg)(struct mmci_host *host, unsigned int blocksize);
        void (*set_clkreg)(struct mmci_host *host, struct mci_ios *ios);
        void (*set_pwrreg)(struct mmci_host *host, unsigned int pwr);
};

void mmci_write_clkreg(struct mmci_host *host, u32 clk);
void mmci_write_pwrreg(struct mmci_host *host, u32 pwr);

static inline u32 mmci_dctrl_blksz(struct mmci_host *host, unsigned int blocksize)
{
        return (ffs(blocksize) - 1) << 4;
}

#ifdef CONFIG_MCI_MMCI_SDMMC
void sdmmc_variant_init(struct mmci_host *host);
#else
static inline void sdmmc_variant_init(struct mmci_host *host)
{
}
#endif
