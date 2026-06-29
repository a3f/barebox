/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_TIMEKEEPING_H
#define _LINUX_TIMEKEEPING_H

#include <clock.h>

/**
 * ktime_get_ns - Get the current time in nanoseconds
 *
 * Returns: current time converted to nanoseconds
 */
static inline u64 ktime_get_ns(void)
{
	return get_time_ns();
}

#endif
