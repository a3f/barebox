/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support (at91bootstrap)
 * ----------------------------------------------------------------------------
 * Copyright (c) 2007, Stelian Pop <stelian.pop@leadtechdesign.com>
 * Copyright (c) 2007 Lead Tech Design <www.leadtechdesign.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/kconfig.h>
#include <asm/system.h>
#include <mach/at91_ddrsdrc.h>
#include <mach/ddramc.h>
#include <mach/early_udelay.h>

#define write_ddramc(addr, off, value) __raw_writel((value), (addr) + (off))
#define read_ddramc(addr, off) __raw_readl((addr) + (off))

static int ddramc_decodtype_is_seq(unsigned int ddramc_cr)
{
	if (IS_ENABLED(CONFIG_SOC_AT91SAM9X5) || IS_ENABLED(CONFIG_SOC_AT91SAM9N12)
	    || IS_ENABLED(CONFIG_SOC_SAMA5)) {
		if (ddramc_cr & AT91C_DDRC2_DECOD_INTERLEAVED)
			return 0;
	}
	return 1;
}


int ddram_initialize(unsigned long base_address,
		     unsigned long ram_address,
		     struct ddramc_register *ddramc_config)
{
	unsigned int ba_offset;
	unsigned int cr = 0;

	/* compute BA[] offset according to CR configuration */
	ba_offset = (ddramc_config->cr & AT91C_DDRC2_NC) + 9;
	if (ddramc_decodtype_is_seq(ddramc_config->cr))
		ba_offset += ((ddramc_config->cr & AT91C_DDRC2_NR) >> 2) + 11;

	ba_offset += (ddramc_config->mdr & AT91C_DDRC2_DBW) ? 1 : 2;

	/*
	 * Step 1: Program the memory device type into the Memory Device Register
	 */
	write_ddramc(base_address, HDDRSDRC2_MDR, ddramc_config->mdr);

	/*
	 * Step 2: Program the feature of DDR2-SDRAM device into
	 * the Timing Register, and into the Configuration Register
	 */
	write_ddramc(base_address, HDDRSDRC2_CR, ddramc_config->cr);

	write_ddramc(base_address, HDDRSDRC2_T0PR, ddramc_config->t0pr);
	write_ddramc(base_address, HDDRSDRC2_T1PR, ddramc_config->t1pr);
	write_ddramc(base_address, HDDRSDRC2_T2PR, ddramc_config->t2pr);

	/*
	 * Step 3: An NOP command is issued to the DDR2-SDRAM
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_NOP_CMD);
	*((unsigned volatile int *)ram_address) = 0;
	/* Now, clocks which drive the DDR2-SDRAM device are enabled */

	/* A minimum pause wait 200 us is provided to precede any signal toggle.
	   (6 core cycles per iteration, core is at 396MHz: min 13340 loops) */
	early_udelay(200);

	/*
	 * Step 4:  An NOP command is issued to the DDR2-SDRAM
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_NOP_CMD);
	*((unsigned volatile int *)ram_address) = 0;
	/* Now, CKE is driven high */
	/* wait 400 ns min */
	early_udelay(1);

	/*
	 * Step 5: An all banks precharge command is issued to the DDR2-SDRAM.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_PRCGALL_CMD);
	*((unsigned volatile int *)ram_address) = 0;

	/* wait 2 cycles min (of tCK) = 15 ns min */
	early_udelay(1);

	/*
	 * Step 6: An Extended Mode Register set(EMRS2) cycle is issued to chose between commercial or high
	 * temperature operations.
	 * Perform a write access to DDR2-SDRAM to acknowledge this command.
	 * The write address must be chosen so that BA[1] is set to 1 and BA[0] is set to 0.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_EXT_LMR_CMD);
	*((unsigned int *)(ram_address + (0x2 << ba_offset))) = 0;

	/* wait 2 cycles min (of tCK) = 15 ns min */
	early_udelay(1);

	/*
	 * Step 7: An Extended Mode Register set(EMRS3) cycle is issued
	 * to set the Extended Mode Register to "0".
	 * Perform a write access to DDR2-SDRAM to acknowledge this command.
	 * The write address must be chosen so that BA[1] is set to 1 and BA[0] is set to 1.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_EXT_LMR_CMD);
	*((unsigned int *)(ram_address + (0x3 << ba_offset))) = 0;

	/* wait 2 cycles min (of tCK) = 15 ns min */
	early_udelay(1);

