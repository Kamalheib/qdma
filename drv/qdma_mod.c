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

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s: " fmt, __func__

#include "qdma_mod.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/aer.h>
#include <linux/vmalloc.h>

/* include early, to verify it depends only on the headers above */
#include "version.h"

static char version[] =
	DRV_MODULE_DESC " v" DRV_MODULE_VERSION "\n";

MODULE_AUTHOR("Xilinx, Inc.");
MODULE_DESCRIPTION(DRV_MODULE_DESC);
MODULE_VERSION(DRV_MODULE_VERSION);
MODULE_LICENSE("Dual BSD/GPL");

/* Module Parameters */
static unsigned int poll_mode_en = 1;
module_param(poll_mode_en, uint, 0644);
MODULE_PARM_DESC(poll_mode_en, "use hw polling instead of interrupts for determining dma transfer completion");

static unsigned int pftch_en = 0;
module_param(pftch_en, uint, 0644);
MODULE_PARM_DESC(pftch_en, "enable c2h prefetch");
static unsigned int ind_intr_mode = 0;
module_param(ind_intr_mode, uint, 0644);
MODULE_PARM_DESC(ind_intr_mode, "enable interrupt aggregation");

#include "pci_ids.h"

/*
 * xpdev helper functions
 */
static LIST_HEAD(xpdev_list);
static DEFINE_MUTEX(xpdev_mutex);

static inline void xpdev_list_remove(struct xlnx_pci_dev *xpdev)
{
	mutex_lock(&xpdev_mutex);
	list_del(&xpdev->list_head);
	mutex_unlock(&xpdev_mutex);
}

static inline void xpdev_list_add(struct xlnx_pci_dev *xpdev)
{
	mutex_lock(&xpdev_mutex);
	list_add_tail(&xpdev->list_head, &xpdev_list);
	mutex_unlock(&xpdev_mutex);
}

int xpdev_list_dump(char *buf, int buflen)
{
	struct xlnx_pci_dev *xpdev, *tmp;
	int len = 0;

	if (!buf || !buflen)
		return 0;

	mutex_lock(&xpdev_mutex);
	list_for_each_entry_safe(xpdev, tmp, &xpdev_list, list_head) {
		struct qdma_dev_conf *conf = qdma_device_get_config(
				xpdev->dev_hndl, NULL, 0);
		if (!conf) {
			len += sprintf(buf + len, "qdma%u\t ERR\n",
					xpdev->idx);
		} else {
			struct pci_dev *pdev = conf->pdev;

#ifdef __QDMA_VF__
			len += sprintf(buf + len, "qdmavf");
#else
			len += sprintf(buf + len, "qdma");
#endif

			len += sprintf(buf + len, "%u\t%s\tmax QP: %u\n",
				xpdev->idx, dev_name(&pdev->dev),
				conf->qsets_max);
		}
		if (len >= buflen)
			break;
	}
	mutex_unlock(&xpdev_mutex);

	buf[len] = '\0';
	return len;
}

struct xlnx_pci_dev *xpdev_find_by_idx(unsigned int idx, char *buf, int buflen)
{
	struct xlnx_pci_dev *xpdev, *tmp;

	mutex_lock(&xpdev_mutex);
	list_for_each_entry_safe(xpdev, tmp, &xpdev_list, list_head) {
		if (xpdev->idx == idx) {
			mutex_unlock(&xpdev_mutex);
			return xpdev;
		}
	}
	mutex_unlock(&xpdev_mutex);

	if (buf && buflen) {
		int len = sprintf(buf, "NO match found with idx %u.\n", idx);

		buf[++len] = '\0';
	}
	return NULL;
}

struct xlnx_qdata *xpdev_queue_get(struct xlnx_pci_dev *xpdev,
			unsigned int qidx, bool c2h, bool check_qhndl,
			char *ebuf, int ebuflen)
{
	struct xlnx_qdata *qdata;
	int len;

	if (qidx >= xpdev->qmax) {
		pr_info("qdma%d QID %u too big, %u.\n",
			xpdev->idx, qidx, xpdev->qmax);
		if (ebuf && ebuflen) {
			len = sprintf(ebuf, "QID %u too big, %u.\n",
					 qidx, xpdev->qmax);
			ebuf[len] = '\0';
		}
		return NULL;
	}

	qdata = xpdev->qdata + qidx;
	if (c2h)
		qdata += xpdev->qmax;

	if (check_qhndl && (!qdata->qhndl && !qdata->xcdev)) {
		pr_info("qdma%d QID %u NOT configured.\n", xpdev->idx, qidx);
		if (ebuf && ebuflen) {
			len = sprintf(ebuf, "QID %u NOT configured.\n", qidx);
			ebuf[len] = '\0';
		}
		return NULL;
	}

	return qdata;
}

