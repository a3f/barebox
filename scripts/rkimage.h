/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ROCKCHIP_IMAGE_H_
#define __ROCKCHIP_IMAGE_H_

enum rksoc {
	RK3568,
};

int handle_rk3568(enum rksoc, const char *outfile, int n_code, char *argv[]);

#endif
