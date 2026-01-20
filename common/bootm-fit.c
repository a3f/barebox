// SPDX-License-Identifier: GPL-2.0-or-later

#include <bootm.h>
#include <image-fit.h>
#include <bootm-fit.h>
#include <memory.h>
#include <zero_page.h>
#include <filetype.h>
#include <fs.h>
#include <libfile.h>

/*
 * bootm_load_fit_os() - load OS from FIT to RAM
 *
 * @data:		image data context
 * @load_address:	The address where the OS should be loaded to
 *
 * This loads the OS to a RAM location. load_address must be a valid
 * address. If the image_data doesn't have a OS specified it's considered
 * an error.
 *
 * Return: 0 on success, negative error code otherwise
 */
int bootm_load_fit_os(struct image_data *data, unsigned long load_address)
{
	const void *kernel = data->fit_kernel;
	unsigned long kernel_size = data->fit_kernel_size;

	/* Check for delayed override (FIT -> non-FIT case) */
	if (IS_ENABLED(CONFIG_BOOT_OVERRIDE) && data->os_file_override) {
		enum filetype fit_kernel_type, override_type;
		void *override_header;
		size_t size;
		int ret;

		/* Detect type of extracted FIT kernel */
		fit_kernel_type = file_detect_boot_image_type(kernel, kernel_size);

		/* Read override file header to detect its type */
		ret = read_file_2(data->os_file_override, &size, &override_header, PAGE_SIZE);
		if (ret < 0 && ret != -EFBIG) {
			pr_err("could not open override image %s: %pe\n",
			       data->os_file_override, ERR_PTR(ret));
			return ret;
		}
		if (size < PAGE_SIZE) {
			pr_err("override image %s too small\n", data->os_file_override);
			free(override_header);
			return -EINVAL;
		}

		override_type = file_detect_boot_image_type(override_header, PAGE_SIZE);
		free(override_header);

		/* Type check unless force mode */
		if (!data->force) {
			if (fit_kernel_type == filetype_unknown && override_type == filetype_unknown) {
				/* Both unknown: allow */
			} else if (fit_kernel_type != override_type) {
				pr_err("Override image file type mismatch: FIT kernel is %s, override '%s' is %s\n",
				       file_type_to_short_string(fit_kernel_type),
				       data->os_file_override,
				       file_type_to_short_string(override_type));
				return -EINVAL;
			}
		}

		/* Load override file instead of FIT kernel */
		data->os_res = file_to_sdram(data->os_file_override, load_address, MEMTYPE_LOADER_CODE);
		if (!data->os_res)
			return -ENOMEM;

		return 0;
	}

	data->os_res = request_sdram_region("kernel",
			load_address, kernel_size,
			MEMTYPE_LOADER_CODE, MEMATTRS_RWX);
	if (!data->os_res)
		return -ENOMEM;

	zero_page_memcpy((void *)load_address, kernel, kernel_size);
	return 0;
}

static int fitconfig_count_ramdisks(struct image_data *data)
{
	int nramdisks;

	nramdisks = fit_count_images(data->os_fit, data->fit_config, "ramdisk");
	if (nramdisks < 0)
		return 0;

	return nramdisks;
}

/*
 * bootm_load_fit_initrd() - load initrd from FIT to RAM
 *
 * @data:		image data context
 * @load_address:	The address where the initrd should be loaded to
 *
 * This loads the initrd to a RAM location. load_address must be a valid
 * address. If the image_data doesn't have a initrd specified this function
 * still returns successful as an initrd is optional.
 *
 * Return: initrd resource on success, NULL if no initrd is present or
 *         an error pointer if an error occurred.
 */
struct resource *bootm_load_fit_initrd(struct image_data *data, unsigned long load_address)
{
	struct resource *res = NULL;
	int nramdisks;

	nramdisks = fitconfig_count_ramdisks(data);

