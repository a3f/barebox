/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/*
 * Copyright (C) 2018, STMicroelectronics - All Rights Reserved
 */

#ifndef __HWSPINLOCK_H
#define __HWSPINLOCK_H

struct hwspinlock;

#ifdef CONFIG_HWSPINLOCK
int __hwspinlock_get_by_index(const struct of_phandle_args *hwlock_spec,
			      struct hwspinlock **hws);

int hwspinlock_lock_timeout(struct hwspinlock *hws, unsigned int timeout_ms);
void hwspinlock_unlock(struct hwspinlock *hws);
#else
static inline int __hwspinlock_get_by_index(const struct of_phandle_args *hwlock_spec,
					    struct hwspinlock **hws) {
	return -ENOSYS;
}
static inline int hwspinlock_lock_timeout(struct hwspinlock *hws,
					  unsigned int timeout_ms)
{
	return -ENOSYS;
}

static inline void hwspinlock_unlock(struct hwspinlock *hws)
{
}
#endif

static inline int hwspinlock_get_by_index(struct device_d *dev,
					  int index,
					  struct hwspinlock **hws)
{
	struct of_phandle_args args;
	int ret;

	/*
	 * we still check for the property, so callers may warn if there's
	 * an hwlock referenced and enabled, but support is not compiled in
	 */
	ret = of_parse_phandle_with_args(dev->device_node, "hwlocks",
					 "#hwlock-cells", index, &args);
	if (ret)
		return ret;

	if (!of_device_is_available(args.np))
		return -ENOENT;


	return __hwspinlock_get_by_index(&args, hws);
}

#endif /* __HWSPINLOCK_H */
