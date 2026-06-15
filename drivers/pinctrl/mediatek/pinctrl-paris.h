/* SPDX-License-Identifier: GPL-2.0 */
/* SPDX-Comment: Origin-URL: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/pinctrl/mediatek/pinctrl-paris.h?id=3ef9f710efcb5cc1335b5b09c16c757f703d7e5f */
/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * Author: Sean Wang <sean.wang@mediatek.com>
 *	   Zhiyong Tao <zhiyong.tao@mediatek.com>
 *	   Hongzhou.Yang <hongzhou.yang@mediatek.com>
 */
#ifndef __PINCTRL_PARIS_H
#define __PINCTRL_PARIS_H

#include <linux/io.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>

#include "../core.h"
#include "../pinconf.h"
#include "../pinctrl-utils.h"
#include "../pinmux.h"
#include "pinctrl-mtk-common-v2.h"

#define MTK_RANGE(_a)		{ .range = (_a), .nranges = ARRAY_SIZE(_a), }

#define MTK_EINT_FUNCTION(_eintmux, _eintnum)				\
	{							\
		.eint_m = _eintmux,					\
		.eint_n = _eintnum,					\
	}

#define MTK_FUNCTION(_val, _name)				\
	{							\
		.muxval = _val,					\
		.name = _name,					\
	}

#define MTK_PIN(_number, _name, _eint, _drv_n, ...) {	\
		.number = _number,			\
		.name = _name,				\
		.eint = _eint,				\
		.drv_n = _drv_n,			\
		.funcs = (struct mtk_func_desc[]){	\
			__VA_ARGS__, { } },				\
	}

#define MTK_EINT_PIN(_number, _instance, _index, _debounce) {	\
		.number = _number,				\
		.instance = _instance,				\
		.index = _index,				\
		.debounce = _debounce,				\
	}

#define PINCTRL_PIN_GROUP(_name_, id)							\
	{										\
		.grp = PINCTRL_PINGROUP(_name_,id##_pins, ARRAY_SIZE(id##_pins)),	\
		.data = id##_funcs,							\
	}

int mtk_paris_pinctrl_probe(struct platform_device *pdev);

ssize_t mtk_pctrl_show_one_pin(struct mtk_pinctrl *hw,
	unsigned int gpio, char *buf, unsigned int bufLen);

#endif /* __PINCTRL_PARIS_H */
