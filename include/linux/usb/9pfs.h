
/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _USB_9PFS_H
#define _USB_9PFS_H

struct f_usb9pfs_opts;

int f_usb9pfs_opts_buflen_get(struct f_usb9pfs_opts *opts);
int f_usb9pfs_opts_buflen_set(struct f_usb9pfs_opts *opts, unsigned buflen);

#endif /* _USB_FASTBOOT_H */
