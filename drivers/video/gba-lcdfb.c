/*
 * Driver for Gameboy Advance BG mode 3 Display
 *
 * Copyright (C) 2019 Ahmad Fatoum
 */

#include <common.h>
#include <init.h>
#include <io.h>
#include <errno.h>
#include <fb.h>

#define DISPCNT		0
#define	DISPCNT_BGMODE_MASK	0x3
#define DISPCNT_BGMODE(x) ((x) & DISPCNT_BGMODE_MASK)
#define	DISPCNT_DISPENABLE_MASK	0x1F00
#define	DISPCNT_DISPENABLE(x)	(((1 << 8) << x) & DISPCNT_DISPENABLE_MASK)

struct gba_lcdfb_priv {
	struct fb_info info;
	struct device_d *dev;
	void __iomem *mmio;
};

static void gba_lcdfb_enable(void __iomem *mmio)
{
	/* Use BG2 in bitmap mode (240x160, 32768 colors) */
	writel(DISPCNT_DISPENABLE(2) | DISPCNT_BGMODE(3), mmio + DISPCNT);
}

static struct fb_ops gba_lcdfb_ops;

static int gba_lcdfb_probe(struct device_d *dev)
{
	struct gba_lcdfb_priv *priv;
	struct fb_info *info;
	void *vram;
	int ret = 0;

	priv = xzalloc(sizeof(*priv));

	priv->dev = dev;

	priv->mmio = dev_request_mem_region_by_name(dev, "mmio");
	if (IS_ERR(priv->mmio))
		return PTR_ERR(priv->mmio);

	vram = (void __force *)dev_request_mem_region_by_name(dev, "vram");
	if (IS_ERR(vram))
		return PTR_ERR(vram);

	info = &priv->info;
	info->priv = priv;
	info->fbops = &gba_lcdfb_ops;

	info->screen_base = vram;
	info->xres = 240;
	info->yres = 160;
	info->bits_per_pixel = 16;

	info->blue.length = info->green.length = info->red.length = 5;
	info->blue.offset = 0;
	info->green.offset = 5;
	info->red.offset = 10;
	gba_lcdfb_enable(priv->mmio);

	ret = register_framebuffer(info);
	if (ret)
		dev_err(dev, "Failed to register framebuffer\n");
	else
		dev_info(dev, "frame buffer registered.\n");

	return ret;
}

static __maybe_unused struct of_device_id gba_lcdfb_compatible[] = {
	{ .compatible = "nintendo,gba-lcdc" },
	{ /* sentinel */ }
};

static struct driver_d gba_lcdfb_driver = {
	.name	= "gba_lcdfb",
	.probe	= gba_lcdfb_probe,
	.of_compatible = DRV_OF_COMPAT(gba_lcdfb_compatible),
};
device_platform_driver(gba_lcdfb_driver);