	/*
	 * Step 8: An Extened Mode Register set(EMRS1) cycle is issued to enable DLL,
	 * and to program D.I.C(Output Driver Impedance Control)
	 * Perform a write access to DDR2-SDRAM to acknowledge this command.
	 * The write address must be chosen so that BA[1] is set to 0 and BA[0] is set to 1.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_EXT_LMR_CMD);
	*((unsigned int *)(ram_address + (0x1 << ba_offset))) = 0;

	/* An additional 200 cycles of clock are required for locking DLL */
	early_udelay(1);

	/*
	 * Step 9: Program DLL field into the Configuration Register to high(Enable DLL reset)
	 */
	cr = read_ddramc(base_address, HDDRSDRC2_CR);
	write_ddramc(base_address, HDDRSDRC2_CR, cr | AT91C_DDRC2_ENABLE_RESET_DLL);

	/*
	 * Step 10: A Mode Register set(MRS) cycle is issied to reset DLL.
	 * Perform a write access to DDR2-SDRAM to acknowledge this command.
	 * The write address must be chosen so that BA[1:0] bits are set to 0.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_LMR_CMD);
	*((unsigned int *)(ram_address + (0x0 << ba_offset))) = 0;

	/* wait 2 cycles min (of tCK) = 15 ns min */
	early_udelay(1);

	/*
	 * Step 11: An all banks precharge command is issued to the DDR2-SDRAM.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_PRCGALL_CMD);
	*(((unsigned volatile int *)ram_address)) = 0;

	/* wait 400 ns min (not needed on certain DDR2 devices) */
	early_udelay(1);

	/*
	 * Step 12: Two auto-refresh (CBR) cycles are provided.
	 * Program the auto refresh command (CBR) into the Mode Register.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_RFSH_CMD);
	*(((unsigned volatile int *)ram_address)) = 0;

	/* wait TRFC cycles min (135 ns min) extended to 400 ns */
	early_udelay(1);

	/* Set 2nd CBR */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_RFSH_CMD);
	*(((unsigned volatile int *)ram_address)) = 0;

	/* wait TRFC cycles min (135 ns min) extended to 400 ns */
	early_udelay(1);

	/*
	 * Step 13: Program DLL field into the Configuration Register to low(Disable DLL reset).
	 */
	cr = read_ddramc(base_address, HDDRSDRC2_CR);
	write_ddramc(base_address, HDDRSDRC2_CR, cr & (~AT91C_DDRC2_ENABLE_RESET_DLL));

	/*
	 * Step 14: A Mode Register set (MRS) cycle is issued to program
	 * the parameters of the DDR2-SDRAM devices, in particular CAS latency,
	 * burst length and to disable DDL reset.
	 * Perform a write access to DDR2-SDRAM to acknowledge this command.
	 * The write address must be chosen so that BA[1:0] bits are set to 0.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_LMR_CMD);
	*((unsigned int *)(ram_address + (0x0 << ba_offset))) = 0;

	/* wait 2 cycles min (of tCK) = 15 ns min */
	early_udelay(1);

	/*
	 * Step 15: Program OCD field into the Configuration Register
	 * to high (OCD calibration default).
	 */
	cr = read_ddramc(base_address, HDDRSDRC2_CR);
	write_ddramc(base_address, HDDRSDRC2_CR, cr | AT91C_DDRC2_OCD_DEFAULT);

	/* wait 2 cycles min (of tCK) = 15 ns min */
	early_udelay(1);

	/*
	 * Step 16: An Extended Mode Register set (EMRS1) cycle is issued to OCD default value.
	 * Perform a write access to DDR2-SDRAM to acknowledge this command.
	 * The write address must be chosen so that BA[1] is set to 0 and BA[0] is set to 1.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_EXT_LMR_CMD);
	*((unsigned int *)(ram_address + (0x1 << ba_offset))) = 0;

	/* wait 2 cycles min (of tCK) = 15 ns min */
	early_udelay(1);

	/*
	 * Step 17: Program OCD field into the Configuration Register
	 * to low (OCD calibration mode exit).
	 */
	cr = read_ddramc(base_address, HDDRSDRC2_CR);
	write_ddramc(base_address, HDDRSDRC2_CR, cr & (~AT91C_DDRC2_OCD_DEFAULT));

	/* wait 2 cycles min (of tCK) = 15 ns min */
	early_udelay(1);

	/*
	 * Step 18: An Extended Mode Register set (EMRS1) cycle is issued to enable OCD exit.
	 * Perform a write access to DDR2-SDRAM to acknowledge this command.
	 * The write address must be chosen so that BA[1] is set to 0 and BA[0] is set to 1.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_EXT_LMR_CMD);
	*((unsigned int *)(ram_address + (0x1 << ba_offset))) = 0;

	/* wait 2 cycles min (of tCK) = 15 ns min */
	early_udelay(1);

	/*
	 * Step 19: A Nornal mode command is provided.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_NORMAL_CMD);
	*(((unsigned volatile int *)ram_address)) = 0;

	/*
	 * Step 20: Perform a write access to any DDR2-SDRAM address
	 */
	*(((unsigned volatile int *)ram_address)) = 0;

	/*
	 * Step 21: Write the refresh rate into the count field in the Refresh Timer register.
	 */
	write_ddramc(base_address, HDDRSDRC2_RTR, ddramc_config->rtr);

	/*
	 * Now we are ready to work on the DDRSDR
	 *  wait for end of calibration
	 */
	early_udelay(10);

	return 0;
}

