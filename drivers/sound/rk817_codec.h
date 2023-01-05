/* SPDX-License-Identifier:     GPL-2.0+ */
/*
 * (C) Copyright 2018 Rockchip Electronics Co., Ltd
 */

#ifndef __RK817_CODEC_H__
#define __RK817_CODEC_H__

/* codec register */

/* RK817_CODEC_DTOP_DIGEN_CLKE */
#define ADC_DIG_CLK_MASK		(0xf << 4)
#define ADC_DIG_CLK_SFT			4
#define ADC_DIG_CLK_DIS			(0x0 << 4)
#define ADC_DIG_CLK_EN			(0xf << 4)

#define DAC_DIG_CLK_MASK		(0xf << 0)
#define DAC_DIG_CLK_SFT			0
#define DAC_DIG_CLK_DIS			(0x0 << 0)
#define DAC_DIG_CLK_EN			(0xf << 0)

/* RK817_CODEC_APLL_CFG5 */
#define PLL_PW_DOWN			(0x01 << 0)
#define PLL_PW_UP			(0x00 << 0)

/* RK817_CODEC_DI2S_CKM */
#define PDM_EN_MASK			(0x1 << 3)
#define PDM_EN_SFT			3
#define PDM_EN_DISABLE			(0x0 << 3)
#define PDM_EN_ENABLE			(0x1 << 3)

#define SCK_EN_ENABLE			(0x1 << 2)
#define SCK_EN_DISABLE			(0x0 << 2)

#define RK817_I2S_MODE_MASK		(0x1 << 0)
#define RK817_I2S_MODE_SFT		0
#define RK817_I2S_MODE_MST		(0x1 << 0)
#define RK817_I2S_MODE_SLV		(0x0 << 0)

/* RK817_CODEC_AHP_CFG1 */
#define HP_ANTIPOP_ENABLE		(0x1 << 4)
#define HP_ANTIPOP_DISABLE		(0x0 << 4)

/* RK817_CODEC_ADAC_CFG1 */
#define PWD_DACBIAS_MASK		(0x1 << 3)
#define PWD_DACBIAS_SFT			3
#define PWD_DACBIAS_DOWN		(0x1 << 3)
#define PWD_DACBIAS_ON			(0x0 << 3)

#define PWD_DACD_MASK			(0x1 << 2)
#define PWD_DACD_SFT			2
#define PWD_DACD_DOWN			(0x1 << 2)
#define PWD_DACD_ON			(0x0 << 2)

#define PWD_DACL_MASK			(0x1 << 1)
#define PWD_DACL_SFT			1
#define PWD_DACL_DOWN			(0x1 << 1)
#define PWD_DACL_ON			(0x0 << 1)

#define PWD_DACR_MASK			(0x1 << 0)
#define PWD_DACR_SFT			0
#define PWD_DACR_DOWN			(0x1 << 0)
#define PWD_DACR_ON			(0x0 << 0)

/* RK817_CODEC_AADC_CFG0 */
#define ADC_L_PWD_MASK			(0x1 << 7)
#define ADC_L_PWD_SFT			7
#define ADC_L_PWD_DIS			(0x0 << 7)
#define ADC_L_PWD_EN			(0x1 << 7)

#define ADC_R_PWD_MASK			(0x1 << 6)
#define ADC_R_PWD_SFT			6
#define ADC_R_PWD_DIS			(0x0 << 6)
#define ADC_R_PWD_EN			(0x1 << 6)

/* RK817_CODEC_AMIC_CFG0 */
#define MIC_DIFF_MASK			(0x1 << 7)
#define MIC_DIFF_SFT			7
#define MIC_DIFF_DIS			(0x0 << 7)
#define MIC_DIFF_EN			(0x1 << 7)

#define PWD_PGA_L_MASK			(0x1 << 5)
#define PWD_PGA_L_SFT			5
#define PWD_PGA_L_DIS			(0x0 << 5)
#define PWD_PGA_L_EN			(0x1 << 5)

#define PWD_PGA_R_MASK			(0x1 << 4)
#define PWD_PGA_R_SFT			4
#define PWD_PGA_R_DIS			(0x0 << 4)
#define PWD_PGA_R_EN			(0x1 << 4)

enum {
	RK817_HIFI,
	RK817_VOICE,
};

enum {
	RK817_MONO = 1,
	RK817_STEREO,
};

enum {
	OFF,
	RCV,
	SPK_PATH,
	HP_PATH,
	HP_NO_MIC,
	BT,
	SPK_HP,
	RING_SPK,
	RING_HP,
	RING_HP_NO_MIC,
	RING_SPK_HP,
};

enum {
	MIC_OFF,
	MAIN_MIC,
	HANDS_FREE_MIC,
	BT_SCO_MIC,
};

struct rk817_reg_val_typ {
	unsigned int reg;
	unsigned int value;
};

struct rk817_init_bit_typ {
	unsigned int reg;
	unsigned int power_bit;
	unsigned int init_bit;
};

#endif /* __RK817_CODEC_H__ */
