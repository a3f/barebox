// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018 Microsemi Corporation.
 * Padmarao Begari, Microsemi Corporation <padmarao.begari@microsemi.com>
 * Copyright (C) 2018 Joey Hewitt <joey@joeyhewitt.com>
 */

#define pr_fmt(fmt) "beaglev-legacy-init: " fmt

#include <init.h>
#include <printk.h>
#include <clock.h>
#include <of.h>

#include "audio_clk_gen_ctrl_macro.h"
#include "audio_rst_gen_ctrl_macro.h"
#include "audio_sys_ctrl_macro.h"
#include "clkgen_ctrl_macro.h"
#include "ezGPIO_fullMux_ctrl_macro.h"
#include "global_reg.h"
#include "io.h"
#include "rstgen_ctrl_macro.h"
#include "syscon_sysmain_ctrl_macro.h"
#include "vic_iopad.h"
#include "vic_module_reset_clkgen.h"
#include "vic_ptc.h"
#include "vout_sys_clkgen_ctrl_macro.h"
#include "vout_sys_rstgen_ctrl_macro.h"

static inline void GPIO_MA_OUTW(unsigned long io, unsigned long val)
{
	pr_notice("0x%lx %lx\n", io - 0x11910000UL, val);
	(*(volatile uint32_t __iomem *)(io) = (uint32_t)(val));
}

static void gc300_init(void)
{
	_SET_SYSCON_REG_register20_u0_syscon_162_SCFG_gc300_csys_req(1);

	//nic and noc associate clk rst
	_ENABLE_CLOCK_clk_jpeg_axi_;
	_ENABLE_CLOCK_clk_jpcgc300_mainclk_;
	_ENABLE_CLOCK_clk_vdecbrg_mainclk_;

	udelay(2000);
	//gc300 clk and rst
	_ENABLE_CLOCK_clk_gc300_2x_;
	_ENABLE_CLOCK_clk_gc300_ahb_;
	_ENABLE_CLOCK_clk_gc300_axi_;

	_CLEAR_RESET_rstgen_rstn_gc300_2x_;
	_CLEAR_RESET_rstgen_rstn_gc300_axi_;
	_CLEAR_RESET_rstgen_rstn_gc300_ahb_;

	udelay(2000);
	//nic and noc associate clk rst;
	_CLEAR_RESET_rstgen_rstn_jpeg_axi_;
	_CLEAR_RESET_rstgen_rstn_jpcgc300_main_;
	_CLEAR_RESET_rstgen_rstn_vdecbrg_main_;
}

static void nvdla_init(void)
{
	_SET_SYSCON_REG_register16_SCFG_nbdla_clkgating_en(1);
	_ENABLE_CLOCK_clk_dla_bus_;
}

static void vp6_init(void)
{
	_ASSERT_RESET_rstgen_rst_vp6_DReset_;
	_ASSERT_RESET_rstgen_rst_vp6_Breset_;

	_ENABLE_CLOCK_clk_vp6_core_ ;
	_ENABLE_CLOCK_clk_vp6_axi_ ;
}

static void audio_subsys_init(void)
{
	_ENABLE_CLOCK_clk_audio_root_;
	_ENABLE_CLOCK_clk_audio_12288_;
	_ENABLE_CLOCK_clk_audio_src_;
	_ENABLE_CLOCK_clk_audio_12288_;
	_ENABLE_CLOCK_clk_dma1p_ahb_;
	_CLEAR_RESET_audio_rst_gen_rstn_apb_bus_;
	_CLEAR_RESET_audio_rst_gen_rstn_dma1p_ahb_;
}

static void i2srx_3ch_init(void)
{
	_ENABLE_CLOCK_clk_adc_mclk_;
	_ENABLE_CLOCK_clk_apb_i2sadc_;
	_CLEAR_RESET_audio_rst_gen_rstn_apb_i2sadc_;
	_CLEAR_RESET_audio_rst_gen_rstn_i2sadc_srst_;
}

