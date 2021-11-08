// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2016 Rockchip Electronics Co., Ltd
 */

#include <common.h>
#include <io.h>
#include <mach/rockchip/hardware.h>
#include <mach/rockchip/bootrom.h>
#include <mach/rockchip/rk3399-regs.h>
#include <mach/rockchip/grf_rk3399.h>
#include <mach/rockchip/stimer.h>
#include <mach/rockchip/rockchip.h>

#define assert(cond) BUG_ON(!(cond))

#define GRF_BASE		IOMEM(0xff770000)
#define PMUSGRF_BASE		IOMEM(0xff330000)

#define KHz		1000
#define MHz		1000000

#define OSC_HZ		(24*MHz)
#define LPLL_HZ		(600*MHz)
#define BPLL_HZ		(600*MHz)
#define GPLL_HZ		(594*MHz)
#define CPLL_HZ		(384*MHz)
#define PPLL_HZ		(676*MHz)

#define PMU_PCLK_HZ	(48*MHz)

#define ACLKM_CORE_L_HZ	(300*MHz)
#define ATCLK_CORE_L_HZ	(300*MHz)
#define PCLK_DBG_L_HZ	(100*MHz)

#define ACLKM_CORE_B_HZ	(300*MHz)
#define ATCLK_CORE_B_HZ	(300*MHz)
#define PCLK_DBG_B_HZ	(100*MHz)

#define PERIHP_ACLK_HZ	(148500*KHz)
#define PERIHP_HCLK_HZ	(148500*KHz)
#define PERIHP_PCLK_HZ	(37125*KHz)

#define PERILP0_ACLK_HZ	(99000*KHz)
#define PERILP0_HCLK_HZ	(99000*KHz)
#define PERILP0_PCLK_HZ	(49500*KHz)

#define PERILP1_HCLK_HZ	(99000*KHz)
#define PERILP1_PCLK_HZ	(49500*KHz)

#define PWM_CLOCK_HZ    PMU_PCLK_HZ

enum apll_l_frequencies {
	APLL_L_1600_MHZ,
	APLL_L_600_MHZ,
};

enum apll_b_frequencies {
	APLL_B_600_MHZ,
};


struct pll_div {
	u32 refdiv;
	u32 fbdiv;
	u32 postdiv1;
	u32 postdiv2;
	u32 frac;
};

#define RATE_TO_DIV(input_rate, output_rate) \
	((input_rate) / (output_rate) - 1)
#define DIV_TO_RATE(input_rate, div)		((input_rate) / ((div) + 1))

#define PLL_DIVISORS(hz, _refdiv, _postdiv1, _postdiv2) {\
	.refdiv = _refdiv,\
	.fbdiv = (u32)((u64)hz * _refdiv * _postdiv1 * _postdiv2 / OSC_HZ),\
	.postdiv1 = _postdiv1, .postdiv2 = _postdiv2};

static const struct pll_div gpll_init_cfg = PLL_DIVISORS(GPLL_HZ, 2, 2, 1);
static const struct pll_div cpll_init_cfg = PLL_DIVISORS(CPLL_HZ, 1, 2, 2);
static const struct pll_div ppll_init_cfg = PLL_DIVISORS(PPLL_HZ, 2, 2, 1);

static const struct pll_div apll_l_1600_cfg = PLL_DIVISORS(1600 * MHz, 3, 1, 1);
static const struct pll_div apll_l_600_cfg = PLL_DIVISORS(600 * MHz, 1, 2, 1);

static const struct pll_div *apll_l_cfgs[] = {
	[APLL_L_1600_MHZ] = &apll_l_1600_cfg,
	[APLL_L_600_MHZ] = &apll_l_600_cfg,
};

static const struct pll_div apll_b_600_cfg = PLL_DIVISORS(600 * MHz, 1, 2, 1);
static const struct pll_div *apll_b_cfgs[] = {
	[APLL_B_600_MHZ] = &apll_b_600_cfg,
};

enum {
	/* PLL_CON0 */
	PLL_FBDIV_MASK			= 0xfff,
	PLL_FBDIV_SHIFT			= 0,

	/* PLL_CON1 */
	PLL_POSTDIV2_SHIFT		= 12,
	PLL_POSTDIV2_MASK		= 0x7 << PLL_POSTDIV2_SHIFT,
	PLL_POSTDIV1_SHIFT		= 8,
	PLL_POSTDIV1_MASK		= 0x7 << PLL_POSTDIV1_SHIFT,
	PLL_REFDIV_MASK			= 0x3f,
	PLL_REFDIV_SHIFT		= 0,

