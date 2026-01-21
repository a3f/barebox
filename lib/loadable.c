// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <loadable.h>
#include <malloc.h>
#include <memory.h>
#include <libfile.h>
#include <unistd.h>
#include <linux/stat.h>

/**
 * loadable_get_info - wrapper that caches info
 */
int loadable_get_info(struct loadable *l, struct loadable_info *info)
{
	int ret;

	if (!l->info_valid) {
		if (!l->ops || !l->ops->get_info)
			return -ENOSYS;

		ret = l->ops->get_info(l, &l->info);
		if (ret)
			return ret;
		l->info_valid = true;
	}

	*info = l->info;
	return 0;
}

/**
 * loadable_commit - load/decompress to target address
 */
int loadable_commit(struct loadable *l, unsigned long load_addr, size_t size)
{
	if (!l->ops || !l->ops->commit)
		return -ENOSYS;

	return l->ops->commit(l, load_addr, size);
}

/**
 * loadable_release - free resources associated with this loadable
 */
void loadable_release(struct loadable *l)
{
	if (!l)
		return;

	if (l->ops && l->ops->release)
		l->ops->release(l);

	/* Note: l->res is NOT freed here, it's managed by caller */
	free(l);
}

/* === File-based loadable implementation === */

struct file_loadable_priv {
	char *path;
};

static int file_loadable_get_info(struct loadable *l, struct loadable_info *info)
{
	struct file_loadable_priv *priv = l->priv;
	struct stat s;
	int ret;

	ret = stat(priv->path, &s);
	if (ret)
		return ret;

	info->size = s.st_size;
	info->compressed_size = s.st_size;
	info->compressed = false;
	info->load_addr = UIMAGE_SOME_ADDRESS;
	info->entry_offset = 0;
	info->filetype = filetype_unknown;

	return 0;
}

static int file_loadable_commit(struct loadable *l, unsigned long load_addr, size_t size)
{
	struct file_loadable_priv *priv = l->priv;
	enum resource_memtype memtype;
	unsigned memattrs;
	struct loadable_info info;
	ssize_t ret;

	/* Get file size */
	ret = loadable_get_info(l, &info);
	if (ret)
		return ret;

	/* Check if buffer is large enough (if size provided) */
	if (size > 0 && size < info.size) {
		pr_err("Buffer too small for %s: need %zu, have %zu\n",
		       priv->path, info.size, size);
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

	/* Read file to provided address */
	ret = read_file_into_buf(priv->path, (void *)load_addr, size > 0 ? size : info.size);
	if (ret < 0)
		return ret;

	/* Create resource descriptor */
	l->res = request_sdram_region(l->name, load_addr, ret,
				      memtype, memattrs);
	if (!l->res) {
		pr_err("Failed to create resource for %s\n", priv->path);
		return -ENOMEM;
	}

	return ret; /* Actual bytes read */
}

static void file_loadable_release(struct loadable *l)
{
	struct file_loadable_priv *priv = l->priv;

	if (priv) {
		free(priv->path);
		free(priv);
	}
}

static int file_loadable_describe(struct loadable *l, char *buf, size_t len)
{
	struct file_loadable_priv *priv = l->priv;
	struct loadable_info info;
	int ret;

	ret = loadable_get_info(l, &info);
	if (ret)
		return ret;

	return snprintf(buf, len, "file:%s size=%zu", priv->path, info.size);
}

static const struct loadable_ops file_loadable_ops = {
	.get_info = file_loadable_get_info,
	.commit = file_loadable_commit,
	.release = file_loadable_release,
	.describe = file_loadable_describe,
};

struct loadable *loadable_from_file(const char *path, enum loadable_type type)
{
	struct loadable *l;
	struct file_loadable_priv *priv;

	if (!path || !*path)
		return ERR_PTR(-EINVAL);

	l = xzalloc(sizeof(*l));
	priv = xzalloc(sizeof(*priv));

	priv->path = xstrdup(path);

	l->name = basprintf("file-%s", path);
	if (!l->name)
		l->name = path;
	l->type = type;
	l->ops = &file_loadable_ops;
	l->priv = priv;
	INIT_LIST_HEAD(&l->list);

	return l;
}
