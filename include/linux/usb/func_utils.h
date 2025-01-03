// SPDX-License-Identifier: GPL-2.0
/*
 * func_utils.h
 *
 * Utility definitions for USB functions
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Andrzej Pietrasiewicz <andrzejtp2010@gmail.com>
 */

#ifndef _FUNC_UTILS_H_
#define _FUNC_UTILS_H_

#include <linux/usb/gadget.h>
#include <linux/bug.h>

/**
 * alloc_ep_req - returns a usb_request allocated by the gadget driver and
 * allocates the request's buffer.
 *
 * @ep: the endpoint to allocate a usb_request
 * @len: usb_requests's buffer suggested size
 *
 * In case @ep direction is OUT, the @len will be aligned to ep's
 * wMaxPacketSize. In order to avoid memory leaks or drops, *always* use
 * usb_requests's length (req->length) to refer to the allocated buffer size.
 * Requests allocated via alloc_ep_req() *must* be freed by free_ep_req().
 */
struct usb_request *alloc_ep_req(struct usb_ep *ep, size_t len);

/* Frees a usb_request previously allocated by alloc_ep_req() */
static inline void free_ep_req(struct usb_ep *ep, struct usb_request *req)
{
	WARN_ON(req->buf == NULL);
	kfree(req->buf);
	req->buf = NULL;
	usb_ep_free_request(ep, req);
}

#endif /* _FUNC_UTILS_H_ */