	/* PLL_CON2 */
	PLL_LOCK_STATUS_SHIFT		= 31,
	PLL_LOCK_STATUS_MASK		= 1 << PLL_LOCK_STATUS_SHIFT,
	PLL_FRACDIV_MASK		= 0xffffff,
	PLL_FRACDIV_SHIFT		= 0,

	/* PLL_CON3 */
	PLL_MODE_SHIFT			= 8,
	PLL_MODE_MASK			= 3 << PLL_MODE_SHIFT,
	PLL_MODE_SLOW			= 0,
	PLL_MODE_NORM,
	PLL_MODE_DEEP,
	PLL_DSMPD_SHIFT			= 3,
	PLL_DSMPD_MASK			= 1 << PLL_DSMPD_SHIFT,
	PLL_INTEGER_MODE		= 1,

	/* PMUCRU_CLKSEL_CON0 */
	PMU_PCLK_DIV_CON_MASK		= 0x1f,
	PMU_PCLK_DIV_CON_SHIFT		= 0,

	/* PMUCRU_CLKSEL_CON1 */
	SPI3_PLL_SEL_SHIFT		= 7,
	SPI3_PLL_SEL_MASK		= 1 << SPI3_PLL_SEL_SHIFT,
	SPI3_PLL_SEL_24M		= 0,
	SPI3_PLL_SEL_PPLL		= 1,
	SPI3_DIV_CON_SHIFT		= 0x0,
	SPI3_DIV_CON_MASK		= 0x7f,

	/* PMUCRU_CLKSEL_CON2 */
	I2C_DIV_CON_MASK		= 0x7f,
	CLK_I2C8_DIV_CON_SHIFT		= 8,
	CLK_I2C0_DIV_CON_SHIFT		= 0,

	/* PMUCRU_CLKSEL_CON3 */
	CLK_I2C4_DIV_CON_SHIFT		= 0,

	/* CLKSEL_CON0 */
	ACLKM_CORE_L_DIV_CON_SHIFT	= 8,
	ACLKM_CORE_L_DIV_CON_MASK	= 0x1f << ACLKM_CORE_L_DIV_CON_SHIFT,
	CLK_CORE_L_PLL_SEL_SHIFT	= 6,
	CLK_CORE_L_PLL_SEL_MASK		= 3 << CLK_CORE_L_PLL_SEL_SHIFT,
	CLK_CORE_L_PLL_SEL_ALPLL	= 0x0,
	CLK_CORE_L_PLL_SEL_ABPLL	= 0x1,
	CLK_CORE_L_PLL_SEL_DPLL		= 0x10,
	CLK_CORE_L_PLL_SEL_GPLL		= 0x11,
	CLK_CORE_L_DIV_MASK		= 0x1f,
	CLK_CORE_L_DIV_SHIFT		= 0,

	/* CLKSEL_CON1 */
	PCLK_DBG_L_DIV_SHIFT		= 0x8,
	PCLK_DBG_L_DIV_MASK		= 0x1f << PCLK_DBG_L_DIV_SHIFT,
	ATCLK_CORE_L_DIV_SHIFT		= 0,
	ATCLK_CORE_L_DIV_MASK		= 0x1f << ATCLK_CORE_L_DIV_SHIFT,

	/* CLKSEL_CON2 */
	ACLKM_CORE_B_DIV_CON_SHIFT	= 8,
	ACLKM_CORE_B_DIV_CON_MASK	= 0x1f << ACLKM_CORE_B_DIV_CON_SHIFT,
	CLK_CORE_B_PLL_SEL_SHIFT	= 6,
	CLK_CORE_B_PLL_SEL_MASK		= 3 << CLK_CORE_B_PLL_SEL_SHIFT,
	CLK_CORE_B_PLL_SEL_ALPLL	= 0x0,
	CLK_CORE_B_PLL_SEL_ABPLL	= 0x1,
	CLK_CORE_B_PLL_SEL_DPLL		= 0x10,
	CLK_CORE_B_PLL_SEL_GPLL		= 0x11,
	CLK_CORE_B_DIV_MASK		= 0x1f,
	CLK_CORE_B_DIV_SHIFT		= 0,

