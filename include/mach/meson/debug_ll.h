/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __MACH_MESON_DEBUG_LL_H
#define __MACH_MESON_DEBUG_LL_H

#include <io.h>
#include <mach/meson/meson.h>
#include <linux/bitops.h>

#define DEBUG_LL_UART_ADDR	MESON_UART_BASE

/* Amlogic Meson UART register offsets */
#define AML_UART_WFIFO			0x00
#define AML_UART_RFIFO			0x04
#define AML_UART_CONTROL		0x08
#define AML_UART_STATUS			0x0c
#define AML_UART_MISC			0x10
#define AML_UART_REG5			0x14

/* Status register bits */
#define AML_UART_TX_FULL		BIT(21)
#define AML_UART_TX_EMPTY		BIT(22)

static inline void meson_serial_putc(void *ctx, int c)
{
	void __iomem *base = IOMEM(ctx);

	/* Wait until TX FIFO is not full */
	while (readl(base + AML_UART_STATUS) & AML_UART_TX_FULL)
		;

	/* Write character to TX FIFO */
	writel(c, base + AML_UART_WFIFO);

	/* Wait until character is transmitted (TX FIFO empty) */
	while (!(readl(base + AML_UART_STATUS) & AML_UART_TX_EMPTY))
		;
}

static inline void PUTC_LL(int c)
{
	meson_serial_putc(IOMEM(DEBUG_LL_UART_ADDR), c);
}

// TODO: use DEBUG_MESON_UART_PORT

#endif /* __MACH_MESON_DEBUG_LL_H */

