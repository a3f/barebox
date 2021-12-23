#define pr_fmt(fmt) "bootm-rk30: " fmt

#include <bootm.h>
#include <common.h>
#include <init.h>
#include <memory.h>
#include <linux/sizes.h>
#include <crypto/rc4.h>
#include <mach/rockchip/rkimage-rk30.h>

static int do_bootm_rk30_barebox_image(struct image_data *data)
{
	void (*barebox)(unsigned long x0, unsigned long x1, unsigned long x2,
			unsigned long x3);
	unsigned init_offset, init_size, boot_offset, boot_size;
	struct rkheader0_info *hdr0 = data->os_header;
	resource_size_t start, end, image_size;
	void *image;
	int ret;

	ret = memory_bank_first_find_space(&start, &end);
	if (ret)
		return ret;

	rk_bootrom_rc4_encode(hdr0, sizeof(*hdr0));

	init_offset = hdr0->init_offset * RK_BLK_SIZE;
	init_size = hdr0->init_size * RK_BLK_SIZE;
	boot_offset = init_offset + hdr0->init_size * RK_BLK_SIZE;
	boot_size = hdr0->init_boot_size * RK_BLK_SIZE - init_size;

	/*
	 * start is normally at least 4K aligned here, but rk boot
	 * images are only 2K aligned. Boot image can contain adrp
	 * instruction assuming 4k text alignment, so we must adjust
	 * it here. On rk3399, 4k aligment was observed to fix wrong
	 * adrp in relocate_to_current_adr. Adjusting to SZ_64K shifts
	 * the issue to LZO decompression ("Compressed data violation").
	 * SZ_1M seems to work. TODO investigate.
	 */
	start = ALIGN(start + boot_offset, SZ_1M) - boot_offset;

	ret = bootm_load_os(data, start);
	if (ret)
		return ret;

	image = (void *)data->os_res->start;
	image_size = resource_size(data->os_res);

	if (image_size < boot_offset + boot_size)
		return -EINVAL;

	if (data->verbose) {
		struct rkheader1_info *hdr1 = image + init_offset;
		if (!hdr0->disable_rc4)
			rk_bootrom_rc4_encode(hdr1, sizeof(*hdr1));

		printf("Skipping initial %.4s image, booting from 0x%x+0x%x\n",
		       (char *)&hdr1->magic, boot_offset, boot_size);
	}

	barebox = image + boot_offset;

	if (!hdr0->disable_rc4)
		rk_bootrom_rc4_encode(barebox, boot_size);

	printf("Starting barebox image at 0x%p\n", barebox);

	shutdown_barebox();

	barebox(0, 0, 0, 0);

	return -EINVAL;
}

static struct image_handler image_handler_rk30_barebox_image = {
	.name = "Rockchip RK30 barebox image",
	.bootm = do_bootm_rk30_barebox_image,
	.filetype = filetype_rockchip_rk30_image,
};

static int rockchip_register_rk30_barebox_image_handler(void)
{
	return register_image_handler(&image_handler_rk30_barebox_image);
}
late_initcall(rockchip_register_rk30_barebox_image_handler);
