// SPDX-License-Identifier: GPL-2.0-or-later

#include <bootm.h>
#include <bootm-uimage.h>
#include <linux/kstrtox.h>
#include <loadable.h>
#include <malloc.h>
#include <linux/ioport.h>

static int uimage_part_num(const char *partname)
{
	if (!partname)
		return 0;
	return simple_strtoul(partname, NULL, 0);
}

/* Old bootm_load_uimage_os(), bootm_open_initrd_uimage(), and
 * bootm_load_uimage_initrd() functions removed.
 * All loading now goes through the loadable infrastructure. */

int bootm_open_oftree_uimage(struct image_data *data, size_t *size,
			     struct fdt_header **fdt)
{
	enum filetype ft;
	const char *oftree = data->oftree_file;
	int num = uimage_part_num(data->oftree_part);
	struct uimage_handle *of_handle;
	int release = 0;

	pr_info("Loading devicetree from '%s'@%d\n", oftree, num);

	if (!strcmp(data->os_file, oftree)) {
		of_handle = data->os;
	} else if (!strcmp(data->initrd_files, oftree)) {
		of_handle = data->initrd;
	} else {
		of_handle = uimage_open(oftree);
		if (!of_handle)
			return -ENODEV;
		uimage_print_contents(of_handle);
		release = 1;
	}

	*fdt = uimage_load_to_buf(of_handle, num, size);

	if (release)
		uimage_close(of_handle);

	ft = file_detect_type(*fdt, *size);
	if (ft != filetype_oftree) {
		pr_err("%s is not an oftree but %s\n",
			data->oftree_file, file_type_to_string(ft));
		free(*fdt);
		return -EINVAL;
	}

	return 0;
}

int bootm_open_uimage(struct image_data *data)
{
	int ret;

	data->os = uimage_open(data->os_file);
	if (!data->os)
		return -EINVAL;

	if (bootm_get_verify_mode() > BOOTM_VERIFY_NONE) {
		ret = uimage_verify(data->os);
		if (ret) {
			pr_err("Checking data crc failed with %pe\n",
					ERR_PTR(ret));
			return ret;
		}
	}

	uimage_print_contents(data->os);

	if (IH_ARCH == IH_ARCH_INVALID || data->os->header.ih_arch != IH_ARCH) {
		pr_err("Unsupported Architecture 0x%x\n",
		       data->os->header.ih_arch);
		return -EINVAL;
	}

	if (data->os_address == UIMAGE_SOME_ADDRESS)
		data->os_address = data->os->header.ih_load;

	return 0;
}

void bootm_close_uimage(struct image_data *data)
{
	if (data->initrd && data->initrd != data->os)
		uimage_close(data->initrd);
	uimage_close(data->os);
}

/* === Loadable implementation for uImage === */

struct uimage_loadable_priv {
	struct uimage_handle *handle;
	int part_num;
};

static int uimage_loadable_get_info(struct loadable *l, struct loadable_info *info)
{
	struct uimage_loadable_priv *priv = l->priv;
	struct uimage_handle *handle = priv->handle;
	resource_size_t size;

	/* Get size from uImage header */
	size = uimage_get_size(handle, priv->part_num);
	if (size == 0)
		return -EINVAL;

	info->size = size;
	info->compressed_size = size; /* uimage_load_to_sdram handles decompression */
	info->compressed = false;

	/* Get load address from uImage header */
	info->load_addr = handle->header.ih_load;
	info->entry_offset = handle->header.ih_ep - handle->header.ih_load;

	info->filetype = filetype_unknown;

	return 0;
}

static int uimage_loadable_commit(struct loadable *l, unsigned long load_addr)
{
	struct uimage_loadable_priv *priv = l->priv;

	/* Load uImage to target address */
	l->res = uimage_load_to_sdram(priv->handle, priv->part_num, load_addr);
	if (!l->res)
		return -ENOMEM;

	return 0;
}

static void uimage_loadable_release(struct loadable *l)
{
	struct uimage_loadable_priv *priv = l->priv;

	if (priv) {
		/* Note: uimage_handle is managed by bootm code, not by loadable */
		free(priv);
	}
}

static int uimage_loadable_describe(struct loadable *l, char *buf, size_t len)
{
	struct uimage_loadable_priv *priv = l->priv;
	struct loadable_info info;
	int ret;

	ret = loadable_get_info(l, &info);
	if (ret)
		return ret;

	return snprintf(buf, len, "uImage[%d] size=%zu load=0x%lx",
			priv->part_num, info.size, info.load_addr);
}

static const struct loadable_ops uimage_loadable_ops = {
	.get_info = uimage_loadable_get_info,
	.commit = uimage_loadable_commit,
	.release = uimage_loadable_release,
	.describe = uimage_loadable_describe,
};

struct loadable *loadable_from_uimage(struct uimage_handle *uimage,
				      int part_num,
				      enum loadable_type type)
{
	struct loadable *l;
	struct uimage_loadable_priv *priv;

	if (!uimage)
		return ERR_PTR(-EINVAL);

	l = xzalloc(sizeof(*l));
	priv = xzalloc(sizeof(*priv));

	priv->handle = uimage;
	priv->part_num = part_num;

	/* Create descriptive name */
	if (part_num > 0)
		l->name = basprintf("uimage-%d", part_num);
	else
		l->name = xstrdup("uimage");

	l->type = type;
	l->ops = &uimage_loadable_ops;
	l->priv = priv;
	INIT_LIST_HEAD(&l->list);

	return l;
}


/*
 * bootm_collect_uimage_loadables - collect loadables from a uImage
 *
 * This creates loadable structures for kernel and initrd from the opened
 * uImage handles. The loadables are added to data->loadables list but not
 * yet committed to memory.
 */
void bootm_collect_uimage_loadables(struct image_data *data)
{
	struct loadable *l;
	int part_num;

	/* Create kernel loadable from opened uImage */
	if (data->os) {
		/* Convert part string to number (default 0 if not specified) */
		part_num = data->os_part ? simple_strtoul(data->os_part, NULL, 0) : 0;
		l = loadable_from_uimage(data->os, part_num, LOADABLE_KERNEL);
		if (!IS_ERR(l)) {
			data->kernel = l;
			list_add_tail(&l->list, &data->loadables);
		}
	}

	/* Open and create initrd loadable if initrd_files is specified */
	if (data->initrd_files && *data->initrd_files) {
		/* Check if we need to open a separate initrd uImage */
		if (!data->initrd || strcmp(data->os_file, data->initrd_files)) {
			data->initrd = uimage_open(data->initrd_files);
			if (!data->initrd) {
				pr_err("Cannot open initrd uImage: %s\n", data->initrd_files);
				return;
			}

			if (bootm_get_verify_mode() > BOOTM_VERIFY_NONE) {
				int ret = uimage_verify(data->initrd);
				if (ret) {
					pr_err("Checking initrd data crc failed with %pe\n",
						ERR_PTR(ret));
					uimage_close(data->initrd);
					data->initrd = NULL;
					return;
				}
			}
			uimage_print_contents(data->initrd);
		} else {
			/* Initrd is in the same file as OS */
			data->initrd = data->os;
		}

		/* Create initrd loadable */
		part_num = data->initrd_part ? simple_strtoul(data->initrd_part, NULL, 0) : 0;
		l = loadable_from_uimage(data->initrd, part_num, LOADABLE_INITRD);
		if (!IS_ERR(l))
			list_add_tail(&l->list, &data->loadables);
	}

	/* FDT and TEE not commonly used in uImage format */
}