static void pdm_init(void)
{
	_ENABLE_CLOCK_clk_apb_pdm_;
	_ENABLE_CLOCK_clk_pdm_mclk_;
	_CLEAR_RESET_audio_rst_gen_rstn_apb_pdm_;
}

static void i2svad_init(void)
{
	_ENABLE_CLOCK_clk_apb_i2svad_ ;
	_CLEAR_RESET_audio_rst_gen_rstn_apb_i2svad_ ;
	_CLEAR_RESET_audio_rst_gen_rstn_i2svad_srst_ ;
}

static void spdif_init(void)
{
	_ENABLE_CLOCK_clk_spdif_;
	_ENABLE_CLOCK_clk_apb_spdif_;
	_CLEAR_RESET_audio_rst_gen_rstn_apb_spdif_;
}

static void pwmdac_init(void)
{
	_ENABLE_CLOCK_clk_apb_pwmdac_;
	_CLEAR_RESET_audio_rst_gen_rstn_apb_pwmdac_;
}

static void i2sdac0_init(void)
{
	_ENABLE_CLOCK_clk_dac_mclk_;
	_ENABLE_CLOCK_clk_apb_i2sdac_;
	_CLEAR_RESET_audio_rst_gen_rstn_apb_i2sdac_;
	_CLEAR_RESET_audio_rst_gen_rstn_i2sdac_srst_;
}

static void i2sdac1_init(void)
{
	_ENABLE_CLOCK_clk_i2s1_mclk_;
	_ENABLE_CLOCK_clk_apb_i2s1_;
	_CLEAR_RESET_audio_rst_gen_rstn_apb_i2s1_;
	_CLEAR_RESET_audio_rst_gen_rstn_i2s1_srst_;
}

static void i2sdac16k_init(void)
{
	_ENABLE_CLOCK_clk_apb_i2sdac16k_;
	_CLEAR_RESET_audio_rst_gen_rstn_apb_i2sdac16k_;
	_CLEAR_RESET_audio_rst_gen_rstn_i2sdac16k_srst_;
}

static void usb_init(void)
{
	_ENABLE_CLOCK_clk_usb_axi_;
	_ENABLE_CLOCK_clk_usbphy_125m_;
	_ENABLE_CLOCK_clk_usb_lpm_clk_predft_;
	_ENABLE_CLOCK_clk_usb_stb_clk_predft_;
	_ENABLE_CLOCK_clk_apb_usb_;

	_CLEAR_RESET_rstgen_rstn_usb_axi_;
	_CLEAR_RESET_audio_rst_gen_rstn_apb_usb_;
	_CLEAR_RESET_audio_rst_gen_rst_axi_usb_;
	_CLEAR_RESET_audio_rst_gen_rst_usb_pwrup_rst_n_;
	_CLEAR_RESET_audio_rst_gen_rst_usb_PONRST_;

	/* for host */
	SET_GPIO_usb_over_current(-1);

	/* config strap */
	_SET_SYSCON_REG_SCFG_usb0_mode_strap(0x2);
	_SET_SYSCON_REG_SCFG_usb7_PLL_EN(0x1);
	_SET_SYSCON_REG_SCFG_usb7_U3_EQ_EN(0x1);
	_SET_SYSCON_REG_SCFG_usb7_U3_SSRX_SEL(0x1);
	_SET_SYSCON_REG_SCFG_usb7_U3_SSTX_SEL(0x1);
	_SET_SYSCON_REG_SCFG_usb3_utmi_iddig(0x1);
}

static void wifi_init(void)
{
	/* WiFi enable */
        SET_GPIO_37_doen_LOW;
        SET_GPIO_37_dout_HIGH;
        udelay(5000);
        SET_GPIO_37_dout_LOW;
        udelay(5000);
        SET_GPIO_37_dout_HIGH;
}

static void ezmaster_init(void)
{
	_ENABLE_CLOCK_clk_ezmaster_ahb_;
	_CLEAR_RESET_rstgen_rstn_ezmaster_ahb_;
}

