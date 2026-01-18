// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2025 Ahmad Fatoum

#define pr_fmt(fmt) "bootm-mock: " fmt

#include <common.h>
#include <bootm.h>
#include <fs.h>
#include <fcntl.h>
#include <init.h>
#include <globalvar.h>
#include <magicvar.h>
#include <libfile.h>
#include <memory.h>
#include <of.h>

#define LASTBOOT_DIR "/tmp/lastboot"

static int bootm_mock_enabled;

static int write_region_to_file(const char *filename, struct resource *res)
{
	if (!res)
		return 0;

	return write_file(filename, (void *)res->start, resource_size(res));
}

static int do_bootm_mock(struct image_data *data)
{
	const struct resource *initrd_res;
	resource_size_t start, end;
	unsigned long load_address;
	void *fdt;
	int ret;

	ret = memory_bank_first_find_space(&start, &end);
	if (ret)
		return ret;

	load_address = PAGE_ALIGN(start);

	ret = bootm_load_os(data, load_address);
	if (ret)
		return ret;

	initrd_res = bootm_load_initrd(data, PAGE_ALIGN(data->os_res->end + 1));
	if (IS_ERR(initrd_res))
		return PTR_ERR(initrd_res);

	fdt = bootm_get_devicetree(data);
	if (IS_ERR(fdt))
		return PTR_ERR(fdt);

	if (data->dryrun) {
		free(fdt);
		return 0;
	}

	ret = make_directory(LASTBOOT_DIR);
	if (ret) {
		pr_err("Failed to create %s: %pe\n", LASTBOOT_DIR, ERR_PTR(ret));
		free(fdt);
		return ret;
	}

	ret = write_region_to_file(LASTBOOT_DIR "/image", data->os_res);
	if (ret) {
		pr_err("Failed to write image: %pe\n", ERR_PTR(ret));
		goto out;
	}

	if (fdt) {
		size_t fdt_size = be32_to_cpu(((struct fdt_header *)fdt)->totalsize);

		ret = write_file(LASTBOOT_DIR "/oftree", fdt, fdt_size);
		if (ret) {
			pr_err("Failed to write oftree: %pe\n", ERR_PTR(ret));
			goto out;
		}
	}

	ret = write_region_to_file(LASTBOOT_DIR "/initrd", data->initrd_res);
	if (ret) {
		pr_err("Failed to write initrd: %pe\n", ERR_PTR(ret));
		goto out;
	}

	pr_info("Boot data written to %s\n", LASTBOOT_DIR);

out:
	free(fdt);
	return ret;
}

static bool bootm_mock_check_image(struct image_handler *handler,
				   struct image_data *data,
				   enum filetype detected_filetype)
{
	return bootm_mock_enabled;
}

static struct image_handler bootm_mock_handler = {
	.name = "Mock image handler",
	.bootm = do_bootm_mock,
	.check_image = bootm_mock_check_image,
};

static int bootm_mock_init(void)
{
	globalvar_add_simple_bool("bootm.mock", &bootm_mock_enabled);
	register_image_handler_head(&bootm_mock_handler);
	return 0;
}
late_initcall(bootm_mock_init);

BAREBOX_MAGICVAR(global.bootm.mock, "Enable mock image handler that writes boot data to /tmp/lastboot/");