	for (int i = 0; i < nramdisks; i++) {
		const void *initrd;
		unsigned long initrd_size;
		struct resource *tmp;
		int ret;

		ret = fit_open_image(data->os_fit, data->fit_config, "ramdisk", i,
				     &initrd, &initrd_size);
		if (ret) {
			pr_err("Cannot open ramdisk image in FIT image: %pe\n",
					ERR_PTR(ret));
			return ERR_PTR(ret);
		}

		tmp = request_sdram_region("initrd",
					   load_address, initrd_size,
					   MEMTYPE_LOADER_DATA, MEMATTRS_RW);
		if (!tmp)
			return ERR_PTR(-ENOMEM);

		memcpy((void *)load_address, initrd, initrd_size);

		printf("aggregating CPIO %u to %lu\n", i, load_address);

		res = __merge_regions("initrd", res, tmp);
		load_address += initrd_size;
	}

	return res;
}

/*
 * bootm_get_fit_devicetree() - get devicetree
 *
 * @data:		image data context
 *
 * This gets the fixed devicetree from the various image sources or the internal
 * devicetree. It returns a pointer to the allocated devicetree which must be
 * freed after use.
 *
 * Return: pointer to the fixed devicetree, NULL if image_data has an empty DT
 *         or a ERR_PTR() on failure.
 */
void *bootm_get_fit_devicetree(struct image_data *data)
{
	int ret;
	const void *of_tree;
	unsigned long of_size;

	ret = fit_open_image(data->os_fit, data->fit_config, "fdt", 0,
			     &of_tree, &of_size);
	if (ret)
		return ERR_PTR(ret);

	return of_unflatten_dtb(of_tree, of_size);
}

static bool bootm_fit_config_valid(struct fit_handle *fit,
				   struct device_node *config)
{
	/*
	 * Consider only FIT configurations which do provide a loadable kernel
	 * image.
	 */
	return !!fit_has_image(fit, config, "kernel");
}

int bootm_open_fit(struct image_data *data)
{
	struct fit_handle *fit;
	struct fdt_header *header;
	static const char *kernel_img = "kernel";
	size_t flen, hlen;
	int ret;

	header = (struct fdt_header *)data->os_header;
	flen = bootm_get_os_size(data);
	hlen = fdt32_to_cpu(header->totalsize);

	fit = fit_open(data->os_file, data->verbose, data->verify,
		       min(flen, hlen));
	if (IS_ERR(fit)) {
		pr_err("Loading FIT image %s failed with: %pe\n", data->os_file, fit);
		return PTR_ERR(fit);
	}

	data->os_fit = fit;

	data->fit_config = fit_open_configuration(data->os_fit,
						  data->os_part,
						  bootm_fit_config_valid);
	if (IS_ERR(data->fit_config)) {
		pr_err("Cannot open FIT image configuration '%s'\n",
		       data->os_part ? data->os_part : "default");
		return PTR_ERR(data->fit_config);
	}

	ret = fit_open_image(data->os_fit, data->fit_config, kernel_img, 0,
			     &data->fit_kernel, &data->fit_kernel_size);
	if (ret)
		return ret;
	if (data->os_address == UIMAGE_SOME_ADDRESS) {
		ret = fit_get_image_address(data->os_fit,
					    data->fit_config,
					    kernel_img,
					    "load", &data->os_address);
		if (!ret)
			pr_info("Load address from FIT '%s': 0x%lx\n",
				kernel_img, data->os_address);
		/* Note: Error case uses default value. */
	}
	if (data->os_entry == UIMAGE_SOME_ADDRESS) {
		unsigned long entry;
		ret = fit_get_image_address(data->os_fit,
					    data->fit_config,
					    kernel_img,
					    "entry", &entry);
		if (!ret) {
			data->os_entry = entry - data->os_address;
			pr_info("Entry address from FIT '%s': 0x%lx\n",
				kernel_img, entry);
		}
		/* Note: Error case uses default value. */
	}

	return 0;
}
