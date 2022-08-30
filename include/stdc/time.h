#ifndef STDC_TIME_H_
#define STDC_TIME_H_

#include <linux/types.h>
#include <clock.h>

static inline time_t time(time_t *timer)
{
	time_t tstamp;

	tstamp = get_time_ns();

	if (timer)
		*timer = tstamp;
	return tstamp;
}

#endif
