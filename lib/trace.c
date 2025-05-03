/* SPDX-License-Identifier: GPL-2.0-or-later */

#define __DISABLE_TRACE_MMIO__
#include <trace.h>
#include <debug_ll.h>

static bool tracing = false;
static unsigned trace_masked = 0;

void tracing_on(void)
{
	tracing = true;
}

void tracing_off(void)
{
	tracing = false;
}

void tracing_mask(void)
{
	trace_masked++;
}

void tracing_unmask(void)
{
	trace_masked--;
}

void trace_log(const char *s)
{
	if (!tracing || trace_masked)
		return;

	puts_ll(s);
	putc_ll('\n');
}
