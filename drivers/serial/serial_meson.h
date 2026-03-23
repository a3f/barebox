// SPDX-FileCopyrightText: Copyright (c) 2026, Sohaib Mohamed <sohaib.amhmd@gmail.com>

#ifndef _SERIAL_MESON_
#define _SERIAL_MESON_

/* AML_UART_STATUS bits */
#define AML_UART_PARITY_ERR		BIT(16)
#define AML_UART_FRAME_ERR		BIT(17)
#define AML_UART_TX_FIFO_WERR		BIT(18)
#define AML_UART_RX_EMPTY		BIT(20)
#define AML_UART_TX_FULL		BIT(21)
#define AML_UART_TX_EMPTY		BIT(22)
#define AML_UART_XMIT_BUSY		BIT(25)
#define AML_UART_ERR			(AML_UART_PARITY_ERR | \
					 AML_UART_FRAME_ERR  | \
					 AML_UART_TX_FIFO_WERR)

/* AML_UART_CONTROL bits */
#define AML_UART_TX_EN			BIT(12)
#define AML_UART_RX_EN			BIT(13)
#define AML_UART_TX_RST			BIT(22)
#define AML_UART_RX_RST			BIT(23)
#define AML_UART_CLR_ERR		BIT(24)

/* AML_UART_REG5 bits */
#define AML_UART_REG5_XTAL_DIV2		BIT(27)
#define AML_UART_REG5_XTAL_CLK_SEL	BIT(26) /* default 0 (div by 3), 1 for no div */
#define AML_UART_REG5_USE_XTAL_CLK	BIT(24) /* default 1 (use crystal as clock source) */
#define AML_UART_REG5_USE_NEW_BAUD	BIT(23) /* default 1 (use new baud rate register) */
#define AML_UART_REG5_BAUD_MASK		0x7fffff

// ----------------------------------------------------------------------------
/* Register offsets */
#define AML_UART_WFIFO      0x00
#define AML_UART_RFIFO      0x04
#define AML_UART_CONTROL    0x08
#define AML_UART_STATUS     0x0c
#define AML_UART_MISC       0x10
#define AML_UART_REG5       0x14

/* Control bits */
#define AML_UART_TX_EN      BIT(12)
#define AML_UART_RX_EN      BIT(13)
#define AML_UART_TX_RST     BIT(22)
#define AML_UART_RX_RST     BIT(23)
#define AML_UART_CLEAR_ERR  BIT(24)
#define AML_UART_DATA_LEN_8BIT  (0x00 << 20)
#define AML_UART_STOP_BIT_1SB   (0x00 << 16)

/* Status bits */
#define AML_UART_RX_EMPTY   BIT(20)
#define AML_UART_TX_FULL    BIT(21)
#define AML_UART_TX_EMPTY   BIT(22)

/* Baud rate */
#define AML_UART_BAUD_MASK  0x7fffff
#define AML_UART_BAUD_USE   BIT(23)
#define AML_UART_BAUD_XTAL  BIT(24)


#endif