	/* CLKSEL_CON3 */
	PCLK_DBG_B_DIV_SHIFT		= 0x8,
	PCLK_DBG_B_DIV_MASK		= 0x1f << PCLK_DBG_B_DIV_SHIFT,
	ATCLK_CORE_B_DIV_SHIFT		= 0,
	ATCLK_CORE_B_DIV_MASK		= 0x1f << ATCLK_CORE_B_DIV_SHIFT,

	/* CLKSEL_CON14 */
	PCLK_PERIHP_DIV_CON_SHIFT	= 12,
	PCLK_PERIHP_DIV_CON_MASK	= 0x7 << PCLK_PERIHP_DIV_CON_SHIFT,
	HCLK_PERIHP_DIV_CON_SHIFT	= 8,
	HCLK_PERIHP_DIV_CON_MASK	= 3 << HCLK_PERIHP_DIV_CON_SHIFT,
	ACLK_PERIHP_PLL_SEL_SHIFT	= 7,
	ACLK_PERIHP_PLL_SEL_MASK	= 1 << ACLK_PERIHP_PLL_SEL_SHIFT,
	ACLK_PERIHP_PLL_SEL_CPLL	= 0,
	ACLK_PERIHP_PLL_SEL_GPLL	= 1,
	ACLK_PERIHP_DIV_CON_SHIFT	= 0,
	ACLK_PERIHP_DIV_CON_MASK	= 0x1f,

	/* CLKSEL_CON21 */
	ACLK_EMMC_PLL_SEL_SHIFT         = 7,
	ACLK_EMMC_PLL_SEL_MASK          = 0x1 << ACLK_EMMC_PLL_SEL_SHIFT,
	ACLK_EMMC_PLL_SEL_GPLL          = 0x1,
	ACLK_EMMC_DIV_CON_SHIFT         = 0,
	ACLK_EMMC_DIV_CON_MASK          = 0x1f,

	/* CLKSEL_CON22 */
	CLK_EMMC_PLL_SHIFT              = 8,
	CLK_EMMC_PLL_MASK               = 0x7 << CLK_EMMC_PLL_SHIFT,
	CLK_EMMC_PLL_SEL_GPLL           = 0x1,
	CLK_EMMC_PLL_SEL_24M            = 0x5,
	CLK_EMMC_DIV_CON_SHIFT          = 0,
	CLK_EMMC_DIV_CON_MASK           = 0x7f << CLK_EMMC_DIV_CON_SHIFT,

	/* CLKSEL_CON23 */
	PCLK_PERILP0_DIV_CON_SHIFT	= 12,
	PCLK_PERILP0_DIV_CON_MASK	= 0x7 << PCLK_PERILP0_DIV_CON_SHIFT,
	HCLK_PERILP0_DIV_CON_SHIFT	= 8,
	HCLK_PERILP0_DIV_CON_MASK	= 3 << HCLK_PERILP0_DIV_CON_SHIFT,
	ACLK_PERILP0_PLL_SEL_SHIFT	= 7,
	ACLK_PERILP0_PLL_SEL_MASK	= 1 << ACLK_PERILP0_PLL_SEL_SHIFT,
	ACLK_PERILP0_PLL_SEL_CPLL	= 0,
	ACLK_PERILP0_PLL_SEL_GPLL	= 1,
	ACLK_PERILP0_DIV_CON_SHIFT	= 0,
	ACLK_PERILP0_DIV_CON_MASK	= 0x1f,

	/* CLKSEL_CON25 */
	PCLK_PERILP1_DIV_CON_SHIFT	= 8,
	PCLK_PERILP1_DIV_CON_MASK	= 0x7 << PCLK_PERILP1_DIV_CON_SHIFT,
	HCLK_PERILP1_PLL_SEL_SHIFT	= 7,
	HCLK_PERILP1_PLL_SEL_MASK	= 1 << HCLK_PERILP1_PLL_SEL_SHIFT,
	HCLK_PERILP1_PLL_SEL_CPLL	= 0,
	HCLK_PERILP1_PLL_SEL_GPLL	= 1,
	HCLK_PERILP1_DIV_CON_SHIFT	= 0,
	HCLK_PERILP1_DIV_CON_MASK	= 0x1f,

	/* CLKSEL_CON26 */
	CLK_SARADC_DIV_CON_SHIFT	= 8,
	CLK_SARADC_DIV_CON_MASK		= GENMASK(15, 8),
	CLK_SARADC_DIV_CON_WIDTH	= 8,

