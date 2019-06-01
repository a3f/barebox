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
	void __iomem *mmio;
	void *vram;
};

static int gba_lcdfb_activate_var(struct fb_info *info)
{
	struct gba_lcdfb_priv *priv = info->priv;
	struct fb_videomode *mode = info->mode;

	if (info->bits_per_pixel != 16 || mode->xres != 240 || mode->yres != 160) {
		dev_err(&info->dev, "only 240x160@XR15 supported\n");
		return -EINVAL;
	}

	/* Use BG2 in bitmap mode (240x160, 32768 colors) */
	writel(DISPCNT_DISPENABLE(2) | DISPCNT_BGMODE(3), priv->mmio + DISPCNT);

	info->blue.length = info->green.length = info->red.length = 5;
	info->red.length = 0;
	info->green.offset = 10;
	info->blue.offset = 15;

	info->screen_base = priv->vram;

	return 0;
}

static struct fb_ops gba_lcdfb_ops = {
	.fb_activate_var = gba_lcdfb_activate_var,
};

static int gba_lcdfb_probe(struct device_d *dev)
{
	struct gba_lcdfb_priv *priv;
	struct fb_info *info;
	int ret = 0;

	priv = xzalloc(sizeof(*priv));

	priv->mmio = dev_request_mem_region_by_name(dev, "mmio");
	if (IS_ERR(priv->mmio))
		return PTR_ERR(priv->mmio);

	priv->vram = (void __force *)dev_request_mem_region_by_name(dev, "vram");
	if (IS_ERR(priv->vram))
		return PTR_ERR(priv->vram);

	info = &priv->info;
	info->priv = priv;
	info->fbops = &gba_lcdfb_ops;

	ret = register_framebuffer(info);
	if (ret != 0)
		dev_err(dev, "Failed to register framebuffer\n");

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
