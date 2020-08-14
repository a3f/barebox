// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyright-Text: 2010 Andy Grundman

#include <common.h>
#include <gui/graphic_utils.h>
#include <linux/bitfield.h>
#include "real.h"

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
	real_t width_scale, height_scale;
	int x, y;

	pr_debug("Resizing from %d x %d -> %d x %d\n",
		 orig_width, orig_height, info->xres, info->yres);

	if (orig_width == info->xres && orig_height == info->yres) {
		memcpy(info->screen_base, info->screen_base_shadow,
		       info->line_length * info->yres);
		return 0;
	}

	width_scale = real_div(int_to_real(orig_width), int_to_real(info->xres));
	height_scale = real_div(int_to_real(orig_height), int_to_real(info->yres));

	for (y = 0; y < info->yres; y++) {
		real_t sy1, sy2;

		sy1 = real_mul(int_to_real(y), height_scale);
		sy2 = real_mul(int_to_real(y + 1), height_scale);

		for (x = 0; x < info->xres; x++) {
			real_t sx1, sx2;
			real_t sx, sy;
			real_t spixels = 0;
			real_t red = 0, green = 0, blue = 0;

			sx1 = real_mul(int_to_real(x), width_scale);
			sx2 = real_mul(int_to_real(x + 1), width_scale);
			sy = sy1;

			do {
				real_t yportion;

				if (real_floor(sy) == real_floor(sy1)) {
					yportion = REAL_C(1) - (sy - real_floor(sy));
					if (yportion > sy2 - sy1) {
						yportion = sy2 - sy1;
					}
					sy = real_floor(sy);
				}
				else if (sy == real_floor(sy2)) {
					yportion = sy2 - real_floor(sy2);
				}
				else {
					yportion = REAL_C(1);
				}

				sx = sx1;

				do {
					u8 r, g, b;
					real_t xportion;
					real_t pcontribution;

					if (real_floor(sx) == real_floor(sx1)) {
						xportion = REAL_C(1) - (sx - real_floor(sx));
						if (xportion > sx2 - sx1)	{
							xportion = sx2 - sx1;
						}
						sx = real_floor(sx);
					}
					else if (sx == real_floor(sx2)) {
						xportion = sx2 - real_floor(sx2);
					}
					else {
						xportion = REAL_C(1);
					}

					pcontribution = real_mul(xportion, yportion);

					get_pixel(info, real_to_int(sx), real_to_int(sy),
						  &r, &g, &b);

					red   += real_mul(int_to_real(r), pcontribution);
					green += real_mul(int_to_real(g), pcontribution);
					blue  += real_mul(int_to_real(b), pcontribution);

					spixels += pcontribution;
					sx += REAL_C(1);
				} while (sx < sx2);

				sy += REAL_C(1);
			} while (sy < sy2);

#if 0
			// If rgba get too large for the fixed-point representation, fallback to the floating point routine
			// This should only happen with very large images
			if (red < 0 || green < 0 || blue < 0) {
				pr_warn("fixed-point overflow: %d %d %d\n", red, green, blue);
				return -EDOM;
			}
#endif

			if (spixels != 0) {
				spixels = real_div(REAL_C(1), spixels);

				red	= real_mul(red, spixels);
				green	= real_mul(green, spixels);
				blue	= real_mul(blue, spixels);
			}

			/* Clamping to allow for rounding errors above */
			red = min(red, REAL_C(255));
			green = min(green, REAL_C(255));
			blue = min(blue, REAL_C(255));

			pr_debug("  -> @(%d, %d) = %x (%d,%d,%d)\n",
				 x, y, COL(real_to_int(red), real_to_int(green), real_to_int(blue)),
				 real_to_int(red), real_to_int(green), real_to_int(blue));

			put_rgb_pixel(info, x, y,
				real_round(red), real_round(green), real_round(blue));
		}
	}

	return 0;
}
