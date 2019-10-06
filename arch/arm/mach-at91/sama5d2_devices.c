// SPDX-License-Identifier: GPL-2.0
/*
 *  On-Chip devices setup code for the SAMA5D2 family
 *
 *  Copyright (C) 2014 Atmel Corporation.
 *		       Bo Shen <voice.shen@atmel.com>
 *  Copyright (C) 2019 Ahmad Fatoum, Pengutronix
 */

#include <common.h>
#include <init.h>
#include <linux/sizes.h>
#include <gpio.h>
#include <asm/armlinux.h>
#include <mach/hardware.h>
#include <mach/board.h>
#include <mach/at91_pmc.h>
#include <mach/at91sam9x5_matrix.h>
#include <mach/at91_ddrsdrc.h>
#include <mach/iomux.h>
#include <mach/cpu.h>
#include <i2c/i2c-gpio.h>

#include "generic.h"

void at91_add_device_sdram(u32 size)
{
	if (!size)
		size = at91sama5_get_ddram_size(IOMEM(SAMA5D2_BASE_MPDDRC));

	arm_add_mem_device("ram0", SAMA5_DDRCS, size);
	add_mem_device("sram0", SAMA5D2_BASE_SRAM0,
		       SAMA5D2_SRAM_SIZE, IORESOURCE_MEM_WRITEABLE);
}

/* --------------------------------------------------------------------
 *  SPI
 * -------------------------------------------------------------------- */
#if defined(CONFIG_DRIVER_SPI_ATMEL)
static unsigned spi0_standard_cs[2] = { AT91_PIN_PC3, AT91_PIN_PC4 };
static unsigned spi1_standard_cs[2] = { AT91_PIN_PB21, AT91_PIN_PB22 };

static struct at91_spi_platform_data spi_pdata[] = {
	[0] = {
		.chipselect = spi0_standard_cs,
		.num_chipselect = ARRAY_SIZE(spi0_standard_cs),
	},
	[1] = {
		.chipselect = spi1_standard_cs,
		.num_chipselect = ARRAY_SIZE(spi1_standard_cs),
	},
};

void at91_add_device_spi(int spi_id, struct at91_spi_platform_data *pdata)
{
	int i;
	int cs_pin;
	resource_size_t start = ~0;

	BUG_ON(spi_id > 1);

	if (!pdata)
		pdata = &spi_pdata[spi_id];

	for (i = 0; i < pdata->num_chipselect; i++) {
		cs_pin = pdata->chipselect[i];

		/* enable chip-select pin */
		if (gpio_is_valid(cs_pin))
			at91_set_gpio_output(cs_pin, 1);
	}

	/* Configure SPI bus(es) */
	switch (spi_id) {
	case 0:
		start = SAMA5D2_BASE_SPI0;
		at91_set_A_periph(AT91_PIN_PC0, 0);	/* SPI0_MISO */
		at91_set_A_periph(AT91_PIN_PC1, 0);	/* SPI0_MOSI */
		at91_set_A_periph(AT91_PIN_PC2, 0);	/* SPI0_SPCK */
		break;
	case 1:
		start = SAMA5D2_BASE_SPI1;
		at91_set_A_periph(AT91_PIN_PB18, 0);	/* SPI1_MISO */
		at91_set_A_periph(AT91_PIN_PB19, 0);	/* SPI1_MOSI */
		at91_set_A_periph(AT91_PIN_PB20, 0);	/* SPI1_SPCK */
		break;
	}

	add_generic_device("atmel_spi", spi_id, NULL, start, SZ_16K,
			   IORESOURCE_MEM, pdata);
}
#else
void at91_add_device_spi(int spi_id, struct at91_spi_platform_data *pdata) {}
#endif

/* --------------------------------------------------------------------
 *  UART
 * -------------------------------------------------------------------- */
#if defined(CONFIG_DRIVER_SERIAL_ATMEL)
resource_size_t __init at91_configure_dbgu(void)
{
	at91_set_A_periph(AT91_PIN_PD2, 1);		/* DBGU_RXD */
	at91_set_A_periph(AT91_PIN_PD3, 0);		/* DBGU_TXD */

	return SAMA5D2_BASE_UART1;
}
#endif
