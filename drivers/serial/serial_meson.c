// drivers/serial/serial_meson.c
// SPDX-License-Identifier: GPL-2.0-or-later

#define DEBUG
#include <common.h>
#include <console.h>
#include <io.h>
#include <linux/clk.h>
#include <init.h>
#include <driver.h>
#include "serial_meson.h"

struct meson_uart {
	struct console_device cdev;
	void __iomem *base;
	struct clk *clk;
	bool mesonf4;
	bool has_fifo;
	int uart_enable_bit;
};

static struct meson_uart *to_meson_uart(struct console_device *cdev)
{
	return container_of(cdev, struct meson_uart, cdev);
}

static void meson_uart_putc(struct console_device *cdev, char c)
{
	struct meson_uart *meson = container_of(cdev,
		struct meson_uart, cdev);

	/* Wait until TX FIFO not full */
	while (readl(meson->base + AML_UART_STATUS) & AML_UART_TX_FULL)
		;

	writel(c, meson->base + AML_UART_WFIFO);
}

static int meson_uart_getc(struct console_device *cdev)
{
	struct meson_uart *meson = container_of(cdev,
		struct meson_uart, cdev);

	/* Wait until RX FIFO not empty */
	while (readl(meson->base + AML_UART_STATUS) & AML_UART_RX_EMPTY)
		;

	return readl(meson->base + AML_UART_RFIFO) & 0xff;
}

static int meson_uart_tstc(struct console_device *cdev)
{
	struct meson_uart *meson = container_of(cdev,
		struct meson_uart, cdev);

	return !(readl(meson->base + AML_UART_STATUS) & AML_UART_RX_EMPTY);
}

static void meson_uart_flush(struct console_device *cdev)
{
	struct meson_uart *meson = container_of(cdev,
		struct meson_uart, cdev);

	/* Wait until TX empty */
	while (!(readl(meson->base + AML_UART_STATUS) & AML_UART_TX_EMPTY))
		;
}

static int meson_uart_setbaudrate(struct console_device *cdev, int baudrate)
{
	//struct meson_uart *meson = container_of(cdev,
	//	struct meson_uart, cdev);
	//u32 val;

	///* Calculate divisor for 24MHz crystal */
	//if (meson->uartclk == 24000000) {
	//	val = DIV_ROUND_CLOSEST(meson->uartclk / 3, baudrate) - 1;
	//	val |= AML_UART_BAUD_XTAL;
	//} else {
	//	val = DIV_ROUND_CLOSEST(meson->uartclk / 4, baudrate) - 1;
	//}
	//val |= AML_UART_BAUD_USE;

	//writel(val, meson->base + AML_UART_REG5);

	return 0;
}

static void meson_serial_init(struct console_device *cdev)
{
	struct meson_uart *meson = to_meson_uart(cdev);
	void __iomem *base = meson->base;
	bool mesonf4 = meson->mesonf4;
	u8 uart_enable_bit = meson->uart_enable_bit;
	u32 val;

	pr_notice("%s:%d\n", __func__, __LINE__);
	//meson->uartclk = clk_get_rate(meson->clk);

	pr_notice("%s:%d\n", __func__, __LINE__);
	/* Reset UART */
	val = readl(base + AML_UART_CONTROL);
	val |= AML_UART_RX_RST | AML_UART_TX_RST | AML_UART_CLEAR_ERR;
	writel(val, base + AML_UART_CONTROL);
	pr_notice("%s:%d\n", __func__, __LINE__);
	val &= ~(AML_UART_RX_RST | AML_UART_TX_RST | AML_UART_CLEAR_ERR);
	writel(val, base + AML_UART_CONTROL);
	pr_notice("%s:%d\n", __func__, __LINE__);

	pr_notice("%s:%d\n", __func__, __LINE__);
	/* Configure: 8N1, enable TX/RX */
	///////val = AML_UART_DATA_LEN_8BIT | AML_UART_STOP_BIT_1SB |
	///////      AML_UART_TX_EN | AML_UART_RX_EN;
	///////writel(val, meson->base + AML_UART_CONTROL);

	pr_notice("%s:%d\n", __func__, __LINE__);
}

static int meson_uart_probe(struct device *dev)
{
	int ret;
	struct meson_uart *meson;
	struct console_device *cdev;
	struct resource *iores;

	pr_notice("%s:%d\n", __func__, __LINE__);

	meson = xzalloc(sizeof(*meson));
	cdev = &meson->cdev;

	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return PTR_ERR(iores);

	pr_notice("%s:%d\n", __func__, __LINE__);
	meson->base = IOMEM(iores->start);

	pr_notice("%s:%d\n", __func__, __LINE__);
	/* Get clocks */
	//meson->clk = clk_get(dev, "xtal");
	//if (IS_ERR(meson->clk)) {
	//	dev_err(dev, "failed to get xtal clock\n");
	//	return PTR_ERR(meson->clk);
	//}

	//pr_notice("%s:%d\n", __func__, __LINE__);
	//ret = clk_enable(meson->clk);
	//if (ret)
	//	return ret;

	/* Register console */
	cdev->dev = dev;
	cdev->tstc = meson_uart_tstc;
	cdev->putc = meson_uart_putc;
	cdev->getc = meson_uart_getc;
	cdev->flush = meson_uart_flush;
	cdev->setbrg = meson_uart_setbaudrate;
	cdev->linux_console_name = "ttyAML";
	cdev->linux_earlycon_name = "meson";

	pr_notice("%s:%d\n", __func__, __LINE__);
	meson_serial_init(cdev);
	pr_notice("%s:%d\n", __func__, __LINE__);

	pr_notice("%s:%d\n", __func__, __LINE__);
	return console_register(cdev);
}

static struct of_device_id meson_uart_dt_ids[] = {
	{
		.compatible = "amlogic,meson-ao-uart",
		// TODO
		//.data = &mesonh7_info
	}, {
		.compatible = "amlogic,meson-gx-uart",
		// TODO
		//.data = &mesonh7_info
	}, {
		.compatible = "amlogic,meson-g12a-uart",
		// TODO
		//.data = &mesonh7_info
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, meson_uart_dt_ids);

static struct driver meson_uart_driver = {
	.name = "meson_uart",
	.probe = meson_uart_probe,
	.of_compatible = DRV_OF_COMPAT(meson_uart_dt_ids),
};
console_platform_driver(meson_uart_driver);