	/* CLKSEL_CON27 */
	CLK_TSADC_SEL_X24M		= 0x0,
	CLK_TSADC_SEL_SHIFT		= 15,
	CLK_TSADC_SEL_MASK		= 1 << CLK_TSADC_SEL_SHIFT,
	CLK_TSADC_DIV_CON_SHIFT		= 0,
	CLK_TSADC_DIV_CON_MASK		= 0x3ff,

	/* CLKSEL_CON47 & CLKSEL_CON48 */
	ACLK_VOP_PLL_SEL_SHIFT		= 6,
	ACLK_VOP_PLL_SEL_MASK		= 0x3 << ACLK_VOP_PLL_SEL_SHIFT,
	ACLK_VOP_PLL_SEL_CPLL		= 0x1,
	ACLK_VOP_DIV_CON_SHIFT		= 0,
	ACLK_VOP_DIV_CON_MASK		= 0x1f << ACLK_VOP_DIV_CON_SHIFT,

	/* CLKSEL_CON49 & CLKSEL_CON50 */
	DCLK_VOP_DCLK_SEL_SHIFT         = 11,
	DCLK_VOP_DCLK_SEL_MASK          = 1 << DCLK_VOP_DCLK_SEL_SHIFT,
	DCLK_VOP_DCLK_SEL_DIVOUT        = 0,
	DCLK_VOP_PLL_SEL_SHIFT          = 8,
	DCLK_VOP_PLL_SEL_MASK           = 3 << DCLK_VOP_PLL_SEL_SHIFT,
	DCLK_VOP_PLL_SEL_VPLL           = 0,
	DCLK_VOP_DIV_CON_MASK           = 0xff,
	DCLK_VOP_DIV_CON_SHIFT          = 0,

	/* CLKSEL_CON57 */
	PCLK_ALIVE_DIV_CON_SHIFT        = 0,
	PCLK_ALIVE_DIV_CON_MASK         = 0x1f << PCLK_ALIVE_DIV_CON_SHIFT,

	/* CLKSEL_CON58 */
	CLK_SPI_PLL_SEL_WIDTH = 1,
	CLK_SPI_PLL_SEL_MASK = ((1 < CLK_SPI_PLL_SEL_WIDTH) - 1),
	CLK_SPI_PLL_SEL_CPLL = 0,
	CLK_SPI_PLL_SEL_GPLL = 1,
	CLK_SPI_PLL_DIV_CON_WIDTH = 7,
	CLK_SPI_PLL_DIV_CON_MASK = ((1 << CLK_SPI_PLL_DIV_CON_WIDTH) - 1),

	CLK_SPI5_PLL_DIV_CON_SHIFT      = 8,
	CLK_SPI5_PLL_SEL_SHIFT	        = 15,

	/* CLKSEL_CON59 */
	CLK_SPI1_PLL_SEL_SHIFT		= 15,
	CLK_SPI1_PLL_DIV_CON_SHIFT	= 8,
	CLK_SPI0_PLL_SEL_SHIFT		= 7,
	CLK_SPI0_PLL_DIV_CON_SHIFT	= 0,

	/* CLKSEL_CON60 */
	CLK_SPI4_PLL_SEL_SHIFT		= 15,
	CLK_SPI4_PLL_DIV_CON_SHIFT	= 8,
	CLK_SPI2_PLL_SEL_SHIFT		= 7,
	CLK_SPI2_PLL_DIV_CON_SHIFT	= 0,

	/* CLKSEL_CON61 */
	CLK_I2C_PLL_SEL_MASK		= 1,
	CLK_I2C_PLL_SEL_CPLL		= 0,
	CLK_I2C_PLL_SEL_GPLL		= 1,
	CLK_I2C5_PLL_SEL_SHIFT		= 15,
	CLK_I2C5_DIV_CON_SHIFT		= 8,
	CLK_I2C1_PLL_SEL_SHIFT		= 7,
	CLK_I2C1_DIV_CON_SHIFT		= 0,

	/* CLKSEL_CON62 */
	CLK_I2C6_PLL_SEL_SHIFT		= 15,
	CLK_I2C6_DIV_CON_SHIFT		= 8,
	CLK_I2C2_PLL_SEL_SHIFT		= 7,
	CLK_I2C2_DIV_CON_SHIFT		= 0,