/* This initialization sequence is sama5d3 and sama5d4 LP-DDR2 specific */

int lpddr2_sdram_initialize(unsigned long base_address,
			    unsigned long ram_address,
			    struct ddramc_register *ddramc_config)
{
	unsigned int reg;

	write_ddramc(base_address,
		     MPDDRC_LPDDR2_LPR, ddramc_config->lpddr2_lpr);

	write_ddramc(base_address,
		     MPDDRC_LPDDR2_TIM_CAL, ddramc_config->tim_calr);

	/*
	 * Step 1: Program the memory device type.
	 */
	write_ddramc(base_address, HDDRSDRC2_MDR, ddramc_config->mdr);

	/*
	 * Step 2: Program the feature of the low-power DDR2-SDRAM device.
	 */
	write_ddramc(base_address, HDDRSDRC2_CR, ddramc_config->cr);

	write_ddramc(base_address, HDDRSDRC2_T0PR, ddramc_config->t0pr);
	write_ddramc(base_address, HDDRSDRC2_T1PR, ddramc_config->t1pr);
	write_ddramc(base_address, HDDRSDRC2_T2PR, ddramc_config->t2pr);

#if 0 /* Adjust Refresh function: we don't use the feature */
	/*
	 * Step 2bis: As we don't use the Adjust Refresh function, no need
	 * to open the input buffers.
	 */
	reg = readl(AT91_BASE_SFR + SFR_DDRCFG);
	reg |= AT91C_DDRCFG_FDQIEN;
	reg |= AT91C_DDRCFG_FDQSIEN;
	writel(reg, AT91_BASE_SFR + SFR_DDRCFG);
#endif

	/*
	 * Step 3: A NOP command is issued to the low-power DDR2-SDRAM.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_NOP_CMD);

	/*
	 * Step 3bis: Add memory barrier then Perform a write access to
	 * any low-power DDR2-SDRAM address to acknowledge the command.
	 */
	dmb();
	*((unsigned volatile int *)ram_address) = 0;

	/*
	 * Step 4: A pause of at least 100 ns must be observed before
	 * a single toggle.
	 */
	early_udelay(1);

	/*
	 * Step 5: A NOP command is issued to the low-power DDR2-SDRAM.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_NOP_CMD);
	dmb();
	*((unsigned volatile int *)ram_address) = 0;

	/*
	 * Step 6: A pause of at least 200 us must be observed before a Reset
	 * Command.
	 */
	early_udelay(200);

	/*
	 * Step 7: A Reset command is issued to the low-power DDR2-SDRAM.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR,
		     AT91C_DDRC2_MRS(63) | AT91C_DDRC2_MODE_LPDDR2_CMD);
	dmb();
	*((unsigned volatile int *)ram_address) = 0;

	/*
	 * Step 8: A pause of at least tINIT5 must be observed before issuing
	 * any commands.
	 */
	early_udelay(1);

