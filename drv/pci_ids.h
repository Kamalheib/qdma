/*
 * This file is part of the Xilinx DMA IP Core driver for Linux
 *
 * Copyright (c) 2017-present,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef __XDMA_PCI_ID_H__
#define __XDMA_PCI_ID_H__

static const struct pci_device_id pci_ids[] = {

#ifdef __QDMA_VF__
	/* PCIe lane width x4 */
	{ PCI_DEVICE(0x10ee, 0xa034), },	/* VF on PF 0 */
	{ PCI_DEVICE(0x10ee, 0xa134), },	/* VF on PF 1 */
	{ PCI_DEVICE(0x10ee, 0xa234), },	/* VF on PF 2 */
	{ PCI_DEVICE(0x10ee, 0xa334), },	/* VF on PF 3 */

	/* PCIe lane width x8 */
	{ PCI_DEVICE(0x10ee, 0xa038), },	/* VF on PF 0 */
	{ PCI_DEVICE(0x10ee, 0xa138), },	/* VF on PF 1 */
	{ PCI_DEVICE(0x10ee, 0xa238), },	/* VF on PF 2 */
	{ PCI_DEVICE(0x10ee, 0xa338), },	/* VF on PF 3 */

	/* PCIe lane width x16 */
	{ PCI_DEVICE(0x10ee, 0xa03f), },	/* VF on PF 0 */
	{ PCI_DEVICE(0x10ee, 0xa13f), },	/* VF on PF 1 */
	{ PCI_DEVICE(0x10ee, 0xa23f), },	/* VF on PF 2 */
	{ PCI_DEVICE(0x10ee, 0xa33f), },	/* VF on PF 3 */
#else
	/* PCIe lane width x4 */
	{ PCI_DEVICE(0x10ee, 0x9034), },	/* PF 0 */
	{ PCI_DEVICE(0x10ee, 0x9134), },	/* PF 1 */
	{ PCI_DEVICE(0x10ee, 0x9234), },	/* PF 2 */
	{ PCI_DEVICE(0x10ee, 0x9334), },	/* PF 3 */

	/* PCIe lane width x8 */
	{ PCI_DEVICE(0x10ee, 0x9038), },	/* PF 0 */
	{ PCI_DEVICE(0x10ee, 0x9138), },	/* PF 1 */
	{ PCI_DEVICE(0x10ee, 0x9238), },	/* PF 2 */
	{ PCI_DEVICE(0x10ee, 0x9338), },	/* PF 3 */

	/* PCIe lane width x16 */
	{ PCI_DEVICE(0x10ee, 0x903f), },	/* PF 0 */
	{ PCI_DEVICE(0x10ee, 0x913f), },	/* PF 1 */
	{ PCI_DEVICE(0x10ee, 0x923f), },	/* PF 2 */
	{ PCI_DEVICE(0x10ee, 0x933f), },	/* PF 3 */
#endif

	{0,}
};
MODULE_DEVICE_TABLE(pci, pci_ids);

#endif /* ifndef __XDMA_PCI_ID_H__ */
