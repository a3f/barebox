// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Generic pipeline support for barebox
 *
 * (C) Copyright 2014 Sascha Hauer, Pengutronix
 */
#define pr_fmt(fmt) "of-pipeline: " fmt

#include <common.h>
#include <driver.h>
#include <of_graph.h>
#include <linux/list.h>
#include <of_pipeline.h>

int of_pipeline_add(struct of_pipeline_head *opls, struct of_pipeline *opl)
{
	list_add_tail(&opl->list, &opls->opl.list);

	pr_debug("%s: %s\n", __func__, opl->node->full_name);

	return 0;
}

static struct of_pipeline *of_pipeline_find_by_node(struct of_pipeline *opls,
						    struct device_node *node)
{
	struct of_pipeline *opl;

	of_device_ensure_probed(node);

	list_for_each_entry(opls, node, list)
		if (opl->node == node)
			return opl;

	return NULL;
}

struct of_pipeline *of_pipeline_get(struct of_pipeline_head *opls,
				    struct device_node *node, int port)
{
	struct of_pipeline *first_opl;

	first_opl = list_first_entry_or_null(&opls->opl.list, struct of_pipeline, list);
	if (!first_opl)
		return NULL;

	node = of_graph_get_port_by_id(node, port);
	if (!node)
		return NULL;

	pr_debug("%s: port: %s\n", __func__, node->full_name);

	node = of_graph_get_remote_port_parent(node);
	if (!node)
		return NULL;

	pr_debug("%s: remote port parent: %s\n", __func__, node->full_name);

	return of_pipeline_find_by_node(first_opl, node);
}

int of_pipeline_call_endpoints(struct of_pipeline_head *opls, struct device_node *port,
			       int (*op)(struct of_pipeline *opl, void *data),
			       void *data)
{
	struct device_node *endpoint;
	int ret;

	for_each_child_of_node(port, endpoint) {
		struct device_node *remote, *remote_parent;
		struct of_pipeline *remote_opl;
		u32 remote_port_id;

		remote = of_graph_get_remote_port(endpoint);
		if (!remote) {
			pr_debug("%s: no remote for endpoint %s\n", __func__, endpoint->full_name);
			continue;
		}

		of_property_read_u32(remote, "reg", &remote_port_id);

		remote_parent = of_graph_get_remote_port_parent(endpoint);
		if (!remote_parent) {
			pr_debug("%s: no remote_parent\n", __func__);
			return -ENODEV;
		}

		if (!of_device_is_available(remote_parent))
			continue;

		remote_opl = of_pipeline_find_by_node(opls, remote_parent);
		if (!remote_opl) {
			pr_debug("%s: cannot find remote of_pipeline %s\n", __func__, remote->full_name);
			continue;
		}

		pr_debug("%s: looked up %s: %pS\n", __func__, remote->full_name, op);
		ret = op(remote_opl, data);
		if (ret)
			return ret;
	}

	return 0;
}

int of_pipeline_call(struct of_pipeline_head *opls,
		     struct device_node *opl_node,
		     unsigned int portno,
		     int (*op)(struct of_pipeline *opl, void *data),
		     void *data)
{
	struct device_node *portnode;
	int ret;

	pr_debug("%s: %s port %d\n", __func__, opl_node->full_name, portno);

	portnode = of_graph_get_port_by_id(opl_node, portno);
	if (!portnode) {
		pr_err("%s: no port %d on %s\n", __func__, portno, opl->node->full_name);
		return -ENODEV;
	}

	return of_pipeline_call_endpoints(opls, portnode, op, data);
}
