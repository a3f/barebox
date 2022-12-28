/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __OF_PIPELINE_H
#define __OF_PIPELINE_H

#include <linux/list.h>

struct device_node;

struct of_pipeline {
	struct list_head list;
	struct device_node *node;
};

/* of_pipeline_call below will not always start from the same head,
 * thus the need to have a complete objet here to avoid out of
 * bound access in list_entry() FIXME
 */
struct of_pipeline_head {
	struct of_pipeline opl;
};

#define OF_PIPELINE_HEAD(name) \
	struct of_pipeline_head name = { .opl.list = LIST_HEAD_INIT(name.opl.list) }

int of_pipeline_add(struct of_pipeline_head *, struct of_pipeline *);

int of_pipeline_call(struct of_pipeline_head *opls,
		     struct device_node *opl_node,
		     unsigned int portno,
		     int (*op)(struct of_pipeline *opl, void *data),
		     void *data);

struct of_pipeline *of_pipeline_get(struct of_pipeline_head *opls,
				    struct device_node *node, int port);

int of_pipeline_call_endpoints(struct of_pipeline_head *opls, struct device_node *port,
			       int (*op)(struct of_pipeline *opl, void *data),
			       void *data);

#endif /* __OF_PIPELINE_H */
