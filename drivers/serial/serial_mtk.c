// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek High-speed UART driver
 *
 * Copyright (C) 2018 MediaTek Inc.
 * Author: Weijie Gao <weijie.gao@mediatek.com>
 */

#include <common.h>
#include <linux/clk.h>
#include <linux/math64.h>
#include <console.h>
#include <errno.h>
#include <linux/io.h>
#include <asm/types.h>
#include <linux/err.h>
#include <linux/printk.h>
#include <platform_data/serial-ns16550.h>

struct mtk_serial_regs {
	u32 rbr;
	u32 ier;
	u32 fcr;
	u32 lcr;
	u32 mcr;
	u32 lsr;
	u32 msr;
	u32 spr;
	u32 mdr1;
	u32 highspeed;
	u32 sample_count;
	u32 sample_point;
	u32 fracdiv_l;
	u32 fracdiv_m;
	u32 escape_en;
	u32 guard;
	u32 rx_sel;
};

#define thr rbr
#define iir fcr
#define dll rbr
#define dlm ier

#define UART_LCR_WLS_8	0x03		/* 8 bit character length */
#define UART_LCR_DLAB	0x80		/* Divisor latch access bit */

#define UART_LSR_DR	0x01		/* Data ready */
#define UART_LSR_THRE	0x20		/* Xmit holding register empty */
#define UART_LSR_TEMT	0x40		/* Xmitter empty */

#define UART_MCR_DTR	0x01		/* DTR   */
#define UART_MCR_RTS	0x02		/* RTS   */

#define UART_FCR_FIFO_EN	0x01	/* Fifo enable */
#define UART_FCR_RXSR		0x02	/* Receiver soft reset */
#define UART_FCR_TXSR		0x04	/* Transmitter soft reset */

#define UART_MCRVAL (UART_MCR_DTR | \
		     UART_MCR_RTS)

/* Clear & enable FIFOs */
#define UART_FCRVAL (UART_FCR_FIFO_EN | \
		     UART_FCR_RXSR |	\
		     UART_FCR_TXSR)

/* the data is correct if the real baud is within 3%. */
#define BAUD_ALLOW_MAX(baud)	((baud) + (baud) * 3 / 100)
#define BAUD_ALLOW_MIX(baud)	((baud) - (baud) * 3 / 100)

/* struct mtk_serial_priv -	Structure holding all information used by the
 *				driver
 * @regs:			Register base of the serial port
 * @clk:			The baud clock device
 * @clk_bus:			The bus clock device
 */
struct mtk_serial_priv {
	struct mtk_serial_regs __iomem *regs;
	struct clk *clk;
	struct clk *clk_bus;
	struct NS16550_plat plat;
	struct console_device cdev;
};

static inline struct mtk_serial_priv *
to_mtk_serial(struct console_device *cdev)
{
	return container_of(cdev, struct mtk_serial_priv, cdev);
}

static void _mtk_serial_setbrg(struct mtk_serial_priv *priv, int baud,
			       uint clk_rate)
{
	u32 quot, realbaud, samplecount = 1;

	/* Special case for low baud clock */
	if (baud <= 115200 && clk_rate == 12000000) {
		writel(3, &priv->regs->highspeed);

		quot = DIV_ROUND_CLOSEST(clk_rate, 256 * baud);
		if (quot == 0)
			quot = 1;

		samplecount = DIV_ROUND_CLOSEST(clk_rate, quot * baud);

		realbaud = clk_rate / samplecount / quot;
		if (realbaud > BAUD_ALLOW_MAX(baud) ||
		    realbaud < BAUD_ALLOW_MIX(baud)) {
			pr_info("baud %d can't be handled\n", baud);
		}

		goto set_baud;
	}

	/*
	 * Upstream linux use highspeed for anything >= 115200 and lowspeed
	 * for < 115200. Simulate this if we are using the upstream compatible.
	 */
	if (baud <= 115200) {
		writel(0, &priv->regs->highspeed);
		quot = DIV_ROUND_CLOSEST(clk_rate, 16 * baud);
	} else {
		writel(3, &priv->regs->highspeed);

		quot = DIV_ROUND_UP(clk_rate, 256 * baud);
		samplecount = DIV_ROUND_CLOSEST(clk_rate, quot * baud);
	}

set_baud:
	/* set divisor */
	writel(UART_LCR_WLS_8 | UART_LCR_DLAB, &priv->regs->lcr);
	writel(quot & 0xff, &priv->regs->dll);
	writel((quot >> 8) & 0xff, &priv->regs->dlm);
	writel(UART_LCR_WLS_8, &priv->regs->lcr);

	/* set highspeed mode sample count & point */
	writel(samplecount - 1, &priv->regs->sample_count);
	writel((samplecount - 2) >> 1, &priv->regs->sample_point);
}