	/* CLKSEL_CON63 */
	CLK_I2C7_PLL_SEL_SHIFT		= 15,
	CLK_I2C7_DIV_CON_SHIFT		= 8,
	CLK_I2C3_PLL_SEL_SHIFT		= 7,
	CLK_I2C3_DIV_CON_SHIFT		= 0,

	/* CRU_SOFTRST_CON4 */
	RESETN_DDR0_REQ_SHIFT		= 8,
	RESETN_DDR0_REQ_MASK		= 1 << RESETN_DDR0_REQ_SHIFT,
	RESETN_DDRPHY0_REQ_SHIFT	= 9,
	RESETN_DDRPHY0_REQ_MASK		= 1 << RESETN_DDRPHY0_REQ_SHIFT,
	RESETN_DDR1_REQ_SHIFT		= 12,
	RESETN_DDR1_REQ_MASK		= 1 << RESETN_DDR1_REQ_SHIFT,
	RESETN_DDRPHY1_REQ_SHIFT	= 13,
	RESETN_DDRPHY1_REQ_MASK		= 1 << RESETN_DDRPHY1_REQ_SHIFT,
};

#define VCO_MAX_KHZ	(3200 * (MHz / KHz))
#define VCO_MIN_KHZ	(800 * (MHz / KHz))
#define OUTPUT_MAX_KHZ	(3200 * (MHz / KHz))
#define OUTPUT_MIN_KHZ	(16 * (MHz / KHz))

/*
 *  the div restructions of pll in integer mode, these are defined in
 *  * CRU_*PLL_CON0 or PMUCRU_*PLL_CON0
 */
#define PLL_DIV_MIN	16
#define PLL_DIV_MAX	3200

struct rockchip_cru {
	u32 apll_l_con[6];
	u32 reserved[2];
	u32 apll_b_con[6];
	u32 reserved1[2];
	u32 dpll_con[6];
	u32 reserved2[2];
	u32 cpll_con[6];
	u32 reserved3[2];
	u32 gpll_con[6];
	u32 reserved4[2];
	u32 npll_con[6];
	u32 reserved5[2];
	u32 vpll_con[6];
	u32 reserved6[0x0a];
	u32 clksel_con[108];
	u32 reserved7[0x14];
	u32 clkgate_con[35];
	u32 reserved8[0x1d];
	u32 softrst_con[21];
	u32 reserved9[0x2b];
	u32 glb_srst_fst_value;
	u32 glb_srst_snd_value;
	u32 glb_cnt_th;
	u32 misc_con;
	u32 glb_rst_con;
	u32 glb_rst_st;
	u32 reserved10[0x1a];
	u32 sdmmc_con[2];
	u32 sdio0_con[2];
	u32 sdio1_con[2];
};
static_assert(sizeof(struct rockchip_cru) == 0x598);

struct rk3399_pmucru {
	u32 ppll_con[6];
	u32 reserved[0x1a];
	u32 pmucru_clksel[6];
	u32 pmucru_clkfrac_con[2];
	u32 reserved2[0x18];
	u32 pmucru_clkgate_con[3];
	u32 reserved3;
	u32 pmucru_softrst_con[2];
	u32 reserved4[2];
	u32 pmucru_rstnhold_con[2];
	u32 reserved5[2];
	u32 pmucru_gatedis_con[2];
};
static_assert(sizeof(struct rk3399_pmucru) == 0x138);

/*
 * How to calculate the PLL(from TRM V0.3 Part 1 Page 63):
 * Formulas also embedded within the Fractional PLL Verilog model:
 * If DSMPD = 1 (DSM is disabled, "integer mode")
 * FOUTVCO = FREF / REFDIV * FBDIV
 * FOUTPOSTDIV = FOUTVCO / POSTDIV1 / POSTDIV2
 * Where:
 * FOUTVCO = Fractional PLL non-divided output frequency
 * FOUTPOSTDIV = Fractional PLL divided output frequency
 *               (output of second post divider)
 * FREF = Fractional PLL input reference frequency, (the OSC_HZ 24MHz input)
 * REFDIV = Fractional PLL input reference clock divider
 * FBDIV = Integer value programmed into feedback divide
 *
 */