	/*
	 * Step 9: A Calibration command is issued to the low-power DDR2-SDRAM.
	 */
	reg = read_ddramc(base_address, HDDRSDRC2_CR);
	reg &= ~AT91C_DDRC2_ZQ;
	reg |= AT91C_DDRC2_ZQ_RESET;
	write_ddramc(base_address, HDDRSDRC2_CR, reg);

	write_ddramc(base_address, HDDRSDRC2_MR,
		     AT91C_DDRC2_MRS(10) | AT91C_DDRC2_MODE_LPDDR2_CMD);
	dmb();
	*((unsigned volatile int *)ram_address) = 0;

	/*
	 * Step 9bis: The ZQ Calibration command is now issued.
	 * Program the type of calibration in the MPDDRC_CR: set the
	 * ZQ field to the SHORT value.
	 */
	reg = read_ddramc(base_address, HDDRSDRC2_CR);
	reg &= ~AT91C_DDRC2_ZQ;
	reg |= AT91C_DDRC2_ZQ_SHORT;
	write_ddramc(base_address, HDDRSDRC2_CR, reg);

	/*
	 * Step 10: A Mode Register Write command with 1 to the MRS field
	 * is issued to the low-power DDR2-SDRAM.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR,
		     AT91C_DDRC2_MRS(1) | AT91C_DDRC2_MODE_LPDDR2_CMD);
	dmb();
	*((unsigned volatile int *)ram_address) = 0;

	/*
	 * Step 11: A Mode Register Write command with 2 to the MRS field
	 * is issued to the low-power DDR2-SDRAM.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR,
		     AT91C_DDRC2_MRS(2) | AT91C_DDRC2_MODE_LPDDR2_CMD);
	dmb();
	*((unsigned volatile int *)ram_address) = 0;

	/*
	 * Step 12: A Mode Register Write command with 3 to the MRS field
	 * is issued to the low-power DDR2-SDRAM.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR,
		     AT91C_DDRC2_MRS(3) | AT91C_DDRC2_MODE_LPDDR2_CMD);
	dmb();
	*((unsigned volatile int *)ram_address) = 0;

	/*
	 * Step 13: A Mode Register Write command with 16 to the MRS field
	 * is issued to the low-power DDR2-SDRAM.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR,
		     AT91C_DDRC2_MRS(16) | AT91C_DDRC2_MODE_LPDDR2_CMD);
	dmb();
	*((unsigned volatile int *)ram_address) = 0;

	/*
	 * Step 14: A Normal Mode command is provided.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_NORMAL_CMD);
	dmb();
	*((unsigned int *)ram_address) = 0;

	/*
	 * Step 15: close the input buffers: error in documentation: no need.
	 */

	/*
	 * Step 16: Write the refresh rate into the COUNT field in the MPDDRC
	 * Refresh Timer Register.
	 */
	write_ddramc(base_address, HDDRSDRC2_RTR, ddramc_config->rtr);

	/*
	 * Now configure the CAL MR4 register.
	 */
	write_ddramc(base_address,
		     MPDDRC_LPDDR2_CAL_MR4, ddramc_config->cal_mr4r);

	return 0;
}

int lpddr1_sdram_initialize(unsigned long base_address,
			    unsigned long ram_address,
			    struct ddramc_register *ddramc_config)
{
	unsigned int ba_offset;

	/* Compute BA[] offset according to CR configuration */
	ba_offset = (ddramc_config->cr & AT91C_DDRC2_NC) + 8;
	if (!(ddramc_config->cr & AT91C_DDRC2_DECOD_INTERLEAVED))
		ba_offset += ((ddramc_config->cr & AT91C_DDRC2_NR) >> 2) + 11;

	ba_offset += (ddramc_config->mdr & AT91C_DDRC2_DBW) ? 1 : 2;

	/*
	 * Step 1: Program the memory device type in the MPDDRC Memory Device Register
	 */
	write_ddramc(base_address, HDDRSDRC2_MDR, ddramc_config->mdr);

