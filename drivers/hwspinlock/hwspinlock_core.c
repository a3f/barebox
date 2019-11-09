#define DEBUG 1
// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2018, STMicroelectronics - All Rights Reserved
 */

#include <common.h>
#include <clock.h>
#include <hwspinlock.h>

#include "hwspinlock_internal.h"

static LIST_HEAD(bank_list);

/**
 * of_hwspin_lock_simple_xlate - translate hwlock_spec to return a lock id
 * @bank: the hwspinlock device bank
 * @hwlock_spec: hwlock specifier as found in the device tree
 *
 * This is a simple translation function, suitable for hwspinlock platform
 * drivers that only has a lock specifier length of 1.
 *
 * Returns a relative index of the lock within a specified bank on success,
 * or -EINVAL on invalid specifier cell count.
 */

static inline int
of_hwspin_lock_simple_xlate(const struct of_phandle_args *hwlock_spec)
{
	if (WARN_ON(hwlock_spec->args_count != 1))
		return -EINVAL;

	return hwlock_spec->args[0];
}

int __hwspinlock_get_by_index(const struct of_phandle_args *args,
			      struct hwspinlock **hws)
{
	struct hwspinlock_device *bank;
	int index;

	index = of_hwspin_lock_simple_xlate(args);
	if (index < 0)
		return index;


	list_for_each_entry(bank, &bank_list, list) {
		if (bank->dev->device_node == args->np) {
			*hws = &bank->lock[index];
			return 0;
		}
	}

	return -EPROBE_DEFER;
}

int hwspinlock_lock_timeout(struct hwspinlock *hws, unsigned int timeout_ms)
{
	const struct hwspinlock_ops *ops;
	u64 start;
	bool locked;

	if (!hws)
		return -EINVAL;

	ops = hws->bank->ops;

	start = get_time_ns();
	do {
		locked = ops->trylock(hws);
		if (locked)
			return 0;

		if (ops->relax)
			ops->relax(hws);
	} while (!is_timeout(start, timeout_ms * MSECOND));

	return -ETIMEDOUT;
}

void hwspinlock_unlock(struct hwspinlock *hws)
{
	const struct hwspinlock_ops *ops;

	if (!hws)
		return;

	ops = hws->bank->ops;

	ops->unlock(hws);
}

int hwspin_lock_register(struct hwspinlock_device *bank, struct device_d *dev,
			 const struct hwspinlock_ops *ops,
			 int base_id, int num_locks)
{
	struct hwspinlock *hwlock;
	int i;

	if (!bank || !ops || !dev || !num_locks || !ops->trylock ||
	    !ops->unlock) {
		pr_err("invalid parameters\n");
		return -EINVAL;
	}

	bank->dev = dev;
	bank->ops = ops;
	bank->base_id = base_id;
	bank->num_locks = num_locks;

	for (i = 0; i < num_locks; i++) {
		hwlock = &bank->lock[i];
		hwlock->bank = bank;
	}

	list_add_tail(&bank->list, &bank_list);

	return 0;
}

