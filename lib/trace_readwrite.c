// SPDX-License-Identifier: GPL-2.0-only
/*
 * Register read and write tracepoints
 *
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/io.h>
#include <stdio.h>
#include <trace.h>

// Uncomment to trace post-writes and pre-reads as well
// This is normally only useful if debugging a bus hang
// #define TRACE_MMIO_ALL 1

static char mmio_suffix(u8 width)
{
	switch (width) {
	case 8:  return 'b';
	case 16:  return 'w';
	case 32:  return 'l';
	case 64:  return 'q';
	default: return '?';
	}
}

void log_write_mmio(u64 val, u8 width, volatile void __iomem *addr,
		    unsigned long caller_addr, unsigned long caller_addr0)
{
	char *b;
	b = basprintf("write%c(%#08llx, %#08lx);\t// %pS -> %pS",
		      mmio_suffix(width), val, (ulong)addr,
		      (void *)caller_addr0, (void *)caller_addr);
	trace_log(b);
	free(b);
}
EXPORT_SYMBOL_GPL(log_write_mmio);

void log_post_write_mmio(u64 val, u8 width, volatile void __iomem *addr,
			 unsigned long caller_addr, unsigned long caller_addr0)
{
	char *b;

	if (!__is_defined(TRACE_MMIO_ALL))
		return;

	b = basprintf("// post write%c(%#08llx, %#08lx);\t// %pS -> %pS",
		      mmio_suffix(width), val, (ulong)addr,
		      (void *)caller_addr0, (void *)caller_addr);
	trace_log(b);
	free(b);
}
EXPORT_SYMBOL_GPL(log_post_write_mmio);

void log_read_mmio(u8 width, const volatile void __iomem *addr,
		   unsigned long caller_addr, unsigned long caller_addr0)
{
	char *b;

	if (!__is_defined(TRACE_MMIO_ALL))
		return;

	b = basprintf("// pre read%c(%#08lx);\t// %pS -> %pS",
		      mmio_suffix(width), (ulong)addr,
		      (void *)caller_addr0, (void *)caller_addr);
	trace_log(b);
	free(b);
}
EXPORT_SYMBOL_GPL(log_read_mmio);

void log_post_read_mmio(u64 val, u8 width, const volatile void __iomem *addr,
			unsigned long caller_addr, unsigned long caller_addr0)
{
	char *b;
	b = basprintf(" read%c(%#08lx) = %#llx;\t// %pS -> %pS",
		      mmio_suffix(width), (ulong)addr, val,
		      (void *)caller_addr0, (void *)caller_addr);
	trace_log(b);
	free(b);
}
EXPORT_SYMBOL_GPL(log_post_read_mmio);
