// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <common.h>
#include <bselftest.h>
#include <crc.h>
#include <driver.h>
#include <fcntl.h>
#include <fs.h>
#include <libfile.h>
#include <malloc.h>
#include <string.h>
#include <xfuncs.h>
#include <dirent.h>
#include <ubootvar.h>

BSELFTEST_GLOBALS();

#define DATA_SIZE	4096
#define RAW_SIZE	(DATA_SIZE + sizeof(uint32_t))
#define REDUNDANT_RAW_SIZE (RAW_SIZE + sizeof(uint8_t))

/*
 * Memory-backed cdev operations for testing.
 * The cdev exposes only the data portion (no CRC header),
 * matching what the real ubootvar driver does.
 */
struct memcdev_priv {
	void *buf;
	size_t size;
};

struct memcdev {
	struct cdev cdev;
	struct memcdev_priv priv;
	char *devpath;
};

static ssize_t memcdev_read(struct cdev *cdev, void *buf, size_t count,
			    loff_t offset, unsigned long flags)
{
	struct memcdev_priv *priv = cdev->priv;

	if (offset < 0 || offset > priv->size)
		return -EINVAL;

	count = min_t(size_t, count, priv->size - offset);
	memcpy(buf, priv->buf + offset, count);
	return count;
}

static ssize_t memcdev_write(struct cdev *cdev, const void *buf, size_t count,
			     loff_t offset, unsigned long flags)
{
	struct memcdev_priv *priv = cdev->priv;

	if (offset < 0 || offset > priv->size)
		return -EINVAL;

	count = min_t(size_t, count, priv->size - offset);
	memcpy(priv->buf + offset, buf, count);
	return count;
}

static int memcdev_memmap(struct cdev *cdev, void **map, int flags)
{
	struct memcdev_priv *priv = cdev->priv;

	*map = priv->buf;
	return 0;
}

static int memcdev_flush(struct cdev *cdev)
{
	return 0;
}

static struct cdev_operations memcdev_ops = {
	.read = memcdev_read,
	.write = memcdev_write,
	.memmap = memcdev_memmap,
	.flush = memcdev_flush,
};

static const char *setup_test_cdev(struct memcdev **out, void *data,
				   size_t size)
{
	struct memcdev *memcdev;
	struct cdev *cdev;
	int idx, ret;

	memcdev = xzalloc(sizeof(*memcdev));
	cdev = &memcdev->cdev;

	memcdev->priv.buf = data;
	memcdev->priv.size = size;

	idx = cdev_find_free_index("ubootvar_test");
	cdev->name = basprintf("ubootvar_test%d", idx);
	cdev->size = size;
	cdev->ops = &memcdev_ops;
	cdev->priv = &memcdev->priv;
	cdev->filetype = filetype_ubootvar;

	ret = devfs_create(cdev);
	if (ret) {
		free(cdev->name);
		free(memcdev);
		return NULL;
	}

	*out = memcdev;
	memcdev->devpath = basprintf("/dev/%s", cdev->name);

	return memcdev->devpath;
}

static void teardown_test_cdev(struct memcdev *memcdev)
{
	struct cdev *cdev;
	int ret;

	if (!memcdev)
		return;

	cdev = &memcdev->cdev;
	ret = devfs_remove(cdev);
	if (!assert_cond(ret == 0))
		return;

	free(memcdev->devpath);
	free(cdev->name);
	free(memcdev);
}

static int count_dir_entries(const char *path)
{
	DIR *dir;
	struct dirent *d;
	int count = 0;

	dir = opendir(path);
	if (!dir)
		return -errno;

	while ((d = readdir(dir))) {
		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;
		count++;
	}

	closedir(dir);
	return count;
}

static char *read_var(const char *mountpoint, const char *name)
{
	char *path = basprintf("%s/%s", mountpoint, name);
	size_t size;
	char *val;

	val = read_file(path, &size);
	free(path);
	return val;
}

static void build_raw_env(void *blob, size_t blob_size, const char *env,
			  size_t env_size, bool redundant, uint8_t flag,
			  bool valid_crc)
{
	uint8_t *data = blob;
	size_t data_size;
	uint32_t crc;

	memset(blob, 0, blob_size);

	data += sizeof(crc);
	if (redundant)
		*data++ = flag;

	data_size = blob_size - (data - (uint8_t *)blob);
	memcpy(data, env, min(env_size, data_size));

	crc = crc32(0, data, data_size);
	if (!valid_crc)
		crc++;

	memcpy(blob, &crc, sizeof(crc));
}

static void assert_var(const char *mountpoint, const char *name,
		       const char *expected)
{
	char *val;

	val = read_var(mountpoint, name);
	if (assert_cond(val != NULL)) {
		assert_streq(val, expected);
		free(val);
	}
}

