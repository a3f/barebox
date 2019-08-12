#ifndef __MACH_SAMA5D2_LL__
#define __MACH_SAMA5D2_LL__

#include <mach/sama5d2_ll.h>
#include <mach/at91_pmc_ll.h>
#include <mach/ddramc.h>

#define sama5d2_pmc_enable_periph_clock(clk) \
	at91_pmc_enable_periph_clock(IOMEM(SAMA5D2_BASE_PMC), clk)

void sama5d2_pmc_init(unsigned int msc);
void sama5d2_ddr2_init(struct at91_ddramc_register *ddramc_reg_config);

#endif
