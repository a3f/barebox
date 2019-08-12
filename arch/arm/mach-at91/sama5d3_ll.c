#include <mach/sama5d3_ll.h>
#include <mach/at91_ddrsdrc.h>
#include <mach/ddramc.h>
#include <mach/early_udelay.h>

#define PMC_BASE	IOMEM(SAMA5D3_BASE_PMC)

void sama5d3_ddr2_init(struct at91_ddramc_register *ddramc_reg_config)
{
	u32 reg;

	/* enable ddr2 clock */
	sama5d3_pmc_enable_periph_clock(SAMA5D3_ID_MPDDRC);
	at91_pmc_enable_system_clock(PMC_BASE, AT91CAP9_PMC_DDR);


	/* Init the special register for sama5d3x */
	/* MPDDRC DLL Slave Offset Register: DDR2 configuration */
	reg = AT91C_MPDDRC_S0OFF_1
		| AT91C_MPDDRC_S2OFF_1
		| AT91C_MPDDRC_S3OFF_1;
	writel(reg, SAMA5D3_BASE_MPDDRC + AT91C_MPDDRC_DLL_SOR);

	/* MPDDRC DLL Master Offset Register */
	/* write master + clk90 offset */
	reg = AT91C_MPDDRC_MOFF_7
		| AT91C_MPDDRC_CLK90OFF_31
		| AT91C_MPDDRC_SELOFF_ENABLED | AT91C_MPDDRC_KEY;
	writel(reg, SAMA5D3_BASE_MPDDRC + AT91C_MPDDRC_DLL_MOR);

	/* MPDDRC I/O Calibration Register */
	/* DDR2 RZQ = 50 Ohm */
	/* TZQIO = 4 */
	reg = AT91C_MPDDRC_RDIV_DDR2_RZQ_50
		| AT91C_MPDDRC_TZQIO_4;
	writel(reg, SAMA5D3_BASE_MPDDRC + AT91C_MPDDRC_IO_CALIBR);

	/* DDRAM2 Controller initialize */
	at91_ddram_initialize(IOMEM(SAMA5D3_BASE_MPDDRC), IOMEM(SAMA5_DDRCS),
			      ddramc_reg_config);
}

void sama5d3_pmc_init(unsigned int msc)
{
	at91_pmc_init(PMC_BASE, AT91_PMC_LL_SAMA5D3);

	/* At this stage the main oscillator
	 * is supposed to be enabled PCK = MCK = MOSC
	 */

	/* Configure PLLA = MOSC * (PLL_MULA + 1) / PLL_DIVA */
	at91_pmc_cfg_plla(PMC_BASE, AT91_PMC3_MUL_(43) | AT91_PMC_OUT_0
			  | AT91_PMC_PLLCOUNT
			  | AT91_PMC_DIV_BYPASS,
			  AT91_PMC_LL_SAMA5D3);

	/* Initialize PLLA charge pump */
	at91_pmc_init_pll(PMC_BASE, AT91_PMC_IPLLA_3);

	/* Switch PCK/MCK on Main clock output */
	at91_pmc_cfg_mck(PMC_BASE, AT91SAM9_PMC_MDIV_4 | AT91_PMC_CSS_MAIN,
			 AT91_PMC_LL_SAMA5D3);

	/* Switch PCK/MCK on PLLA output */
	at91_pmc_cfg_mck(PMC_BASE, AT91SAM9_PMC_MDIV_4 | AT91_PMC_CSS_PLLA,
			 AT91_PMC_LL_SAMA5D3);

	early_udelay_init(PMC_BASE, IOMEM(SAMA5D3_BASE_PIT),
			  SAMA5D3_ID_PIT, msc, AT91_PMC_LL_SAMA5D3);
}