static void assert_no_var(const char *mountpoint, const char *name)
{
	char *val;

	val = read_var(mountpoint, name);
	assert_cond(val == NULL);
	free(val);
}

static bool mount_env_data(const void *env, size_t size, const char *mnt,
			   struct memcdev **memcdev, void **data)
{
	const char *devpath;
	int ret;

	*data = xmemdup(env, size);
	*memcdev = NULL;

	devpath = setup_test_cdev(memcdev, *data, size);
	if (!assert_cond(devpath != NULL))
		goto free_data;

	make_directory(mnt);

	ret = mount(devpath, "ubootvarfs", mnt, NULL);
	if (!assert_cond(ret == 0))
		goto teardown_cdev;

	return true;

teardown_cdev:
	teardown_test_cdev(*memcdev);
free_data:
	free(*data);
	*data = NULL;
	return false;
}

static void umount_env_data(const char *mnt, struct memcdev *memcdev,
			    void *data)
{
	int ret;

	ret = umount(mnt);
	assert_cond(ret == 0);
	teardown_test_cdev(memcdev);
	free(data);
}

static void assert_env_data(const void *env, size_t size,
			    void (*fn)(const char *mnt))
{
	static const char mnt[] = "/mnt/ubootvar-selftest";
	struct memcdev *memcdev;
	void *data;

	if (!mount_env_data(env, size, mnt, &memcdev, &data))
		return;

	fn(mnt);
	umount_env_data(mnt, memcdev, data);
}

static void assert_basic_env(const char *mnt)
{
	assert_var(mnt, "bootcmd", "run mmcboot");
	assert_var(mnt, "baudrate", "115200");
	assert_cond(count_dir_entries(mnt) == 2);
}

static void write_var(const char *mountpoint, const char *name,
		      const char *value)
{
	char *path = basprintf("%s/%s", mountpoint, name);
	int ret;

	ret = write_file(path, value, strlen(value));
	assert_cond(ret == 0);
	free(path);
}

static void assert_mutable_env(const char *mnt)
{
	write_var(mnt, "bootcmd", "x");
	assert_var(mnt, "bootcmd", "x");
	assert_var(mnt, "baudrate", "115200");

	write_var(mnt, "bootcmd", "run net");
	assert_var(mnt, "bootcmd", "run net");
	assert_var(mnt, "baudrate", "115200");
	assert_cond(count_dir_entries(mnt) == 2);
}

/*
 * Test: Mount ubootvarfs on binary env data, verify variables.
 *
 * Creates a data buffer with known key=value\0 pairs (the binary
 * format the real driver exposes after stripping the CRC header),
 * mounts ubootvarfs on it, and verifies variable access.
 */
static void test_ubootvarfs_mount(void)
{
	char env_data[] = "bootcmd=run mmcboot\0baudrate=115200\0";
	void *data;

	data = xzalloc(DATA_SIZE);
	memcpy(data, env_data, sizeof(env_data));
	assert_env_data(data, DATA_SIZE, assert_basic_env);
	free(data);
}

static void test_ubootvarfs_mutate_tail(void)
{
	char env_data[] = "bootcmd=run mmcboot\0baudrate=115200\0";

	assert_env_data(env_data, sizeof(env_data), assert_mutable_env);
}

static void assert_unlink_full_env(const char *mnt)
{
	char *path;
	int ret;

	assert_var(mnt, "bootcmd", "run mmcboot");
	assert_var(mnt, "baudrate", "115200");

	path = basprintf("%s/bootcmd", mnt);
	ret = unlink(path);
	assert_cond(ret == 0);
	free(path);

	assert_no_var(mnt, "bootcmd");
	assert_var(mnt, "baudrate", "115200");
	assert_cond(count_dir_entries(mnt) == 1);
}

/*
 * Test: environment that completely fills the backing store, i.e. the
 * last entry's '\0' is the final byte and there is no room for the
 * trailing empty-string terminator. Deleting a variable relocates the
 * tail and must not access anything past the mapping.
 */
static void test_ubootvarfs_unlink_full_env(void)
{
	/* sizeof() includes the implicit '\0', making it the last byte */
	char env_data[] = "bootcmd=run mmcboot\0baudrate=115200";

	assert_env_data(env_data, sizeof(env_data), assert_unlink_full_env);
}

static void assert_stored_redundant_env(const char *mnt)
{
	assert_var(mnt, "bootcmd", "run stored");
	assert_var(mnt, "baudrate", "9600");
	assert_no_var(mnt, "default_only");
	assert_cond(count_dir_entries(mnt) == 2);
}