int xpdev_queue_delete(struct xlnx_pci_dev *xpdev, unsigned int qidx, bool c2h,
			char *ebuf, int ebuflen)
{
	struct xlnx_qdata *qdata = xpdev_queue_get(xpdev, qidx, c2h, 1, ebuf,
						ebuflen);
	int rv = 0;

	if (!qdata)
		return -EINVAL;

	if (qdata->xcdev)
		qdma_cdev_destroy(qdata->xcdev);

	if (qdata->qhndl != QDMA_QUEUE_IDX_INVALID)
		rv = qdma_queue_remove(xpdev->dev_hndl, qdata->qhndl,
					ebuf, ebuflen);
	else
		pr_info("qidx %u/%u, c2h %d, qhndl invalid.\n",
			qidx, xpdev->qmax, c2h);

	memset(qdata, 0, sizeof(*qdata));
	return rv;
}

int xpdev_queue_add(struct xlnx_pci_dev *xpdev, struct qdma_queue_conf *qconf,
			char *ebuf, int ebuflen)
{
	struct xlnx_qdata *qdata;
	struct qdma_cdev *xcdev;
	unsigned long qhndl;
	int rv;

	rv = qdma_queue_add(xpdev->dev_hndl, qconf, &qhndl, ebuf, ebuflen);
	if (rv < 0)
		return rv;

	pr_debug("qdma%d idx %u, st %d, c2h %d, added, qhndl 0x%lx.\n",
		xpdev->idx, qconf->qidx, qconf->st, qconf->c2h, qhndl);

	qdata = xpdev_queue_get(xpdev, qconf->qidx, qconf->c2h, 0, ebuf,
				ebuflen);
	if (!qdata) {
		pr_info("q added 0x%lx, get failed, idx 0x%x.\n",
			qhndl, qconf->qidx);
		return rv;
	}

	/* always create the cdev */
	rv = qdma_cdev_create(&xpdev->cdev_cb, xpdev->pdev, qconf, qhndl,
				qhndl, &xcdev, ebuf, ebuflen);

	qdata->qhndl = qhndl;
	qdata->xcdev = xcdev;

	return rv;
}

static void xpdev_free(struct xlnx_pci_dev *p)
{
	xpdev_list_remove(p);

	if (((unsigned long)p) >= VMALLOC_START &&
	    ((unsigned long)p) < VMALLOC_END)
		vfree(p);
	else
		kfree(p);
}

static struct xlnx_pci_dev *xpdev_alloc(struct pci_dev *pdev, unsigned int qmax)
{
	int sz = sizeof(struct xlnx_pci_dev) +
		qmax * 2 * sizeof(struct xlnx_qdata);
	struct xlnx_pci_dev *xpdev = kzalloc(sz, GFP_KERNEL);

	if (!xpdev) {
		xpdev = vmalloc(sz);
		if (xpdev)
			memset(xpdev, 0, sz);
	}

	if (!xpdev) {
		pr_info("OMM, qmax %u, sz %u.\n", qmax, sz);
		return NULL;
	}
	xpdev->pdev = pdev;
	xpdev->qmax = qmax;
	xpdev->idx = 0xFF;

	xpdev_list_add(xpdev);
	return xpdev;
}

static int probe_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct qdma_dev_conf conf;
	struct xlnx_pci_dev *xpdev = NULL;
	unsigned long dev_hndl;
	int rv;

	pr_info("%s: func 0x%x/0x%x, p/v %d/%d,0x%p.\n",
		dev_name(&pdev->dev), PCI_FUNC(pdev->devfn), QDMA_PF_MAX,
		pdev->is_physfn, pdev->is_virtfn, pdev->physfn);

	memset(&conf, 0, sizeof(conf));
	conf.poll_mode = poll_mode_en;
	conf.pftch_en = pftch_en;
	conf.indirect_intr_mode = ind_intr_mode;
	conf.h2c_channel_max = QDMA_MM_ENGINE_MAX;
	conf.c2h_channel_max = QDMA_MM_ENGINE_MAX;
	conf.user_max = 0;
	conf.vf_max = 0;	/* enable via sysfs */
#ifdef __QDMA_VF__
	conf.qsets_max = QDMA_Q_PER_VF_MAX;
#else
	conf.qsets_max = QDMA_Q_PER_PF_MAX;
#endif /* #ifdef __QDMA_VF__ */
	conf.pdev = pdev;

	rv = qdma_device_open(DRV_MODULE_NAME, &conf, &dev_hndl);
	if (rv < 0)
		return rv;

	xpdev = xpdev_alloc(pdev, conf.qsets_max);
	if (!xpdev) {
		rv = -EINVAL;
		goto close_device;
	}
	xpdev->dev_hndl = dev_hndl;
	xpdev->idx = conf.idx;

	rv = qdma_cdev_device_init(&xpdev->cdev_cb);
	if (rv < 0)
		goto close_device;
	xpdev->cdev_cb.xpdev = xpdev;

	dev_set_drvdata(&pdev->dev, xpdev);

	return 0;

close_device:
	qdma_device_close(pdev, dev_hndl);

	if (xpdev)
		xpdev_free(xpdev);

	return rv;
}

