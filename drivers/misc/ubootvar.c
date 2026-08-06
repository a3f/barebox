// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * U-Boot environment vriable blob driver
 *
 * Copyright (C) 2019 Zodiac Inflight Innovations
 */

#include <common.h>
#include <init.h>
#include <io.h>
#include <of.h>
#include <malloc.h>
#include <envfs.h>
#include <fs.h>
#include <libfile.h>
#include <command.h>
#include <crc.h>
#include <ubootvar.h>
#include <unistd.h>

enum ubootvar_flag_scheme {
	FLAG_NONE,
	FLAG_BOOLEAN,
	FLAG_INCREMENTAL,
};

struct ubootvar_data {
	struct cdev cdev;
	char *path[2];
	char *default_env_path;
	bool crc_invalid;
	bool current;
	uint8_t flag;
	int count;
	void *data;
	size_t size;
	struct list_head list;
};

struct ubootvar_copy {
	void *blob;
	uint8_t *data;
	size_t size;
	uint8_t flag;
	bool usable;
	bool crc_ok;
};

static LIST_HEAD(ubootvar_list);

static int ubootvar_flush(struct cdev *cdev)
{
	struct device *dev = cdev->dev;
	struct ubootvar_data *ubdata = dev->priv;
	const char *path = ubdata->path[!ubdata->current];
	uint32_t crc = 0xffffffff;
	resource_size_t size;
	const void *data;
	int fd, ret = 0;

	fd = open(path, O_WRONLY);
	if (fd < 0) {
		dev_err(dev, "Failed to open %s\n", path);
		return -errno;
	}
	/*
	 * FIXME: This code needs to do a proper protect/unprotect and
	 * erase calls to work on MTD devices
	 */

	/*
	 * Write a dummy CRC first as a way of invalidating the
	 * environment in case we fail mid-flushing
	 */
	if (write_full(fd, &crc, sizeof(crc)) != sizeof(crc)) {
		dev_err(dev, "Failed to write dummy CRC\n");
		ret = -errno;
		goto close_fd;
	}

	if (ubdata->count > 1) {
		/*
		 * FIXME: This assumes FLAG_INCREMENTAL
		 */
		const uint8_t flag = ++ubdata->flag;

		if (write_full(fd, &flag, sizeof(flag)) != sizeof(flag)) {
			dev_dbg(dev, "Failed to write flag\n");
			ret = -errno;
			goto close_fd;
		}
	}

	data = (const void *)ubdata->data;
	size = ubdata->size;

	/*
	 * Write out and flush all of the new environment data
	 */
	if (write_full(fd, data, size) != size) {
		dev_dbg(dev, "Failed to write data\n");
		ret = -errno;
		goto close_fd;
	}

	if (flush(fd)) {
		dev_dbg(dev, "Failed to flush written data\n");
		ret = -errno;
		goto close_fd;
	}
	/*
	 * Now that all of the environment data is out, we can go back
	 * to the start of the block and write correct CRC, to finish
	 * the processs.
	 */
	if (lseek(fd, 0, SEEK_SET) != 0) {
		dev_dbg(dev, "lseek() failed\n");
		ret = -errno;
		goto close_fd;
	}

	crc = crc32(0, data, size);
	if (write_full(fd, &crc, sizeof(crc)) != sizeof(crc)) {
		dev_dbg(dev, "Failed to write valid CRC\n");
		ret = -errno;
		goto close_fd;
	}
	/*
	 * Now that we've successfully written new environment blob
	 * out, switch current partition.
	 */
	ubdata->current = !ubdata->current;

close_fd:
	close(fd);
	return ret;
}

static ssize_t
ubootvar_read(struct cdev *cdev, void *buf, size_t count, loff_t offset,
	      unsigned long flags)
{
	struct device *dev = cdev->dev;
	struct ubootvar_data *ubdata = dev->priv;

	WARN_ON(flags & O_RWSIZE_MASK);

	memcpy(buf, ubdata->data + offset, count);

	return count;
}

static ssize_t
ubootvar_write(struct cdev *cdev, const void *buf, size_t count,
	       loff_t offset, unsigned long flags)
{
	struct device *dev = cdev->dev;
	struct ubootvar_data *ubdata = dev->priv;

	WARN_ON(flags & O_RWSIZE_MASK);

	memcpy(ubdata->data + offset, buf, count);

	return count;
}

static int ubootvar_memmap(struct cdev *cdev, void **map, int flags)
{
	struct device *dev = cdev->dev;
	struct ubootvar_data *ubdata = dev->priv;

	*map = ubdata->data;

	return 0;
}

static struct cdev_operations ubootvar_ops = {
	.read = ubootvar_read,
	.write = ubootvar_write,
	.memmap = ubootvar_memmap,
	.flush = ubootvar_flush,
};

static void ubootenv_info(struct device *dev)
{
	struct ubootvar_data *ubdata = dev->priv;

	printf("Current environment copy: %s\n",
	       ubdata->path[ubdata->current]);
}