static void rkclk_set_pll(u32 *pll_con, const struct pll_div *div)
{
	/* All 8 PLLs have same VCO and output frequency range restrictions. */
	u32 vco_khz = OSC_HZ / 1000 * div->fbdiv / div->refdiv;
	u32 output_khz = vco_khz / div->postdiv1 / div->postdiv2;

	pr_debug("PLL at %p: fbdiv=%d, refdiv=%d, postdiv1=%d, "
			   "postdiv2=%d, vco=%u khz, output=%u khz\n",
			   pll_con, div->fbdiv, div->refdiv, div->postdiv1,
			   div->postdiv2, vco_khz, output_khz);
	assert(vco_khz >= VCO_MIN_KHZ && vco_khz <= VCO_MAX_KHZ &&
	       output_khz >= OUTPUT_MIN_KHZ && output_khz <= OUTPUT_MAX_KHZ &&
	       div->fbdiv >= PLL_DIV_MIN && div->fbdiv <= PLL_DIV_MAX);

	/*
	 * When power on or changing PLL setting,
	 * we must force PLL into slow mode to ensure output stable clock.
	 */
	rk_clrsetreg(&pll_con[3], PLL_MODE_MASK,
		     PLL_MODE_SLOW << PLL_MODE_SHIFT);

	/* use integer mode */
	rk_clrsetreg(&pll_con[3], PLL_DSMPD_MASK,
		     PLL_INTEGER_MODE << PLL_DSMPD_SHIFT);

	rk_clrsetreg(&pll_con[0], PLL_FBDIV_MASK,
		     div->fbdiv << PLL_FBDIV_SHIFT);
	rk_clrsetreg(&pll_con[1],
		     PLL_POSTDIV2_MASK | PLL_POSTDIV1_MASK |
		     PLL_REFDIV_MASK | PLL_REFDIV_SHIFT,
		     (div->postdiv2 << PLL_POSTDIV2_SHIFT) |
		     (div->postdiv1 << PLL_POSTDIV1_SHIFT) |
		     (div->refdiv << PLL_REFDIV_SHIFT));

	/* waiting for pll lock */
	while (!(readl(&pll_con[2]) & (1 << PLL_LOCK_STATUS_SHIFT)))
		udelay(1);

	/* pll enter normal mode */
	rk_clrsetreg(&pll_con[3], PLL_MODE_MASK,
		     PLL_MODE_NORM << PLL_MODE_SHIFT);
}

static void rk3399_configure_cpu_b(struct rockchip_cru *cru,
			    enum apll_b_frequencies apll_b_freq)
{
	u32 aclkm_div;
	u32 pclk_dbg_div;
	u32 atclk_div;

	/* Setup cluster B */
	rkclk_set_pll(&cru->apll_b_con[0], apll_b_cfgs[apll_b_freq]);

	aclkm_div = BPLL_HZ / ACLKM_CORE_B_HZ - 1;
	assert((aclkm_div + 1) * ACLKM_CORE_B_HZ == BPLL_HZ &&
	       aclkm_div < 0x1f);

	pclk_dbg_div = BPLL_HZ / PCLK_DBG_B_HZ - 1;
	assert((pclk_dbg_div + 1) * PCLK_DBG_B_HZ == BPLL_HZ &&
	       pclk_dbg_div < 0x1f);

	atclk_div = BPLL_HZ / ATCLK_CORE_B_HZ - 1;
	assert((atclk_div + 1) * ATCLK_CORE_B_HZ == BPLL_HZ &&
	       atclk_div < 0x1f);

	rk_clrsetreg(&cru->clksel_con[2],
		     ACLKM_CORE_B_DIV_CON_MASK | CLK_CORE_B_PLL_SEL_MASK |
		     CLK_CORE_B_DIV_MASK,
		     aclkm_div << ACLKM_CORE_B_DIV_CON_SHIFT |
		     CLK_CORE_B_PLL_SEL_ABPLL << CLK_CORE_B_PLL_SEL_SHIFT |
		     0 << CLK_CORE_B_DIV_SHIFT);

	rk_clrsetreg(&cru->clksel_con[3],
		     PCLK_DBG_B_DIV_MASK | ATCLK_CORE_B_DIV_MASK,
		     pclk_dbg_div << PCLK_DBG_B_DIV_SHIFT |
		     atclk_div << ATCLK_CORE_B_DIV_SHIFT);
}

static void rk3399_configure_cpu_l(struct rockchip_cru *cru,
			    enum apll_l_frequencies apll_l_freq)
{
	u32 aclkm_div;
	u32 pclk_dbg_div;
	u32 atclk_div;