static void xpdev_device_cleanup(struct xlnx_pci_dev *xpdev)
{
	struct xlnx_qdata *qdata = xpdev->qdata;
	int i;
	int max = xpdev->qmax << 1;

	for (i = 0; i < max; i++, qdata++) {
		if (qdata->xcdev)
			qdma_cdev_destroy(qdata->xcdev);
		memset(qdata, 0, sizeof(*qdata));
	}
}

static void remove_one(struct pci_dev *pdev)
{
	struct xlnx_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

	if (!xpdev) {
		pr_info("%s NOT attached.\n", dev_name(&pdev->dev));
		return;
	}

	pr_info("%s pdev 0x%p, xdev 0x%p, hndl 0x%lx, qdma%d.\n",
		dev_name(&pdev->dev), pdev, xpdev, xpdev->dev_hndl, xpdev->idx);

	xpdev_device_cleanup(xpdev);

	qdma_device_close(pdev, xpdev->dev_hndl);

	xpdev_free(xpdev);

	dev_set_drvdata(&pdev->dev, NULL);
}

#if defined(CONFIG_PCI_IOV) && !defined(__QDMA_VF__)
static int sriov_config(struct pci_dev *pdev, int num_vfs)
{
	struct xlnx_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

	if (!xpdev) {
		pr_info("%s NOT attached.\n", dev_name(&pdev->dev));
		return -EINVAL;
	}

	pr_info("%s pdev 0x%p, xdev 0x%p, hndl 0x%lx, qdma%d.\n",
		dev_name(&pdev->dev), pdev, xpdev, xpdev->dev_hndl, xpdev->idx);

	if (num_vfs > QDMA_VF_PER_PF_MAX) {
		pr_info("%s, clamp down # of VFs %d -> %d.\n",
			dev_name(&pdev->dev), num_vfs, QDMA_VF_PER_PF_MAX);
		num_vfs = QDMA_VF_PER_PF_MAX;
	}

	return qdma_device_sriov_config(pdev, xpdev->dev_hndl, num_vfs);
}
#endif


static pci_ers_result_t qdma_error_detected(struct pci_dev *pdev,
					pci_channel_state_t state)
{
	struct xlnx_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

	switch (state) {
	case pci_channel_io_normal:
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		pr_warn("dev 0x%p,0x%p, frozen state error, reset controller\n",
			pdev, xpdev);
		pci_disable_device(pdev);
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		pr_warn("dev 0x%p,0x%p, failure state error, req. disconnect\n",
			pdev, xpdev);
		return PCI_ERS_RESULT_DISCONNECT;
	}
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t qdma_slot_reset(struct pci_dev *pdev)
{
	struct xlnx_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

	if (!xpdev) {
		pr_info("%s NOT attached.\n", dev_name(&pdev->dev));
		return PCI_ERS_RESULT_DISCONNECT;
	}

	pr_info("0x%p restart after slot reset\n", xpdev);
	if (pci_enable_device_mem(pdev)) {
		pr_info("0x%p failed to renable after slot reset\n", xpdev);
		return PCI_ERS_RESULT_DISCONNECT;
	}

	pci_set_master(pdev);
	pci_restore_state(pdev);
	pci_save_state(pdev);

	return PCI_ERS_RESULT_RECOVERED;
}

static void qdma_error_resume(struct pci_dev *pdev)
{
	struct xlnx_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

	if (!xpdev) {
		pr_info("%s NOT attached.\n", dev_name(&pdev->dev));
		return;
	}

	pr_info("dev 0x%p,0x%p.\n", pdev, xpdev);
	pci_cleanup_aer_uncorrect_error_status(pdev);
}

static const struct pci_error_handlers qdma_err_handler = {
	.error_detected	= qdma_error_detected,
	.slot_reset	= qdma_slot_reset,
	.resume		= qdma_error_resume,
};

static struct pci_driver pci_driver = {
	.name = DRV_MODULE_NAME,
	.id_table = pci_ids,
	.probe = probe_one,
	.remove = remove_one,
//	.shutdown = shutdown_one,
#if defined(CONFIG_PCI_IOV) && !defined(__QDMA_VF__)
        .sriov_configure = sriov_config,
#endif
	.err_handler = &qdma_err_handler,
};

int xlnx_nl_init(void);
void xlnx_nl_exit(void);

static int __init qdma_mod_init(void)
{
	int rv;

	pr_info("%s", version);

	rv = libqdma_init();
	if (rv < 0)
		return rv;

	rv = xlnx_nl_init();
	if (rv < 0)
		return rv;

	rv = qdma_cdev_init();
	if (rv < 0)
		return rv;

	return pci_register_driver(&pci_driver);
}

static void __exit qdma_mod_exit(void)
{
	/* unregister this driver from the PCI bus driver */
	pci_unregister_driver(&pci_driver);

	xlnx_nl_exit();

	qdma_cdev_cleanup();

	libqdma_exit();
}

module_init(qdma_mod_init);
module_exit(qdma_mod_exit);
