// SPDX-License-Identifier: GPL-2.0
/*
 *  hrtimers - High-resolution kernel timers
 *
 *   Copyright(C) 2005, Thomas Gleixner <tglx@linutronix.de>
 *   Copyright(C) 2005, Red Hat, Inc., Ingo Molnar
 *
 *  data type definitions, declarations, prototypes
 *
 *  Started by: Thomas Gleixner and Ingo Molnar
 */
#ifndef _LINUX_HRTIMER_H
#define _LINUX_HRTIMER_H

#include <linux/list.h>
#include <linux/types.h>
#include <linux/ktime.h>

/*
 * Mode arguments of xxx_hrtimer functions:
 *
 * HRTIMER_MODE_REL		- Time value is relative to now
 *				  hard irq context even on PREEMPT_RT.
 */
enum hrtimer_mode {
	HRTIMER_MODE_REL	= 0x01,
};

/*
 * Return values for the callback function
 */
enum hrtimer_restart {
	HRTIMER_NORESTART,	/* Timer is not restarted */
	HRTIMER_RESTART,	/* Timer must be restarted */
};

typedef enum { CLOCK_MONOTONIC } clockid_t;

/**
 * struct hrtimer - the basic hrtimer structure
 * @poller:	underlying poller instance
 * @next:       the next expiration
 * @function:	timer expiry callback function
 *
 * The hrtimer structure must be initialized by hrtimer_init()
 */
struct hrtimer {
	struct poller_async		poller;
	u64				next;
	enum hrtimer_restart		(*function)(struct hrtimer *);
};

/* Initialize timers: */
static inline void __hrtimer_init(struct hrtimer *timer, const char *name)
{
	poller_async_register(&timer->poller, name);
}

#define hrtimer_init(timer, which_clock, mode) do { \
	char buf[64]; \
	snprintf(buf, sizeof(buf), "hrtimer-%s-%d\n", __func__, __LINE__); \
	buf[sizeof(buf) - 1] = '\0'; \
	(void)which_clock; \
	(void)mode; \
	__hrtimer_init(timer, buf); \
} while (0)

static void hrtimer_poller_cb(void *_timer);

/**
 * hrtimer_start - (re)start an hrtimer
 * @timer:	the timer to be added
 * @tim:	expiry time
 * @mode:	timer mode: relative (HRTIMER_MODE_REL)
 */
static inline void hrtimer_start(struct hrtimer *timer, ktime_t tim,
				 const enum hrtimer_mode mode)
{
	timer->next = tim;
	poller_call_async(&timer->poller, timer->next,
			  hrtimer_poller_cb, timer);
}

static void hrtimer_poller_cb(void *_timer)
{
	struct hrtimer *timer = _timer;
	enum hrtimer_restart ret;

	ret = timer->function(timer);

	if (ret == HRTIMER_RESTART)
		poller_call_async(&timer->poller, timer->next,
				  hrtimer_poller_cb, timer);
}

static inline int hrtimer_cancel(struct hrtimer *timer)
{
	return poller_async_cancel(&timer->poller);
}

/**
 * hrtimer_forward_now - forward the timer expiry so it expires after now
 * @timer:	hrtimer to forward
 * @interval:	the interval to forward
 *
 * Forward the timer expiry so it will expire after the current time
 * of the hrtimer clock base. Returns the number of overruns.
 *
 * Can be safely called from the callback function of @timer. If
 * called from other contexts @timer must neither be enqueued nor
 * running the callback and the caller needs to take care of
 * serialization.
 *
 * Note: This only updates the timer expiry value and does not requeue
 * the timer.
 */
static inline u64 hrtimer_forward_now(struct hrtimer *timer,
				      ktime_t interval)
{
	timer->next = interval;
	return 0;
}

#endif