	/* Setup cluster L */
	rkclk_set_pll(&cru->apll_l_con[0], apll_l_cfgs[apll_l_freq]);

	aclkm_div = LPLL_HZ / ACLKM_CORE_L_HZ - 1;
	assert((aclkm_div + 1) * ACLKM_CORE_L_HZ == LPLL_HZ &&
	       aclkm_div < 0x1f);

	pclk_dbg_div = LPLL_HZ / PCLK_DBG_L_HZ - 1;
	assert((pclk_dbg_div + 1) * PCLK_DBG_L_HZ == LPLL_HZ &&
	       pclk_dbg_div < 0x1f);

	atclk_div = LPLL_HZ / ATCLK_CORE_L_HZ - 1;
	assert((atclk_div + 1) * ATCLK_CORE_L_HZ == LPLL_HZ &&
	       atclk_div < 0x1f);

	rk_clrsetreg(&cru->clksel_con[0],
		     ACLKM_CORE_L_DIV_CON_MASK | CLK_CORE_L_PLL_SEL_MASK |
		     CLK_CORE_L_DIV_MASK,
		     aclkm_div << ACLKM_CORE_L_DIV_CON_SHIFT |
		     CLK_CORE_L_PLL_SEL_ALPLL << CLK_CORE_L_PLL_SEL_SHIFT |
		     0 << CLK_CORE_L_DIV_SHIFT);

	rk_clrsetreg(&cru->clksel_con[1],
		     PCLK_DBG_L_DIV_MASK | ATCLK_CORE_L_DIV_MASK,
		     pclk_dbg_div << PCLK_DBG_L_DIV_SHIFT |
		     atclk_div << ATCLK_CORE_L_DIV_SHIFT);
}

static void pmuclk_init(struct rk3399_pmucru *pmucru)
{
	u32 pclk_div;

	/*  configure pmu pll(ppll) */
	rkclk_set_pll(&pmucru->ppll_con[0], &ppll_init_cfg);

	/*  configure pmu pclk */
	pclk_div = PPLL_HZ / PMU_PCLK_HZ - 1;
	rk_clrsetreg(&pmucru->pmucru_clksel[0],
		     PMU_PCLK_DIV_CON_MASK,
		     pclk_div << PMU_PCLK_DIV_CON_SHIFT);
}

