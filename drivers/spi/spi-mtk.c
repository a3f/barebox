// SPDX-License-Identifier: GPL-2.0-or-later
#include "clock.h"
#include <common.h>
#include <spi/spi.h>

struct mtk_spi_regs {
	u32 cfg0;
	u32 cfg1;
	u32 tx_src;
	u32 rx_dst;
	u32 tx_data;
	u32 rx_data;
	u32 cmd;
	u32 status0;
	u32 status1;
	u32 pad_macro_sel;
};

#define SPI_CMD_ACT BIT(0)
#define SPI_CMD_RESUME BIT(1)
#define SPI_CMD_RST BIT(2)
#define SPI_CMD_PAUSE_EN BIT(4)

#define SPI_CFG1_PACKET_LOOP_SHIFT 8
#define SPI_CFG1_PACKET_LEN_SHIFT 16
#define SPI_CFG1_PACKET_LOOP_MASK (0xffU << 8)
#define SPI_CFG1_PACKET_LEN_MASK (0x3ffU << 16)

#define MTK_FIFO_DEPTH 32U
#define MTK_TIMEOUT_MS 1000
#define MTK_ARBITRARY_VALUE 0xdeaddeadU

#define MTK_SPI_BUSY_STATUS 0x1
#define MTK_SPI_PAUSE_INT_STATUS 0x2

enum {
	MTK_SPI_IDLE = 0,
	MTK_SPI_PAUSE_IDLE = 1,
};

struct mtk_spi {
	struct mtk_spi_regs *regs;
	struct spi_controller host;
	int state;
};

static int mtk_spi_claim_bus(struct mtk_spi *bus)
{
	setbits_le32(&bus->regs->cmd, SPI_CMD_PAUSE_EN);
	udelay(30); /* google,cros-ec-spi-pre-delay */
	//bus->state = MTK_SPI_IDLE;
	return 0;
}

static int mtk_spi_release_bus(struct mtk_spi *bus)
{
	setbits_le32(&bus->regs->cmd, SPI_CMD_RST);
	clrbits_le32(&bus->regs->cmd, SPI_CMD_RST | SPI_CMD_PAUSE_EN);
	bus->state = MTK_SPI_IDLE;
	return 0;
}