static void ubootvar_parse_copy(struct ubootvar_copy *copy, void *blob,
				size_t size, bool redundant)
{
	uint32_t crc;
	uint8_t *data = blob;
	size_t header_size = sizeof(uint32_t);

	if (redundant)
		header_size += sizeof(uint8_t);

	if (size < header_size)
		return;

	memcpy(&crc, data, sizeof(crc));
	data += sizeof(crc);
	size -= sizeof(crc);

	if (redundant) {
		copy->flag = *data;
		data++;
		size--;
	}

	copy->data = data;
	copy->size = size;
	copy->usable = true;
	copy->crc_ok = crc32(0, data, size) == crc;
}

static int ubootvar_select_copy(struct device *dev, struct ubootvar_data *ubdata,
				struct ubootvar_copy *copy, int count,
				enum ubootvar_flag_scheme flag_scheme)
{
	unsigned int crc_ok = 0;
	int current, i;

	for (i = 0; i < count; i++) {
		if (copy[i].crc_ok)
			crc_ok |= BIT(i);
	}

	switch (crc_ok) {
	case 0b00:
		current = -EINVAL;

		for (i = 0; i < count; i++) {
			if (copy[i].usable) {
				current = i;
				break;
			}
		}

		if (current < 0) {
			dev_err(dev, "No readable U-Boot environment data found\n");
			return current;
		}

		memset(copy[current].data, 0, copy[current].size);
		ubdata->crc_invalid = true;
		dev_info(dev, "No good partitions found, creating an empty one\n");
		break;
	case 0b11:
		/*
		 * Both partitions are valid, so we need to examine flags to
		 * determine which one to use as current.
		 */
		switch (flag_scheme) {
		case FLAG_INCREMENTAL:
			if ((copy[0].flag == 0xff && copy[1].flag == 0) ||
			    (copy[1].flag == 0xff && copy[0].flag == 0)) {
				/*
				 * When flag overflow happens current
				 * partition is the one whose counter reached
				 * zero first.
				 */
				current = copy[1].flag == 0;
			} else {
				current = copy[1].flag > copy[0].flag;
			}
			break;
		default:
			dev_err(dev, "Unknown flag scheme %u\n", flag_scheme);
			return -EINVAL;
		}
		break;
	default:
		/*
		 * Only one partition is valid, so the choice of the current
		 * one is obvious. This deliberately wins over builtin default
		 * fallback.
		 */
		current = __ffs(crc_ok);
		break;
	}

	ubdata->data = copy[current].data;
	ubdata->size = copy[current].size;
	ubdata->current = current;
	ubdata->count = count;
	ubdata->flag = copy[current].flag;

	return current;
}

static int ubootvar_apply_default_blob(struct ubootvar_data *ubdata,
				       const void *blob, size_t file_size)
{
	size_t data_size;

	if (file_size <= sizeof(uint32_t))
		return -EINVAL;

	data_size = min(file_size - sizeof(uint32_t), ubdata->size);
	memset(ubdata->data, 0, ubdata->size);
	memcpy(ubdata->data, (const uint8_t *)blob + sizeof(uint32_t),
	       data_size);

	return 0;
}

#if IS_ENABLED(CONFIG_SELFTEST_UBOOTVAR)
int ubootvar_apply_blobs(const void * const blob[2],
				const size_t size[2], int count,
				const void *default_blob, size_t default_size,
				void **out, size_t *out_size)
{
	struct ubootvar_copy copy[2] = {};
	struct ubootvar_data ubdata = {};
	bool redundant = count > 1;
	int current, i, ret = 0;

	if (!out || !out_size || count < 1 || count > 2)
		return -EINVAL;

	*out = NULL;
	*out_size = 0;

	for (i = 0; i < count; i++) {
		if (!blob[i])
			continue;

		copy[i].blob = memdup(blob[i], size[i]);
		if (!copy[i].blob) {
			ret = -ENOMEM;
			goto out;
		}

		ubootvar_parse_copy(&copy[i], copy[i].blob, size[i],
				    redundant);
	}

	current = ubootvar_select_copy(NULL, &ubdata, copy, count,
				       FLAG_INCREMENTAL);
	if (current < 0) {
		ret = current;
		goto out;
	}

	if (ubdata.crc_invalid && default_blob)
		ubootvar_apply_default_blob(&ubdata, default_blob,
					    default_size);

	*out_size = ubdata.size;
	if (ubdata.size) {
		*out = memdup(ubdata.data, ubdata.size);
		if (!*out)
			ret = -ENOMEM;
	}

out:
	for (i = 0; i < count; i++)
		free(copy[i].blob);

	return ret;
}
#endif

