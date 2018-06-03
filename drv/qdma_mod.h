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

#ifndef __QDMA_MODULE_H__
#define __QDMA_MODULE_H__

#include <linux/types.h>
#include <linux/pci.h>

#include "libqdma/libqdma_export.h"
#include "cdev.h"

struct xlnx_qdata {
	unsigned long qhndl;
	struct qdma_cdev *xcdev;
};

struct xlnx_pci_dev {
	struct list_head list_head;
	struct pci_dev *pdev;
	unsigned long dev_hndl;
	struct qdma_cdev_cb cdev_cb;
	unsigned int qmax;
	unsigned int idx;
	struct xlnx_qdata qdata[0];
};

int xpdev_list_dump(char *buf, int blen);
struct xlnx_pci_dev *xpdev_find_by_idx(unsigned int idx, char *ebuf,
			int ebuflen);
struct xlnx_qdata *xpdev_queue_get(struct xlnx_pci_dev *xpdev,
			unsigned int qidx, bool c2h, bool check_qhndl,
			char *ebuf, int ebuflen);
int xpdev_queue_add(struct xlnx_pci_dev *, struct qdma_queue_conf *,
			char *ebuf, int ebuflen);
int xpdev_queue_delete(struct xlnx_pci_dev *xpdev, unsigned int qidx, bool c2h,
			char *ebuf, int ebuflen);


#endif /* ifndef __QDMA_MODULE_H__ */
