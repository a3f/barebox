/* SPDX-License-Identifier: GPL-2.0
 *
 * simple_card_utils.h
 *
 * Copyright (c) 2016 Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
 */

#ifndef __SIMPLE_CARD_UTILS_H
#define __SIMPLE_CARD_UTILS_H

struct device;
struct device_node;

void asoc_simple_parse_daifmt(struct device *dev,
			      struct device_node *node,
			      struct device_node *codec,
			      char *prefix,
			      unsigned int *retfmt);

#endif /* __SIMPLE_CARD_UTILS_H */
