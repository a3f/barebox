/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __UBOOTVAR_H
#define __UBOOTVAR_H

#include <linux/types.h>

/*
 * Run up to two raw U-Boot environment copies and an optional default
 * environment blob through the same selection the driver uses at probe
 * time and return the resulting environment data.
 *
 * Exposed for the selftest.
 */
int ubootvar_apply_blobs(const void * const blob[2], const size_t size[2],
			 int count, const void *default_blob,
			 size_t default_size, void **out, size_t *out_size);

#endif /* __UBOOTVAR_H */
