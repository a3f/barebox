// SPDX-License-Identifier: GPL-2.0

#include <errno.h>

const char *rust_helper_errname(int err)
{
	return strerror(err);
}
