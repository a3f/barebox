/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: 2012 Marc Kleine-Budde <mkl@pengutronix.de> */

#ifndef __ASM_DMA_H
#define __ASM_DMA_H

#include <linux/pagemap.h>

/* Maximum cache line size that we support */
#define DMA_ALIGNMENT		64
/* Coherent allocations are realized by marking pages uncached.
 * For nommu, we want to keep the same alignment for symmetry.
 */
#define DMA_COHERENT_ALIGNMENT	PAGE_SIZE

#endif /* __ASM_DMA_H */
