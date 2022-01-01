// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <errno.h>
#include <of.h>
#include <of_pci.h>

/**
 * of_pci_get_devfn() - Get device and function numbers for a device node
 * @np: device node
 *
 * Parses a standard 5-cell PCI resource and returns an 8-bit value that can
 * be passed to the PCI_SLOT() and PCI_FUNC() macros to extract the device
 * and function numbers respectively. On error a negative error code is
 * returned.
 */
int of_pci_get_devfn(struct device_node *np)
{
	unsigned int size;
	const __be32 *reg;

	reg = of_get_property(np, "reg", &size);

	if (!reg || size < 5 * sizeof(__be32))
		return -EINVAL;

	return (be32_to_cpup(reg) >> 8) & 0xff;
}
EXPORT_SYMBOL_GPL(of_pci_get_devfn);

static inline bool is_64bit(const struct resource *res)
{
	return res->flags & IORESOURCE_MEM_64;
}

int of_pci_parse_ranges(struct device_node *np, struct resource *io, struct resource *mem, struct resource *prefetch)
{
	struct of_pci_range_parser parser;
	struct of_pci_range range;
	struct resource memtmp = {};
	struct resource res;

	if (of_pci_range_parser_init(&parser, np)) {
		pr_err("%s: missing \"ranges\" property\n", np->name);
		return -EINVAL;
	}

	for_each_of_pci_range(&parser, &range) {
		of_pci_range_to_resource(&range, np, &res);

		switch (res.flags & IORESOURCE_TYPE_BITS) {
		case IORESOURCE_IO:
			*io = res;
			io->name = "I/O";
			break;

		case IORESOURCE_MEM:
			if (res.flags & IORESOURCE_PREFETCH) {
				*prefetch = res;
				prefetch->name = "PREFETCH";
			} else {
				/* Choose 32-bit mappings over 64-bit ones if possible */
				if (memtmp.name && !is_64bit(&memtmp) && is_64bit(&res))
					break;

				memtmp = res;
				memtmp.name = "MEM";
			}
			break;
		}
	}

	if (memtmp.name)
		*mem = memtmp;

	return 0;
}
EXPORT_SYMBOL_GPL(of_pci_parse_ranges);