	/*
	 * Step 2: Program the features of the low-power DDR1-SDRAM device
	 * in the MPDDRC Configuration Register and in the MPDDRC Timing
	 * Parameter 0 Register/MPDDRC Timing Parameter 1 Register.
	 */
	write_ddramc(base_address, HDDRSDRC2_CR, ddramc_config->cr);

	write_ddramc(base_address, HDDRSDRC2_T0PR, ddramc_config->t0pr);
	write_ddramc(base_address, HDDRSDRC2_T1PR, ddramc_config->t1pr);
	write_ddramc(base_address, HDDRSDRC2_T2PR, ddramc_config->t2pr);

	/*
	 * Step 3: Program Temperature Compensated Self-refresh (TCR),
	 * Partial Array Self-refresh (PASR) and Drive Strength (DS) parameters
	 * in the MPDDRC Low-power Register.
	 */
	write_ddramc(base_address, HDDRSDRC2_LPR, ddramc_config->lpr);

	/*
	 * Step 4: A NOP command is issued to the low-power DDR1-SDRAM.
	 * Program the NOP command in the MPDDRC Mode Register (MPDDRC_MR).
	 * The clocks which drive the low-power DDR1-SDRAM device
	 * are now enabled.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_NOP_CMD);
	*((unsigned volatile int *)ram_address) = 0;

	/*
	 * Step 5: A pause of at least 200 us must be observed before
	 * a signal toggle.
	 */
	early_udelay(200);

	/*
	 * Step 6: A NOP command is issued to the low-power DDR1-SDRAM.
	 * Program the NOP command in the MPDDRC_MR. calibration request is
	 * now made to the I/O pad.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_NOP_CMD);
	*((unsigned volatile int *)ram_address) = 0;

	/*
	 * Step 7: An All Banks Precharge command is issued
	 * to the low-power DDR1-SDRAM.
	 * Program All Banks Precharge command in the MPDDRC_MR.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_PRCGALL_CMD);
	*((unsigned volatile int *)ram_address) = 0;

	/*
	 * Step 8: Two auto-refresh (CBR) cycles are provided.
	 * Program the Auto Refresh command (CBR) in the MPDDRC_MR.
	 * The application must write a four to the MODE field
	 * in the MPDDRC_MR. Perform a write access to any low-power
	 * DDR1-SDRAM location twice to acknowledge these commands.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_RFSH_CMD);
	*((unsigned volatile int *)ram_address) = 0;

	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_RFSH_CMD);
	*((unsigned volatile int *)ram_address) = 0;

	/*
	 * Step 9: An Extended Mode Register Set (EMRS) cycle is issued to
	 * program the low-power DDR1-SDRAM parameters (TCSR, PASR, DS).
	 * The application must write a five to the MODE field in the MPDDRC_MR
	 * and perform a write access to the SDRAM to acknowledge this command.
	 * The write address must be chosen so that signal BA[1] is set to 1
	 * and BA[0] is set to 0.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_EXT_LMR_CMD);
	*((unsigned volatile int *)(ram_address + (0x2 << ba_offset))) = 0;

	/*
	 * Step 10: A Mode Register Set (MRS) cycle is issued to program
	 * parameters of the low-power DDR1-SDRAM devices, in particular
	 * CAS latency.
	 * The application must write a three to the MODE field in the MPDDRC_MR
	 * and perform a write access to the SDRAM to acknowledge this command.
	 * The write address must be chosen so that signals BA[1:0] are set to 0.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_LMR_CMD);
	*((unsigned volatile int *)(ram_address + (0x0 << ba_offset))) = 0;

	/*
	 * Step 11: The application must enter Normal mode, write a zero
	 * to the MODE field in the MPDDRC_MR and perform a write access
	 * at any location in the SDRAM to acknowledge this command.
	 */
	write_ddramc(base_address, HDDRSDRC2_MR, AT91C_DDRC2_MODE_NORMAL_CMD);
	*((unsigned volatile int *)ram_address) = 0;

	/*
	 * Step 12: Perform a write access to any low-power DDR1-SDRAM address.
	 */
	*((unsigned volatile int *)ram_address) = 0;

	/*
	 * Step 14: Write the refresh rate into the COUNT field in the MPDDRC
	 * Refresh Timer Register (MPDDRC_RTR):
	 */
	write_ddramc(base_address, HDDRSDRC2_RTR, ddramc_config->rtr);

	return 0;
}
