// SPDX-License-Identifier: GPL-2.0-or-later

#include <bootm.h>
#include <image-fit.h>
#include <bootm-fit.h>
#include <memory.h>
#include <zero_page.h>
#include <filetype.h>
#include <fs.h>
#include <libfile.h>
#include <loadable.h>
#include <malloc.h>

/* Old bootm_load_fit_os() and bootm_load_fit_initrd() functions removed.
 * All loading now goes through the loadable infrastructure. */

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

/**
 * bootm_fit_config_valid - validate FIT configuration for bootm
 * @fit: FIT image handle
 * @config: configuration device node to validate
 *
 * Validation function for FIT configurations used by bootm. A configuration
 * is considered valid for bootm if it contains a kernel image, as this is
 * the minimum requirement for booting.
 *
 * This function is passed to fit_open_configuration() to filter available
 * configurations when opening a FIT image for boot.
 *
 * Return: true if configuration is valid for bootm, false otherwise
 */
bool bootm_fit_config_valid(struct fit_handle *fit,
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

/* === Loadable implementation for FIT images === */

struct fit_loadable_priv {
	struct fit_handle *fit;
	struct device_node *config;
	const char *image_name;
	int index;
};

static int fit_loadable_get_info(struct loadable *l, struct loadable_info *info)
{
	struct fit_loadable_priv *priv = l->priv;
	unsigned long load_addr = UIMAGE_INVALID_ADDRESS;
	unsigned long entry = 0;
	const void *data;
	unsigned long size;
	int ret;

	/* Open image to get size */
	ret = fit_open_image(priv->fit, priv->config, priv->image_name,
			     priv->index, &data, &size);
	if (ret)
		return ret;

	info->size = size;
	info->compressed_size = size; /* fit_open_image already decompresses */
	info->compressed = false;

	/* Try to get load address */
	ret = fit_get_image_address(priv->fit, priv->config,
				     priv->image_name, "load", &load_addr);
	if (!ret)
		info->load_addr = load_addr;
	else
		info->load_addr = UIMAGE_SOME_ADDRESS;

	/* Try to get entry address */
	ret = fit_get_image_address(priv->fit, priv->config,
				     priv->image_name, "entry", &entry);
	if (!ret && UIMAGE_IS_ADDRESS_VALID(load_addr))
		info->entry_offset = entry - load_addr;
	else
		info->entry_offset = 0;

	info->filetype = filetype_unknown;

	return 0;
}

/**
 * fit_loadable_commit - load FIT image data to target address
 * @l: loadable representing FIT image component
 * @load_addr: physical address to load data to
 * @buf_size: size of buffer at load_addr (0 = no limit check)
 *
 * Commits the FIT image component to the specified memory address. This
 * involves:
 * 1. Opening the FIT image to get decompressed data
 * 2. Checking buffer size if buf_size > 0
 * 3. Copying data to target address
 * 4. Registering memory region with request_sdram_region()
 *
 * The FIT data is already decompressed by fit_open_image(), so this just
 * performs a memcpy to the target address.
 *
 * Return: actual number of bytes written on success, negative errno on error
 *         -ENOSPC if buf_size is specified and too small
 *         -ENOMEM if failed to register SDRAM region
 */
static int fit_loadable_commit(struct loadable *l, unsigned long load_addr, size_t buf_size)
{
	struct fit_loadable_priv *priv = l->priv;
	const void *data;
	unsigned long size;
	int ret;
	enum resource_memtype memtype;
	unsigned memattrs;

	/* Open image to get data */
	ret = fit_open_image(priv->fit, priv->config, priv->image_name,
			     priv->index, &data, &size);
	if (ret)
		return ret;

	/* Check if buffer is large enough (if size provided) */
	if (buf_size > 0 && buf_size < size) {
		pr_err("Buffer too small for FIT:%s[%d]: need %lu, have %zu\n",
		       priv->image_name, priv->index, size, buf_size);
		return -ENOSPC;
	}

	/* Determine memory type and attributes based on loadable type */
	switch (l->type) {
	case LOADABLE_KERNEL:
	case LOADABLE_TEE:
		memtype = MEMTYPE_LOADER_CODE;
		memattrs = MEMATTRS_RWX;
		break;
	case LOADABLE_INITRD:
	case LOADABLE_FDT:
	default:
		memtype = MEMTYPE_LOADER_DATA;
		memattrs = MEMATTRS_RW;
		break;
	}

	/* Copy data to target */
	memcpy((void *)load_addr, data, size);

	/* Create resource descriptor */
	l->res = request_sdram_region(l->name, load_addr, size,
				      memtype, memattrs);
	if (!l->res) {
		pr_err("Failed to create resource for FIT:%s[%d]\n",
		       priv->image_name, priv->index);
		return -ENOMEM;
	}

	return size; /* Return actual bytes written */
}

static void fit_loadable_release(struct loadable *l)
{
	struct fit_loadable_priv *priv = l->priv;

	if (priv) {
		free((void *)priv->image_name);
		free(priv);
	}
}

static int fit_loadable_describe(struct loadable *l, char *buf, size_t len)
{
	struct fit_loadable_priv *priv = l->priv;
	struct loadable_info info;
	int ret;

	ret = loadable_get_info(l, &info);
	if (ret)
		return ret;

	return snprintf(buf, len, "FIT:%s[%d] size=%zu load=0x%lx",
			priv->image_name, priv->index, info.size, info.load_addr);
}

static const struct loadable_ops fit_loadable_ops = {
	.get_info = fit_loadable_get_info,
	.commit = fit_loadable_commit,
	.release = fit_loadable_release,
	.describe = fit_loadable_describe,
};

/**
 * loadable_from_fit - create a loadable from FIT image component
 * @fit: opened FIT image handle
 * @config: FIT configuration device node
 * @image_name: name of image in FIT (e.g., "kernel", "ramdisk", "fdt")
 * @index: index for multi-image types (e.g., ramdisk-0, ramdisk-1)
 * @type: type of loadable (LOADABLE_KERNEL, LOADABLE_INITRD, etc.)
 *
 * Creates a loadable structure that wraps access to a component within a
 * FIT image. The loadable uses the FIT handle to access decompressed image
 * data on demand during commit.
 *
 * The created loadable must be freed with loadable_release() when done.
 * The FIT handle itself is managed by the caller and must remain valid
 * until the loadable is released.
 *
 * Return: pointer to allocated loadable on success, ERR_PTR() on error
 */
struct loadable *loadable_from_fit(struct fit_handle *fit,
				   void *config,
				   const char *image_name,
				   int index,
				   enum loadable_type type)
{
	struct loadable *l;
	struct fit_loadable_priv *priv;

	if (!fit || !config || !image_name)
		return ERR_PTR(-EINVAL);

	l = xzalloc(sizeof(*l));
	priv = xzalloc(sizeof(*priv));

	priv->fit = fit;
	priv->config = config;
	priv->image_name = xstrdup(image_name);
	priv->index = index;

	/* Create descriptive name */
	if (index > 0)
		l->name = xasprintf("fit-%s-%d", image_name, index);
	else
		l->name = xasprintf("fit-%s", image_name);
	if (!l->name)
		l->name = image_name;
	l->type = type;
	l->ops = &fit_loadable_ops;
	l->priv = priv;
	INIT_LIST_HEAD(&l->list);

	return l;
}

/**
 * bootm_collect_fit_loadables - create loadables from opened FIT image
 * @data: image data context with opened FIT image
 *
 * Creates loadable structures for all boot components found in the opened
 * FIT image configuration. This includes:
 * * Kernel from "kernel" image
 * * Initrd(s) from "ramdisk" images (supports multiple ramdisks)
 * * FDT from "fdt" image if present
 * * TEE from "tee" image if present
 *
 * Each loadable wraps access to the FIT image data and is added to
 * data->loadables list. Appropriate shortcuts (data->kernel, data->fdt,
 * data->tee) are set. The loadables are not yet committed to memory - that
 * happens later during bootm_load_os/bootm_load_initrd.
 *
 * Requires: data->os_fit and data->fit_config must be already opened
 * Context: Called during boot preparation for FIT image boots
 */
void bootm_collect_fit_loadables(struct image_data *data)
{
	struct loadable *l;
	int nramdisks, i;

	if (!IS_ENABLED(CONFIG_BOOTM_FITIMAGE) || !data->os_fit)
		return;

	/* Create kernel loadable */
	if (data->fit_kernel) {
		l = loadable_from_fit(data->os_fit, data->fit_config,
				      "kernel", 0, LOADABLE_KERNEL);
		if (!IS_ERR(l)) {
			data->kernel = l;
			list_add_tail(&l->list, &data->loadables);
		}
	}

	/* Create initrd loadable(s) */
	nramdisks = fit_count_images(data->os_fit, data->fit_config, "ramdisk");
	for (i = 0; i < nramdisks; i++) {
		l = loadable_from_fit(data->os_fit, data->fit_config,
				      "ramdisk", i, LOADABLE_INITRD);
		if (!IS_ERR(l)) {
			/* Add to main loadables list only */
			list_add_tail(&l->list, &data->loadables);
		}
	}

	/* Create FDT loadable if present */
	if (fit_has_image(data->os_fit, data->fit_config, "fdt")) {
		l = loadable_from_fit(data->os_fit, data->fit_config,
				      "fdt", 0, LOADABLE_FDT);
		if (!IS_ERR(l)) {
			data->fdt = l;
			list_add_tail(&l->list, &data->loadables);
		}
	}

	/* Create TEE loadable */
	if (fit_has_image(data->os_fit, data->fit_config, "tee")) {
		l = loadable_from_fit(data->os_fit, data->fit_config,
				      "tee", 0, LOADABLE_TEE);
		if (!IS_ERR(l)) {
			data->tee = l;
			list_add_tail(&l->list, &data->loadables);
		}
	}
}