static void assert_default_env(const char *mnt)
{
	assert_var(mnt, "bootcmd", "run default");
	assert_var(mnt, "default_only", "yes");
	assert_no_var(mnt, "baudrate");
	assert_cond(count_dir_entries(mnt) == 2);
}

static void assert_empty_env(const char *mnt)
{
	assert_no_var(mnt, "bootcmd");
	assert_cond(count_dir_entries(mnt) == 0);
}

static void test_ubootvar_redundant_uses_valid_copy(void)
{
	char invalid_env[] = "bootcmd=run invalid\0";
	char stored_env[] = "bootcmd=run stored\0baudrate=9600\0";
	char default_env[] = "bootcmd=run default\0default_only=yes\0";
	uint8_t blob0[REDUNDANT_RAW_SIZE];
	uint8_t blob1[REDUNDANT_RAW_SIZE];
	uint8_t default_blob[RAW_SIZE];
	const void *blobs[2] = { blob0, blob1 };
	const size_t sizes[2] = { sizeof(blob0), sizeof(blob1) };
	void *selected;
	size_t selected_size;
	int ret;

	build_raw_env(blob0, sizeof(blob0), invalid_env, sizeof(invalid_env),
		      true, 10, false);
	build_raw_env(blob1, sizeof(blob1), stored_env, sizeof(stored_env),
		      true, 1, true);
	build_raw_env(default_blob, sizeof(default_blob), default_env,
		      sizeof(default_env), false, 0, true);

	ret = ubootvar_apply_blobs(blobs, sizes, 2, default_blob,
				  sizeof(default_blob), &selected,
				  &selected_size);
	if (!assert_cond(ret == 0))
		return;

	assert_env_data(selected, selected_size, assert_stored_redundant_env);
	free(selected);
}

static void test_ubootvar_all_invalid_uses_default(void)
{
	char stored0_env[] = "bootcmd=run invalid0\0";
	char stored1_env[] = "bootcmd=run invalid1\0baudrate=9600\0";
	char default_env[] = "bootcmd=run default\0default_only=yes\0";
	uint8_t blob0[REDUNDANT_RAW_SIZE];
	uint8_t blob1[REDUNDANT_RAW_SIZE];
	uint8_t default_blob[RAW_SIZE];
	const void *blobs[2] = { blob0, blob1 };
	const size_t sizes[2] = { sizeof(blob0), sizeof(blob1) };
	void *selected;
	size_t selected_size;
	int ret;

	build_raw_env(blob0, sizeof(blob0), stored0_env, sizeof(stored0_env),
		      true, 10, false);
	build_raw_env(blob1, sizeof(blob1), stored1_env, sizeof(stored1_env),
		      true, 1, false);
	build_raw_env(default_blob, sizeof(default_blob), default_env,
		      sizeof(default_env), false, 0, true);

	ret = ubootvar_apply_blobs(blobs, sizes, 2, default_blob,
				  sizeof(default_blob), &selected,
				  &selected_size);
	if (!assert_cond(ret == 0))
		return;

	assert_env_data(selected, selected_size, assert_default_env);
	free(selected);
}

static void test_ubootvar_bad_default_keeps_empty_env(void)
{
	char stored0_env[] = "bootcmd=run invalid0\0";
	char stored1_env[] = "bootcmd=run invalid1\0";
	uint8_t blob0[REDUNDANT_RAW_SIZE];
	uint8_t blob1[REDUNDANT_RAW_SIZE];
	uint8_t default_blob[sizeof(uint32_t)] = {};
	const void *blobs[2] = { blob0, blob1 };
	const size_t sizes[2] = { sizeof(blob0), sizeof(blob1) };
	void *selected;
	size_t selected_size;
	int ret;

	build_raw_env(blob0, sizeof(blob0), stored0_env, sizeof(stored0_env),
		      true, 10, false);
	build_raw_env(blob1, sizeof(blob1), stored1_env, sizeof(stored1_env),
		      true, 1, false);

	ret = ubootvar_apply_blobs(blobs, sizes, 2, default_blob,
				  sizeof(default_blob), &selected,
				  &selected_size);
	if (!assert_cond(ret == 0))
		return;

	assert_env_data(selected, selected_size, assert_empty_env);
	free(selected);
}

static void test_ubootvar(void)
{
	test_ubootvarfs_mount();
	test_ubootvarfs_mutate_tail();
	test_ubootvarfs_unlink_full_env();
	test_ubootvar_redundant_uses_valid_copy();
	test_ubootvar_all_invalid_uses_default();
	test_ubootvar_bad_default_keeps_empty_env();
}
bselftest(core, test_ubootvar);