static void uart0_init(void)
{
	_ENABLE_CLOCK_clk_uart0_apb_;
	_ENABLE_CLOCK_clk_uart0_core_;

	_CLEAR_RESET_rstgen_rstn_uart0_apb_;
	_CLEAR_RESET_rstgen_rstn_uart0_core_;
}

static void i2c0_init(void)
{
	_ENABLE_CLOCK_clk_i2c0_apb_;
	_ENABLE_CLOCK_clk_i2c0_core_;

	_CLEAR_RESET_rstgen_rstn_i2c0_apb_;
	_CLEAR_RESET_rstgen_rstn_i2c0_core_;
}

static void i2c1_init(void)
{
	_ENABLE_CLOCK_clk_i2c1_apb_;
	_ENABLE_CLOCK_clk_i2c1_core_;

	_CLEAR_RESET_rstgen_rstn_i2c1_apb_;
	_CLEAR_RESET_rstgen_rstn_i2c1_core_;
}

static void i2c2_init(void)
{
	_ENABLE_CLOCK_clk_i2c2_apb_;
	_ENABLE_CLOCK_clk_i2c2_core_;

	_CLEAR_RESET_rstgen_rstn_i2c2_apb_;
	_CLEAR_RESET_rstgen_rstn_i2c2_core_;
}

static void spi2_init(void)
{
    _ENABLE_CLOCK_clk_spi2_apb_;
    _ENABLE_CLOCK_clk_spi2_core_;
	_ASSERT_RESET_rstgen_rstn_spi2_core_;
	_ASSERT_RESET_rstgen_rstn_spi2_apb_;

    _CLEAR_RESET_rstgen_rstn_spi2_apb_;
    _CLEAR_RESET_rstgen_rstn_spi2_core_;
}

/* set cntr register */
static void Set_rptc_cntr(uint32_t num, uint32_t data)
{
	MA_OUTW(PTC_RPTC_CNTR(num), data);
}

/* set hrc register */
static void Set_rptc_hrc(uint32_t num, uint32_t data)
{
	MA_OUTW(PTC_RPTC_HRC(num), data);
}

/* set lrc register */
static void Set_rptc_lrc(uint32_t num, uint32_t data)
{
	MA_OUTW(PTC_RPTC_LRC(num), data);
}

/* set capture mode for pwm input signal */
static int Set_ptc_capMode(uint32_t num, uint32_t data)
{
	uint32_t value;

	value = (MA_INW(PTC_RPTC_CTRL(num))& 0x1FF);

	if(data == PTC_CAPT_SINGLE)
	{
		value |= PTC_SIGNLE;
	}
	else if(data == PTC_CAPT_CONTINUE)
	{
		value = ~( (~value) | PTC_SIGNLE);
	}
	MA_OUTW(PTC_RPTC_CTRL(num), value);

	return 0;
}

/* clear cntr in ctrl register */
static void ptc_reset_cntr(uint32_t num)
{
	uint32_t value;
	uint32_t *reg_addr;

	reg_addr = (uint32_t *)PTC_RPTC_CTRL(num);
	value = (MA_INW(reg_addr)& 0x1FF);
	value |= PTC_CNTRRST;
	MA_OUTW(reg_addr, value);
}

/* enable capture mode */
static void ptc_reset_capt(uint32_t num)
{
	uint32_t value;
	uint32_t *reg_addr;

	reg_addr = (uint32_t *)PTC_RPTC_CTRL(num);
	value = (MA_INW(reg_addr)& 0x1FF);
	value |= PTC_CAPTE;
	MA_OUTW(reg_addr, value & 0X1ff);
}

/* reset ctrl register */
static void ptc_reset_ctrl(uint32_t num)
{
	uint32_t value;
	uint32_t *reg_addr;

	reg_addr = (uint32_t *)PTC_RPTC_CTRL(num);
	value = MA_INW(reg_addr);
	value = ~( (~value) | PTC_EN);
	value = ~( (~value) | PTC_ECLK);
	value = ~( (~value) | PTC_OE);
	value = ~( (~value) | PTC_INTE);
	value = ~( (~value) | PTC_INT);
	value |= PTC_INT;
	value = ~( (~value) | PTC_CNTRRST);
	value = ~( (~value) | PTC_CAPTE);

	MA_OUTW(reg_addr, value & 0x1FF);

	value = ~( (~value) | PTC_INT);
	MA_OUTW(PTC_RPTC_CTRL(num), value & 0x1FF);
}

