/*
 * Copyright (C) 2012
 * Jean-Christophe PLAGNIOL-VILLARD <planioj@jcrosoft.com>
 *
 * Under GPLv2
 */

#ifndef __MACH_DEBUG_LL_H__
#define __MACH_DEBUG_LL_H__

#include <asm/io.h>
#include <mach/gpio.h>
#include <mach/sama5d2_ll.h>
#include <mach/hardware.h>
#include <mach/at91_dbgu.h>

#define ATMEL_US_CSR		0x0014
#define ATMEL_US_THR		0x001c
#define ATMEL_US_TXRDY		(1 << 1)
#define ATMEL_US_TXEMPTY	(1 << 9)

/*
 * The following code assumes the serial port has already been
 * initialized by the bootloader.  If you didn't setup a port in
 * your bootloader then nothing will appear (which might be desired).
 *
 * This does not append a newline
 */
static inline void at91_dbgu_putc(void __iomem *base, int c)
{
	while (!(readl(base + ATMEL_US_CSR) & ATMEL_US_TXRDY))
		barrier();
	writel(c, base + ATMEL_US_THR);

	while (!(readl(base + ATMEL_US_CSR) & ATMEL_US_TXEMPTY))
		barrier();
}

static inline void PUTC_LL(char c)
{
	at91_dbgu_putc(IOMEM(CONFIG_DEBUG_AT91_UART_BASE), c);
}

static inline int sama5d2_dbgu_setup_ll(unsigned dbgu_id,
					unsigned pin, unsigned periph,
					unsigned mck,
					unsigned baudrate)
{
	unsigned mask, bank, pio_id;
	void __iomem *dbgu_base, *pio_base;

	mask = pin_to_mask(pin);
	bank = pin_to_bank(pin);

	switch (dbgu_id) {
	case SAMA5D2_ID_UART0:
		dbgu_base = SAMA5D2_BASE_UART0;
		break;
	case SAMA5D2_ID_UART1:
		dbgu_base = SAMA5D2_BASE_UART1;
		break;
	case SAMA5D2_ID_UART2:
		dbgu_base = SAMA5D2_BASE_UART2;
		break;
	case SAMA5D2_ID_UART3:
		dbgu_base = SAMA5D2_BASE_UART3;
		break;
	case SAMA5D2_ID_UART4:
		dbgu_base = SAMA5D2_BASE_UART4;
		break;
	default:
		return -EINVAL;
	}

	pio_base = sama5d2_pio_map_bank(bank, &pio_id);
	if (!pio_base)
		return -EINVAL;

	sama5d2_pmc_enable_periph_clock(pio_id);

	at91_mux_pio4_set_periph(pio_base, mask, periph);

	sama5d2_pmc_enable_periph_clock(dbgu_id);

	at91_dbgu_setup_ll(dbgu_base, mck / 2, baudrate);

	return 0;
}

#endif
