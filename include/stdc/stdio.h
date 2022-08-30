#ifndef STDC_STDIO_H_
#define STDC_STDIO_H_

#include <linux/types.h>
#include <linux/printk.h>

#define stdin (void *)0x10
#define stdout (void *)0x11
#define stderr (void *)0x12

#define _IONBF 2

static inline void setvbuf(void *fp, char *buf, int mode, size_t size)
{
	if (buf == NULL && mode == _IONBF && size == 0)
		return;

	pr_warn("setvbuf: unsupported setting\n");

}

static inline void fflush(void *fp)
{
}

#endif
