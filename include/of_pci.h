/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __OF_PCI_H
#define __OF_PCI_H

#include <linux/pci.h>

struct device_node;
struct resource;

#ifdef CONFIG_OF_PCI
int of_pci_get_devfn(struct device_node *np);

int of_pci_parse_ranges(struct device_node *np, struct resource *io,
			struct resource *mem, struct resource *prefetch);
#else
static inline int of_pci_get_devfn(struct device_node *np)
{
	return -EINVAL;
}

static inline int of_pci_parse_ranges(struct device_node *np, struct resource *io,
				      struct resource *mem, struct resource *prefetch)
{
	return -EINVAL;
}

#endif

#endif