static void rkclk_init(struct rockchip_cru *cru)
{
	u32 aclk_div;
	u32 hclk_div;
	u32 pclk_div;

	// FIXME: do we even need this?

	rk3399_configure_cpu_l(cru, APLL_L_600_MHZ);
	rk3399_configure_cpu_b(cru, APLL_B_600_MHZ);
	/*
	 * some cru registers changed by bootrom, we'd better reset them to
	 * reset/default values described in TRM to avoid confusion in kernel.
	 * Please consider these three lines as a fix of bootrom bug.
	 */
	rk_clrsetreg(&cru->clksel_con[12], 0xffff, 0x4101);
	rk_clrsetreg(&cru->clksel_con[19], 0xffff, 0x033f);
	rk_clrsetreg(&cru->clksel_con[56], 0x0003, 0x0003);

	/* configure gpll cpll */
	rkclk_set_pll(&cru->gpll_con[0], &gpll_init_cfg);
	rkclk_set_pll(&cru->cpll_con[0], &cpll_init_cfg);

	/* configure perihp aclk, hclk, pclk */
	aclk_div = GPLL_HZ / PERIHP_ACLK_HZ - 1;
	assert((aclk_div + 1) * PERIHP_ACLK_HZ == GPLL_HZ && aclk_div < 0x1f);

	hclk_div = PERIHP_ACLK_HZ / PERIHP_HCLK_HZ - 1;
	assert((hclk_div + 1) * PERIHP_HCLK_HZ ==
	       PERIHP_ACLK_HZ && (hclk_div < 0x4));

	pclk_div = PERIHP_ACLK_HZ / PERIHP_PCLK_HZ - 1;
	assert((pclk_div + 1) * PERIHP_PCLK_HZ ==
	       PERIHP_ACLK_HZ && (pclk_div < 0x7));

	rk_clrsetreg(&cru->clksel_con[14],
		     PCLK_PERIHP_DIV_CON_MASK | HCLK_PERIHP_DIV_CON_MASK |
		     ACLK_PERIHP_PLL_SEL_MASK | ACLK_PERIHP_DIV_CON_MASK,
		     pclk_div << PCLK_PERIHP_DIV_CON_SHIFT |
		     hclk_div << HCLK_PERIHP_DIV_CON_SHIFT |
		     ACLK_PERIHP_PLL_SEL_GPLL << ACLK_PERIHP_PLL_SEL_SHIFT |
		     aclk_div << ACLK_PERIHP_DIV_CON_SHIFT);

	/* configure perilp0 aclk, hclk, pclk */
	aclk_div = GPLL_HZ / PERILP0_ACLK_HZ - 1;
	assert((aclk_div + 1) * PERILP0_ACLK_HZ == GPLL_HZ && aclk_div < 0x1f);

	hclk_div = PERILP0_ACLK_HZ / PERILP0_HCLK_HZ - 1;
	assert((hclk_div + 1) * PERILP0_HCLK_HZ ==
	       PERILP0_ACLK_HZ && (hclk_div < 0x4));

	pclk_div = PERILP0_ACLK_HZ / PERILP0_PCLK_HZ - 1;
	assert((pclk_div + 1) * PERILP0_PCLK_HZ ==
	       PERILP0_ACLK_HZ && (pclk_div < 0x7));

	rk_clrsetreg(&cru->clksel_con[23],
		     PCLK_PERILP0_DIV_CON_MASK | HCLK_PERILP0_DIV_CON_MASK |
		     ACLK_PERILP0_PLL_SEL_MASK | ACLK_PERILP0_DIV_CON_MASK,
		     pclk_div << PCLK_PERILP0_DIV_CON_SHIFT |
		     hclk_div << HCLK_PERILP0_DIV_CON_SHIFT |
		     ACLK_PERILP0_PLL_SEL_GPLL << ACLK_PERILP0_PLL_SEL_SHIFT |
		     aclk_div << ACLK_PERILP0_DIV_CON_SHIFT);

	/* perilp1 hclk select gpll as source */
	hclk_div = GPLL_HZ / PERILP1_HCLK_HZ - 1;
	assert((hclk_div + 1) * PERILP1_HCLK_HZ ==
	       GPLL_HZ && (hclk_div < 0x1f));

	pclk_div = PERILP1_HCLK_HZ / PERILP1_HCLK_HZ - 1;
	assert((pclk_div + 1) * PERILP1_HCLK_HZ ==
	       PERILP1_HCLK_HZ && (hclk_div < 0x7));

	rk_clrsetreg(&cru->clksel_con[25],
		     PCLK_PERILP1_DIV_CON_MASK | HCLK_PERILP1_DIV_CON_MASK |
		     HCLK_PERILP1_PLL_SEL_MASK,
		     pclk_div << PCLK_PERILP1_DIV_CON_SHIFT |
		     hclk_div << HCLK_PERILP1_DIV_CON_SHIFT |
		     HCLK_PERILP1_PLL_SEL_GPLL << HCLK_PERILP1_PLL_SEL_SHIFT);
}

static void rk3399_clock_init(void)
{
	struct rockchip_cru *cru = IOMEM(0xff760000);
	struct rk3399_pmucru *pmucru = IOMEM(0xff750000);

	rkclk_init(cru);
	pmuclk_init(pmucru);
}

void rk3399_lowlevel_init(void)
{
	struct rk3399_pmusgrf_regs __iomem *sgrf;
	struct rk3399_grf_regs __iomem *grf;

	/*
	 * Disable DDR and SRAM security regions.
	 *
	 * As we are entered from the BootROM, the region from
	 * 0x0 through 0xfffff (i.e. the first MB of memory) will
	 * be protected. This will cause issues with the DW_MMC
	 * driver, which tries to DMA from/to the stack (likely)
	 * located in this range.
	 */
	sgrf = PMUSGRF_BASE;
	rk_clrsetreg(&sgrf->ddr_rgn_con[16], 0x1ff, 0);
	rk_clrreg(&sgrf->slv_secure_con4, 0x2000);

	/*  eMMC clock generator: disable the clock multipilier */
	grf = GRF_BASE;
	rk_clrreg(&grf->emmccore_con[11], 0x0ff);

	rockchip_stimer_init(IOMEM(RK3399_STIMER_BASE));

	rk3399_clock_init();
}

int rk3399_init(bool pro)
{
	rockchip_parse_bootrom_iram(rockchip_scratch_space());
	return 0;
}
