// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Video pipeline (VPL) support for barebox
 *
 * (C) Copyright 2014 Sascha Hauer, Pengutronix
 */
#define pr_fmt(fmt) "VPL: " fmt

#include <common.h>
#include <driver.h>
#include <video/vpl.h>
#include <of_pipeline.h>

struct vpl_ioctl_context {
	unsigned int cmd;
	void *ptr;
};

static OF_PIPELINE_HEAD(vpls);

static struct vpl *to_vpl(struct of_pipeline *opl)
{
	return container_of(opl, struct vpl, pipeline);
}

int vpl_register(struct vpl *vpl)
{
	vpl->pipeline.node = vpl->node;
	return of_pipeline_add(&vpls, &vpl->pipeline);
}

struct vpl *of_vpl_get(struct device_node *node, int port)
{
	return to_vpl(of_pipeline_get(&vpls, node, port));
}

static int do_ioctl(struct of_pipeline *opl, void *data)
{
	struct vpl *vpl = to_vpl(opl);
	struct vpl_ioctl_context *ctx = data;

	return vpl->ioctl(vpl, ctx->port, ctx->cmd, ctx->ptr);
}

int vpl_ioctl(struct vpl *vpl, unsigned int port,
	      unsigned int cmd, void *ptr)
{
	struct vpl_ioctl_context ctx = {
		.port = port, .cmd = cmd, .ptr = ptr,
	};

	return of_pipeline_call(&vpl->pipeline, port, do_ioctl, &ctx);
}
