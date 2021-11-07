/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ROCKCHIP_IMAGE_H_
#define __ROCKCHIP_IMAGE_H_

enum rksoc {
	RK3036,
	RK3066,
	RK3128,
	PX3SE,
	RK3188,
	RK322x,
	RK3288,
	RK3308,
	RK3328,
	RK3368,
	RK3399,
	RK3326,
	PX30,
	RV1108,
	RV1126,
	RK1808,
	RK3568,
};

int handle_rk30(enum rksoc, const char *outfile, int n_code, char *argv[]);
int handle_rk3568(enum rksoc, const char *outfile, int n_code, char *argv[]);

#endif