/*set default duty in uboot , pwm period is 400 us ,high level is 200 us */
static void ptc_set_default_duty(uint32_t num)
{
	uint32_t data_hrc = 2000;
	uint32_t data_lrc = 4000;
	uint32_t data_cap_mode = PTC_CAPT_CONTINUE;

	/* set lcr hcr cntr */
	Set_rptc_cntr(num, 0);
	Set_rptc_hrc(num,data_hrc);
	Set_rptc_lrc(num, data_lrc);
	Set_ptc_capMode(num, data_cap_mode);/* 0:continue; 1:single */
}

/* enable pwm mode ,and don't enable interrupt */
static void ptc_start(uint32_t num)
{
	uint32_t value;
	uint32_t *reg_addr;

	reg_addr = (uint32_t *)PTC_RPTC_CTRL(num);

	value = MA_INW(reg_addr);

	value |= PTC_ECLK;
	//value |= ptc_data->capmode; ///0:continue; 1:single
	value |= PTC_EN;
	value |= PTC_OE;
	//value |= PTC_INTE;
	//value &= ~PTC_INT;
	MA_OUTW(reg_addr, value);
}

/* set GPIO PIN MUX */
static void ptc_pinmux_init(uint32_t num)
{
	int i = 0;

	if(num == 0) { /* GPIOB7 */
		SET_GPIO_7_dout_pwm_pad_out_bit0;
		SET_GPIO_7_doen_LOW;
		while(0) {
			for(i=0; i<100; i++) ;
			SET_GPIO_7_dout_HIGH;
		}
	} else if(num == 1) { /* GPIOB5 */
		SET_GPIO_5_dout_pwm_pad_out_bit1;
		SET_GPIO_5_doen_LOW;
	} else if(num == 2) {
		SET_GPIO_45_dout_pwm_pad_out_bit2;
		SET_GPIO_45_doen_LOW;
	}
}

/*reset apb clock */
void ptc_reset_clock(void)
{
	vic_ptc_pwm_reset_clk_disable;
	vic_ptc_pwm_reset_clk_enable;
}

/* reset ptc */
void ptc_reset(void)
{
	uint32_t num = 0;

	for(num = 0; num < PTC_CAPT_ALL; num++) {
		/* set pin mux */
		ptc_pinmux_init(num);
		ptc_reset_cntr(num);
		ptc_reset_capt(num);
		ptc_reset_ctrl(num);
		ptc_set_default_duty(num);
	}

	for(num = 0; num < PTC_CAPT_ALL; num++) {
		ptc_start(num);
	}
}

/* added by chenjieqin for ptc on 20200824 */
static void ptc_init(void)
{
	/* reset clock */
	ptc_reset_clock();

	/* reset cnt */
	ptc_reset();
}