static void _mtk_serial_putc(struct mtk_serial_priv *priv, const char ch)
{
	if (!(readl(&priv->regs->lsr) & UART_LSR_THRE))
		;

	writel(ch, &priv->regs->thr);
}

static int _mtk_serial_getc(struct mtk_serial_priv *priv)
{
	if (!(readl(&priv->regs->lsr) & UART_LSR_DR))
		return -EAGAIN;

	return readl(&priv->regs->rbr);
}

static int _mtk_serial_pending(struct mtk_serial_priv *priv, bool input)
{
	if (input)
		return (readl(&priv->regs->lsr) & UART_LSR_DR) ? 1 : 0;
	else
		return (readl(&priv->regs->lsr) & UART_LSR_THRE) ? 0 : 1;
}

static int mtk_serial_setbrg(struct console_device *cdev, int baudrate)
{
	struct mtk_serial_priv *priv = to_mtk_serial(cdev);
	u32 clk_rate;

	clk_rate = clk_get_rate(priv->clk);
	if (clk_rate <= 0)
		clk_rate = priv->plat.clock;

	_mtk_serial_setbrg(priv, baudrate, clk_rate);

	return 0;
}

static void mtk_serial_putc(struct console_device *cdev, char ch)
{
	struct mtk_serial_priv *priv = to_mtk_serial(cdev);

	_mtk_serial_putc(priv, ch);
}

static int mtk_serial_getc(struct console_device *cdev)
{
	struct mtk_serial_priv *priv = to_mtk_serial(cdev);

	return _mtk_serial_getc(priv);
}

static int mtk_serial_tstc(struct console_device *cdev)
{
	struct mtk_serial_priv *priv = to_mtk_serial(cdev);
	return _mtk_serial_pending(priv, true);
}

static int mtk_serial_probe(struct device *dev)
{
	struct device_node *np = dev_of_node(dev);
	struct mtk_serial_priv *priv;
	struct console_device *cdev;
	struct resource *res;
	int err;

	res = dev_request_mem_resource(dev, 0);
	if (IS_ERR(res))
		return PTR_ERR(res);

	priv = xzalloc(sizeof(*priv));

	priv->regs = IOMEM(res->start);

	priv->clk = clk_get(dev, NULL);
	if (IS_ERR(priv->clk)) {
		err = of_property_read_u32(np, "clock-frequency",
					   &priv->plat.clock);
		if (err) {
			dev_err(dev, "baud clock not defined\n");
			return -EINVAL;
		}
	} else {
		err = clk_get_rate(priv->clk);
		if (err < 0) {
			dev_err(dev, "invalid baud clock\n");
			return -EINVAL;
		}
	}

	priv->clk_bus = clk_get(dev, "bus");

	cdev = &priv->cdev;
	cdev->dev = dev;
	cdev->tstc = mtk_serial_tstc;
	cdev->putc = mtk_serial_putc;
	cdev->getc = mtk_serial_getc;
	cdev->setbrg = priv->clk ? mtk_serial_setbrg : NULL;
	cdev->linux_console_name = "ttyS";
	cdev->linux_earlycon_name = "mtk8250";
	cdev->phys_base = priv->regs;

	/* Disable interrupt */
	writel(0, &priv->regs->ier);

	writel(UART_MCRVAL, &priv->regs->mcr);
	writel(UART_FCRVAL, &priv->regs->fcr);

	clk_enable(priv->clk);
	clk_enable(priv->clk_bus);

	return console_register(cdev);
}

static const struct of_device_id mtk_serial_ids[] = {
	{ .compatible = "mediatek,mt6577-uart" },
	{ /* sentinel */ }
};

static struct driver mtk_serial_driver = {
	.name   = "mtk_serial",
	.probe  = mtk_serial_probe,
	.of_compatible = DRV_OF_COMPAT(mtk_serial_ids),
};
console_platform_driver(mtk_serial_driver);
