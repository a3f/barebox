/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __BLOCK_H
#define __BLOCK_H

#include <driver.h>
#include <linux/list.h>
#include <linux/types.h>

struct block_device;

struct block_device_ops {
	int (*read)(struct block_device *, void *buf, sector_t block, blkcnt_t num_blocks);
	int (*write)(struct block_device *, const void *buf, sector_t block, blkcnt_t num_blocks);
	int (*flush)(struct block_device *);
};

struct chunk;

enum blk_type {
	BLK_TYPE_UNSPEC = 0,
	BLK_TYPE_SD,
	BLK_TYPE_MMC,
	BLK_TYPE_VIRTUAL,
	BLK_TYPE_ATA,
	BLK_TYPE_USB,
	BLK_TYPE_NVME,
};

const char *blk_type_str(enum blk_type);

struct block_device {
	struct device *dev;
	struct list_head list;
	struct block_device_ops *ops;
	u8 blockbits;
	u8 type; /* holds enum blk_type */
	blkcnt_t num_blocks;
	int rdbufsize;
	int blkmask;

	sector_t discard_start;
	blkcnt_t discard_size;

	struct list_head buffered_blocks;
	struct list_head idle_blocks;

	struct cdev cdev;
};

extern struct list_head block_device_list;

#define for_each_block_device(bdev) list_for_each_entry(bdev, &block_device_list, list)

int blockdevice_register(struct block_device *blk);
int blockdevice_unregister(struct block_device *blk);

int block_read(struct block_device *blk, void *buf, sector_t block, blkcnt_t num_blocks);
int block_write(struct block_device *blk, void *buf, sector_t block, blkcnt_t num_blocks);

static inline int block_flush(struct block_device *blk)
{
	return cdev_flush(&blk->cdev);
}

#ifdef CONFIG_BLOCK
struct block_device *cdev_get_block_device(const struct cdev *cdev);
#else
static inline struct block_device *cdev_get_block_device(const struct cdev *cdev)
{
	return NULL;
}
#endif

#endif /* __BLOCK_H */