static void vout_subsys_init(void)
{
    _ENABLE_CLOCK_clk_vout_src_ ;
    _ENABLE_CLOCK_clk_disp_axi_;
    _ENABLE_CLOCK_clk_dispnoc_axi_ ;

    _CLEAR_RESET_rstgen_rstn_vout_src_ ;
    _CLEAR_RESET_rstgen_rstn_disp_axi_ ;
    _CLEAR_RESET_rstgen_rstn_dispnoc_axi_ ;

    _ENABLE_CLOCK_clk_vout_apb_ ;
    _ENABLE_CLOCK_clk_mapconv_apb_ ;
    _ENABLE_CLOCK_clk_mapconv_axi_ ;
    _ENABLE_CLOCK_clk_disp0_axi_ ;
    _ENABLE_CLOCK_clk_disp1_axi_ ;
    _ENABLE_CLOCK_clk_lcdc_oclk_ ;
    _ENABLE_CLOCK_clk_lcdc_axi_ ;
    _ENABLE_CLOCK_clk_vpp0_axi_ ;
    _ENABLE_CLOCK_clk_vpp1_axi_ ;
    _ENABLE_CLOCK_clk_vpp2_axi_ ;
    _ENABLE_CLOCK_clk_pixrawout_apb_ ;
    _ENABLE_CLOCK_clk_pixrawout_axi_ ;
    _ENABLE_CLOCK_clk_csi2tx_strm0_pixclk_ ;
    _ENABLE_CLOCK_clk_csi2tx_strm0_apb_ ;
    _ENABLE_CLOCK_clk_dsi_apb_ ;
    _ENABLE_CLOCK_clk_dsi_sys_clk_ ;
    _ENABLE_CLOCK_clk_ppi_tx_esc_clk_ ;

    _CLEAR_RESET_vout_sys_rstgen_rstn_mapconv_apb_ ;
    _CLEAR_RESET_vout_sys_rstgen_rstn_mapconv_axi_ ;
    _CLEAR_RESET_vout_sys_rstgen_rstn_disp0_axi_ ;
    _CLEAR_RESET_vout_sys_rstgen_rstn_disp1_axi_ ;
    _CLEAR_RESET_vout_sys_rstgen_rstn_lcdc_oclk_ ;
    _CLEAR_RESET_vout_sys_rstgen_rstn_lcdc_axi_ ;
    _CLEAR_RESET_vout_sys_rstgen_rstn_vpp0_axi_ ;
    _CLEAR_RESET_vout_sys_rstgen_rstn_vpp1_axi_ ;
    _CLEAR_RESET_vout_sys_rstgen_rstn_vpp2_axi_ ;
    _CLEAR_RESET_vout_sys_rstgen_rstn_pixrawout_apb_ ;
    _CLEAR_RESET_vout_sys_rstgen_rstn_pixrawout_axi_ ;
    _CLEAR_RESET_vout_sys_rstgen_rstn_csi2tx_strm0_apb_ ;
    _CLEAR_RESET_vout_sys_rstgen_rstn_csi2tx_strm0_pix_ ;
    _CLEAR_RESET_vout_sys_rstgen_rstn_csi2tx_ppi_tx_esc_ ;

    _CLEAR_RESET_vout_sys_rstgen_rstn_dsi_apb_ ;
    _CLEAR_RESET_vout_sys_rstgen_rstn_dsi_sys_ ;
    //TODO:confirm these register
    //_CLEAR_RESET_vout_sys_rstgen_rstn_dsi_dpi_pix_ ;
    //_CLEAR_RESET_vout_sys_rstgen_rstn_dsi_ppi_txbyte_hs_ ;
    _CLEAR_RESET_vout_sys_rstgen_rstn_dsi_ppi_tx_esc_ ;
    _CLEAR_RESET_vout_sys_rstgen_rstn_dsi_ppi_rx_esc_ ;
}

static void (*legacy_inits[])(void) = {
	gc300_init,
	nvdla_init,
	vp6_init,
	audio_subsys_init,
	i2srx_3ch_init,
	pdm_init,
	i2svad_init,
	spdif_init,
	pwmdac_init,
	i2sdac0_init,
	i2sdac1_init,
	i2sdac16k_init,
	usb_init,
	wifi_init,
	ezmaster_init,
	uart0_init,
	i2c0_init,
	i2c1_init,
	i2c2_init,
	spi2_init,
	ptc_init,

	/* Video Output Subsystem */
	vout_subsys_init,
	NULL,
};


static int beaglev_legacy_init(void)
{
	void (**init)(void);
	if (!of_machine_is_compatible("beagle,beaglev-starlight-jh7100"))
		return 0;

	pr_info("applying static configuration\n");

	for (init = legacy_inits; *init; init++) {
		pr_debug("  initializing %pS\n", *init);
		(*init)();
	}

	return 0;
}
postcore_initcall(beaglev_legacy_init);