static int mtk_spi_wait(struct mtk_spi *bus)
{
	ulong start;

	start = get_time_ns();
	while ((readl(&bus->regs->status1) & MTK_SPI_BUSY_STATUS) == 0) {
		if (is_timeout(start, MTK_TIMEOUT_MS * 1000)) {
			dev_err(bus->host.dev, "status1 timeout: %d\n",
				readl(&bus->regs->status1));
			return -ETIMEDOUT;
		}
	}
	start = get_time_ns();
	/*
	 * coreboot's MTK_SPI_PAUSE_FINISH_INT_STATUS = 3 is a MASK over
	 * bits 0,1 (finish / pause_int); the loop exits when ANY bit is
	 * set, not both. An earlier `== 3` comparison hung because on
	 * mt8196 only one of the two bits fires.
	 */
	while ((readl(&bus->regs->status0) & 3U) == 0U) {
		if (is_timeout(start, MTK_TIMEOUT_MS * 1000)) {
			dev_err(bus->host.dev, "status0 timeout\n");
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static int mtk_spi_chunk(struct mtk_spi *bus, u8 *in, const u8 *out, u32 size)
{
	struct mtk_spi_regs *regs = bus->regs;
	u32 reg_val = 0;
	u32 i;
	int ret;

	clrsetbits_le32(&regs->cfg1,
			SPI_CFG1_PACKET_LEN_MASK | SPI_CFG1_PACKET_LOOP_MASK,
			(size - 1) << SPI_CFG1_PACKET_LEN_SHIFT);

	if (out) {
		for (i = 0; i < size; i++) {
			reg_val |= (u32)out[i] << ((i % 4) * 8);
			if (i % 4 == 3) {
				writel(reg_val, &regs->tx_data);
				reg_val = 0;
			}
		}
		if (i % 4 != 0)
			writel(reg_val, &regs->tx_data);
	} else {
		/*
		 * Full-duplex controller: must clock out something for an
		 * RX-only transfer. EC ignores MOSI here.
		 */
		u32 word_count = (size + 3) / 4;

		for (i = 0; i < word_count; i++)
			writel(MTK_ARBITRARY_VALUE, &regs->tx_data);
	}

	if (bus->state == MTK_SPI_IDLE) {
		setbits_le32(&regs->cmd, SPI_CMD_ACT);
		bus->state = MTK_SPI_PAUSE_IDLE;
	} else {
		setbits_le32(&regs->cmd, SPI_CMD_RESUME);
	}

	ret = mtk_spi_wait(bus);
	if (ret)
		return ret;

	if (in) {
		for (i = 0; i < size; i++) {
			if (i % 4 == 0)
				reg_val = readl(&regs->rx_data);
			in[i] = (reg_val >> ((i % 4) * 8)) & 0xffU;
		}
	} else {
		/* Drain RX FIFO so it doesn't carry over. */
		u32 word_count = (size + 3) / 4;

		for (i = 0; i < word_count; i++)
			(void)readl(&regs->rx_data);
	}
	return 0;
}

static int mtk_spi_transfer_one(struct spi_controller *ctrlr,
				struct spi_device *spi,
				struct spi_transfer *xfer)
{
	struct mtk_spi *bus = spi_controller_get_devdata(ctrlr);
	struct mtk_spi_regs *regs = bus->regs;
	u32 size = xfer->len;
	u32 offset = 0;
	int ret = 0;

	while (size) {
		u32 chunk = size > MTK_FIFO_DEPTH ? MTK_FIFO_DEPTH : size;
		const u8 *out =
			xfer->tx_buf ? (const u8 *)xfer->tx_buf + offset : NULL;
		u8 *in = xfer->rx_buf ? (u8 *)xfer->rx_buf + offset : NULL;

		//DELTE THIS dev_warn(&spi->dev, "chunk=%d, size=%d, offset=%d, out=%ld, in=%ld\n", chunk, size, offset, out, in);

		ret = mtk_spi_chunk(bus, in, out, chunk);
		if (ret) {
			setbits_le32(&regs->cmd, SPI_CMD_RST);
			clrbits_le32(&regs->cmd, SPI_CMD_RST);
			bus->state = MTK_SPI_IDLE;
			break;
		}
		offset += chunk;
		size -= chunk;
	}

	return ret;
}

static void mtk_spi_set_cs(struct spi_device *spi, bool enable)
{
	struct mtk_spi *bus = spi_controller_get_devdata(spi->controller);

	if (spi->mode & SPI_CS_HIGH)
		enable = !enable;

	if (enable)
		mtk_spi_release_bus(bus);
	else
		mtk_spi_claim_bus(bus);

	return;
}

static int mtk_spi_setup(struct spi_device *spi)
{
	return 0;
}

static int mtk_spi_probe(struct device *dev)
{
	struct mtk_spi *bus;
	struct spi_controller *host;
	struct resource *iores;
	struct clk *parent_clk, *sel_clk, *spi_clk, *spi_hclk;
	int ret;

	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return PTR_ERR(iores);

	bus = xzalloc(sizeof(struct mtk_spi));
	host = &bus->host;

	bus->state = MTK_SPI_IDLE;
	bus->regs = (struct mtk_spi_regs *)iores->start;

	host->dev = dev;
	host->num_chipselect = 1;
	host->setup = mtk_spi_setup;
	host->transfer_one = mtk_spi_transfer_one;
	host->set_cs = mtk_spi_set_cs;

	parent_clk = clk_get_enabled(dev, "parent-clk");
	if (IS_ERR(parent_clk)) {
		dev_err(dev, "Failed to get parent_clk\n");
		ret = PTR_ERR(parent_clk);
		goto err;
	}
	sel_clk = clk_get_enabled(dev, "sel-clk");
	if (IS_ERR(sel_clk)) {
		dev_err(dev, "Failed to get sel_clk\n");
		ret = PTR_ERR(sel_clk);
		goto err;
	}
	spi_clk = clk_get_enabled(dev, "spi-clk");
	if (IS_ERR(spi_clk)) {
		dev_err(dev, "Failed to get spi_clk\n");
		ret = PTR_ERR(spi_clk);
		goto err;
	}
	spi_hclk = clk_get_optional_enabled(dev, "hclk");
	if (IS_ERR(spi_hclk)) {
		dev_err(dev, "Failed to get hclk\n");
		ret = PTR_ERR(spi_hclk);
		goto err;
	}

	ret = clk_set_parent(sel_clk, parent_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to set clock parent\n");
		goto err;
	}

	/* Reset controller to a known state. */
	setbits_le32(&bus->regs->cmd, SPI_CMD_RST);
	clrbits_le32(&bus->regs->cmd, SPI_CMD_RST);

	spi_controller_set_devdata(host, bus);

	ret = spi_register_controller(host);
	if (ret < 0) {
		dev_err(dev, "Failed to register controller\n");
		return ret;
	}

	return 0;
err:
	return -ENODEV;
}

static const struct of_device_id mtk_spi_dt_match[] = {
	{
		.compatible = "mediatek,mt6765-spi",
	},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_spi_dt_match);

static struct driver mtk_spi_driver = {
	.name = "mtk-spi",
	.probe = mtk_spi_probe,
	.of_compatible = mtk_spi_dt_match,
};
coredevice_platform_driver(mtk_spi_driver);

MODULE_DESCRIPTION("MTK SPI Controller driver");
MODULE_LICENSE("GPL v2");
