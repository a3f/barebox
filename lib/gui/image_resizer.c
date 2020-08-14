// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyright-Text: 2010 Andy Grundman

#include <common.h>
#include <gui/graphic_utils.h>
#include <linux/bitfield.h>
#include "fixed.h"

#define RED_OFFSET	16
#define GREEN_OFFSET	8
#define BLUE_OFFSET	0

#define COL(red, green, blue) \
	(((red) << RED_OFFSET) | ((green) << GREEN_OFFSET) | ((blue) << BLUE_OFFSET))
#define COL_RED(col)	((col >> RED_OFFSET) & 0xFF)
#define COL_GREEN(col)	((col >> GREEN_OFFSET) & 0xFF)
#define COL_BLUE(col)	((col >> BLUE_OFFSET) & 0xFF)

static inline void get_pixel(struct fb_info *info, s32 x, s32 y,
			     u8 *r, u8 *g, u8 *b)
{
	u32 *pixbuf = info->screen_base_shadow;
	gu_get_rgb_pixel(info, &pixbuf[(y * info->xres) + x],
		      r , g, b);
}

static inline void put_rgb_pixel(struct fb_info *info, s32 x, s32 y,
				 u8 r, u8 g, u8 b)
{
	u32 *outbuf = info->screen_base;
	gu_set_rgb_pixel(info, &outbuf[(y * info->xres) + x],
			 r, g, b);
}

int gu_screen_blit_resized(struct screen *sc,
			   int orig_width, int orig_height)
{
	struct fb_info *info = sc->info;
	fixed_t width_scale, height_scale;
	int x, y;

	pr_debug("Resizing from %d x %d -> %d x %d\n",
		 orig_width, orig_height, info->xres, info->yres);

	if (orig_width == info->xres && orig_height == info->yres) {
		memcpy(info->screen_base, info->screen_base_shadow,
		       info->line_length * info->yres);
		return 0;
	}

	width_scale = fixed_div(int_to_fixed(orig_width), int_to_fixed(info->xres));
	height_scale = fixed_div(int_to_fixed(orig_height), int_to_fixed(info->yres));

	for (y = 0; y < info->yres; y++) {
		fixed_t sy1, sy2;

		sy1 = fixed_mul(int_to_fixed(y), height_scale);
		sy2 = fixed_mul(int_to_fixed(y + 1), height_scale);

		for (x = 0; x < info->xres; x++) {
			fixed_t sx1, sx2;
			fixed_t sx, sy;
			fixed_t spixels = 0;
			fixed_t red = 0, green = 0, blue = 0;

			sx1 = fixed_mul(int_to_fixed(x), width_scale);
			sx2 = fixed_mul(int_to_fixed(x + 1), width_scale);
			sy = sy1;

			do {
				fixed_t yportion;

				if (fixed_floor(sy) == fixed_floor(sy1)) {
					yportion = FIXED_1 - (sy - fixed_floor(sy));
					if (yportion > sy2 - sy1) {
						yportion = sy2 - sy1;
					}
					sy = fixed_floor(sy);
				}
				else if (sy == fixed_floor(sy2)) {
					yportion = sy2 - fixed_floor(sy2);
				}
				else {
					yportion = FIXED_1;
				}

				sx = sx1;

				do {
					u8 r, g, b;
					fixed_t xportion;
					fixed_t pcontribution;

					if (fixed_floor(sx) == fixed_floor(sx1)) {
						xportion = FIXED_1 - (sx - fixed_floor(sx));
						if (xportion > sx2 - sx1)	{
							xportion = sx2 - sx1;
						}
						sx = fixed_floor(sx);
					}
					else if (sx == fixed_floor(sx2)) {
						xportion = sx2 - fixed_floor(sx2);
					}
					else {
						xportion = FIXED_1;
					}

					pcontribution = fixed_mul(xportion, yportion);

					get_pixel(info, fixed_to_int(sx), fixed_to_int(sy),
						  &r, &g, &b);

					red   += fixed_mul(int_to_fixed(r), pcontribution);
					green += fixed_mul(int_to_fixed(g), pcontribution);
					blue  += fixed_mul(int_to_fixed(b), pcontribution);

					spixels += pcontribution;
					sx += FIXED_1;
				} while (sx < sx2);

				sy += FIXED_1;
			} while (sy < sy2);

			// If rgba get too large for the fixed-point representation, fallback to the floating point routine
			// This should only happen with very large images
			if (red < 0 || green < 0 || blue < 0) {
				pr_warn("fixed-point overflow: %d %d %d\n", red, green, blue);
				return -EDOM;
			}

			if (spixels != 0) {
				spixels = fixed_div(FIXED_1, spixels);

				red	= fixed_mul(red, spixels);
				green	= fixed_mul(green, spixels);
				blue	= fixed_mul(blue, spixels);
			}

			/* Clamping to allow for rounding errors above */
			red = min(red, FIXED_255);
			green = min(green, FIXED_255);
			blue = min(blue, FIXED_255);

			pr_debug("  -> @(%d, %d) = %x (%d,%d,%d)\n",
				 x, y, COL(fixed_to_int(red), fixed_to_int(green), fixed_to_int(blue)),
				 fixed_to_int(red), fixed_to_int(green), fixed_to_int(blue));

			put_rgb_pixel(info, x, y,
				fixed_to_int(red), fixed_to_int(green), fixed_to_int(blue));
		}
	}

	return 0;
}
