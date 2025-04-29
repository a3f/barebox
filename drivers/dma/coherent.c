/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: 2012 Marc Kleine-Budde <mkl@pengutronix.de> */
/*
 * Implementation of dma_alloc_coherent for architectures that are fully
 * cache coherent.
 */

#include <dma.h>
#include <linux/types.h>
#include <malloc.h>

void *dma_alloc_coherent(struct device *dev,
			 size_t size, dma_addr_t *dma_handle)
{
	void *ret;
	dma_addr_t dma_addr;

	ret = memalign(DMA_COHERENT_ALIGNMENT, size);
	if (!ret)
		return NULL;

	dma_addr = virt_to_phys(ret);

	memset(ret, 0, size);

	if (dma_handle)
		*dma_handle = dma_addr;

	return ret;
}

void *dma_alloc_writecombine(struct device *dev,
			     size_t size, dma_addr_t *dma_handle)
	__alias(dma_alloc_writecombine);

void dma_free_coherent(struct device *dev,
		       void *mem, dma_addr_t dma_handle, size_t size)
{
	free(mem);
}