static int ubootenv_probe(struct device *dev)
{
	struct ubootvar_data *ubdata;
	struct ubootvar_copy copy[2] = {};
	int ret, i, current, count = 0;
	size_t size[2];

	/*
	 * FIXME: Flag scheme is determined by the type of underlined
	 * non-volatible device, so it should probably come from
	 * Device Tree binding. Currently we just assume incremental
	 * scheme since that is what is used on SD/eMMC devices.
	 */
	enum ubootvar_flag_scheme flag_scheme = FLAG_INCREMENTAL;

	ubdata = xzalloc(sizeof(*ubdata));

	of_property_read_string(dev->of_node, "default-environment-path",
				(const char **)&ubdata->default_env_path);
	if (ubdata->default_env_path)
		ubdata->default_env_path = strdup(ubdata->default_env_path);

	ret = of_find_path(dev->of_node, "device-path-0",
			   &ubdata->path[0],
			   OF_FIND_PATH_FLAGS_BB);
	if (ret)
		ret = of_find_path(dev->of_node, "device-path",
				   &ubdata->path[0],
				   OF_FIND_PATH_FLAGS_BB);

	if (ret) {
		dev_err(dev, "Failed to find first device\n");
		goto out;
	}

	count++;

	if (!of_find_path(dev->of_node, "device-path-1",
			  &ubdata->path[1],
			  OF_FIND_PATH_FLAGS_BB)) {
		count++;
	} else {
		/*
		 * If there's no redundant environment partition we
		 * configure both paths to point to the same device,
		 * so that writing logic could stay the same for both
		 * redundant and non-redundant cases
		 */
		ubdata->path[1] = strdup(ubdata->path[0]);
	}

	for (i = 0; i < count; i++) {
		copy[i].blob = read_file(ubdata->path[i], &size[i]);
		if (!copy[i].blob) {
			/*
			 * Leave the copy unusable instead of substituting an
			 * empty environment: the data may well be intact and
			 * only this read have failed, and flushing an empty
			 * environment over it would destroy it for good. If
			 * no copy at all can be read, probe fails below.
			 */
			dev_warn(dev, "Failed to read U-Boot environment %s\n",
				 ubdata->path[i]);
			continue;
		}

		ubootvar_parse_copy(&copy[i], copy[i].blob, size[i],
				    count > 1);
		if (!copy[i].usable)
			dev_warn(dev, "U-Boot environment %s is too small\n",
				 ubdata->path[i]);
	}

	current = ubootvar_select_copy(dev, ubdata, copy, count, flag_scheme);
	if (current < 0) {
		ret = current;
		goto out;
	}

	ubdata->cdev.name = basprintf("ubootvar%d",
				      cdev_find_free_index("ubootvar"));
	ubdata->cdev.size = ubdata->size;
	ubdata->cdev.ops = &ubootvar_ops;
	ubdata->cdev.dev = dev;
	ubdata->cdev.filetype = filetype_ubootvar;

	dev->priv = ubdata;

	ret = devfs_create(&ubdata->cdev);
	if (ret) {
		dev_err(dev, "Failed to create corresponding cdev\n");
		goto out;
	}

	cdev_create_default_automount(&ubdata->cdev);

	for (i = 0; i < count; i++) {
		/*
		 * We won't be using data from other copies, so we may as
		 * well free them at this point.
		 */
		if (i != current)
			free(copy[i].blob);
	}

	devinfo_add(dev, ubootenv_info);

	list_add_tail(&ubdata->list, &ubootvar_list);

	return 0;
out:
	for (i = 0; i < count; i++)
		free(copy[i].blob);

	free(ubdata->cdev.name);
	free(ubdata->path[0]);
	free(ubdata->path[1]);
	free(ubdata->default_env_path);
	free(ubdata);

	return ret;
}

static int ubootvar_load_defaults(void)
{
	struct ubootvar_data *ubdata;

	list_for_each_entry(ubdata, &ubootvar_list, list) {
		struct device *dev = ubdata->cdev.dev;
		size_t file_size;
		void *blob;
		int ret;

		if (!ubdata->crc_invalid || !ubdata->default_env_path)
			continue;

		blob = read_file(ubdata->default_env_path, &file_size);
		if (!blob) {
			dev_warn(dev, "default environment %s not found\n",
				 ubdata->default_env_path);
			continue;
		}

		ret = ubootvar_apply_default_blob(ubdata, blob, file_size);
		if (ret) {
			dev_err(dev, "default environment %s too small\n",
				ubdata->default_env_path);
			free(blob);
			continue;
		}

		free(blob);

		dev_info(dev, "Restored default environment from %s\n",
			 ubdata->default_env_path);
	}

	return 0;
}
postenvironment_initcall(ubootvar_load_defaults);

static struct of_device_id ubootenv_dt_ids[] = {
	{
		.compatible = "barebox,uboot-environment",
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, ubootenv_dt_ids);

static struct driver ubootenv_driver = {
	.name		= "uboot-environment",
	.probe		= ubootenv_probe,
	.of_compatible	= ubootenv_dt_ids,
};
late_platform_driver(ubootenv_driver);
