/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __INCLUDE_TRACE_H__
#define __INCLUDE_TRACE_H__

void tracing_on(void);
void tracing_off(void);

void tracing_mask(void);
void tracing_unmask(void);

void trace_log(const char *);

#endif
