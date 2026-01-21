/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __BOOTM_FIT_H
#define __BOOTM_FIT_H

#include <linux/types.h>
#include <image-fit.h>
#include <bootm.h>

struct resource;

#ifdef CONFIG_BOOTM_FITIMAGE

/* bootm_load_fit_os() and bootm_load_fit_initrd() removed - use loadables */

void *bootm_get_fit_devicetree(struct image_data *data);

int bootm_open_fit(struct image_data *data);

static inline void bootm_close_fit(struct image_data *data)
{
	fit_close(data->os_fit);
}

static inline bool bootm_fit_has_fdt(struct image_data *data)
{
	if (!data->os_fit)
		return false;

	return fit_has_image(data->os_fit, data->fit_config, "fdt");
}

void bootm_collect_fit_loadables(struct image_data *data);

#else

/* bootm_load_fit_os() and bootm_load_fit_initrd() removed - use loadables */

static inline void *bootm_get_fit_devicetree(struct image_data *data)
{
	return ERR_PTR(-ENOSYS);
}

static inline int bootm_open_fit(struct image_data *data)
{
	return -ENOSYS;
}

static inline void bootm_close_fit(struct image_data *data)
{
}

static inline bool bootm_fit_has_fdt(struct image_data *data)
{
	return false;
}

static inline void bootm_collect_fit_loadables(struct image_data *data)
{
}

#endif

#endif
