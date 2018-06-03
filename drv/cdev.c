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

#define pr_fmt(fmt)	KBUILD_MODNAME ":%s: " fmt, __func__

#include "cdev.h"

#include <asm/cacheflush.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include "qdma_mod.h"

struct class *qdma_class;

/*
 * character device file operations
 */
static int cdev_gen_open(struct inode *inode, struct file *file)
{
	struct qdma_cdev *xcdev = container_of(inode->i_cdev, struct qdma_cdev,
						cdev);
	file->private_data = xcdev;

	if (xcdev->fp_open_extra)
		return xcdev->fp_open_extra(xcdev);

	return 0;
}

static int cdev_gen_close(struct inode *inode, struct file *file)
{
	struct qdma_cdev *xcdev = (struct qdma_cdev *)file->private_data;

	if (xcdev->fp_close_extra)
		return xcdev->fp_close_extra(xcdev);

	return 0;
}

static loff_t cdev_gen_llseek(struct file *file, loff_t off, int whence)
{
	struct qdma_cdev *xcdev = (struct qdma_cdev *)file->private_data;
	loff_t newpos = 0;

	switch (whence) {
	case 0: /* SEEK_SET */
		newpos = off;
		break;
	case 1: /* SEEK_CUR */
		newpos = file->f_pos + off;
		break;
	case 2: /* SEEK_END, @TODO should work from end of address space */
		newpos = UINT_MAX + off;
		break;
	default: /* can't happen */
		return -EINVAL;
	}
	if (newpos < 0)
		return -EINVAL;
	file->f_pos = newpos;

	pr_debug("%s: pos=%lld\n", xcdev->name, (signed long long)newpos);

	return newpos;
}

static long cdev_gen_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	struct qdma_cdev *xcdev = (struct qdma_cdev *)file->private_data;

	if (xcdev->fp_ioctl_extra)
		return xcdev->fp_ioctl_extra(xcdev, cmd, arg);

	pr_info("%s ioctl NOT supported.\n", xcdev->name);
	return -EINVAL;
}

/*
 * cdev r/w
 */
static inline void iocb_release(struct qdma_io_cb *iocb)
{
	sg_free_table(iocb->sgt);
	if (iocb->pages) {
		kfree(iocb->pages);
		iocb->pages = NULL;
	}
	iocb->buf = NULL;
}

static void unmap_user_buf(struct qdma_io_cb *iocb, bool write)
{
	int i;

	sg_free_table(iocb->sgt);

	if (!iocb->pages || !iocb->pages_nr)
		return;

	for (i = 0; i < iocb->pages_nr; i++) {
		if (iocb->pages[i]) {
			if (!write)
				set_page_dirty_lock(iocb->pages[i]);
			put_page(iocb->pages[i]);
		} else
			break;
	}

	if (i != iocb->pages_nr)
		pr_info("sgl pages %d/%u.\n", i, iocb->pages_nr);

	iocb->pages_nr = 0;
}

static int map_user_buf_to_sgl(struct qdma_io_cb *iocb, bool write)
{
	unsigned long len = iocb->len;
	char *buf = iocb->buf;
	struct scatterlist *sg;
	unsigned int pages_nr = (((unsigned long)buf + len + PAGE_SIZE - 1) -
				 ((unsigned long)buf & PAGE_MASK))
				>> PAGE_SHIFT;
	int i;
	int rv;

	if (pages_nr == 0)
		return -EINVAL;

	if (sg_alloc_table(iocb->sgt, pages_nr, GFP_KERNEL)) {
		pr_info("sgl OOM.\n");
		return -ENOMEM;
	}

	iocb->pages = kcalloc(pages_nr, sizeof(struct page *), GFP_KERNEL);
	if (!iocb->pages) {
		pr_info("pages OOM.\n");
		rv = -ENOMEM;
		goto err_out;
	}

	rv = get_user_pages_fast((unsigned long)buf, pages_nr, 1/* write */,
				iocb->pages);
	/* No pages were pinned */
	if (rv < 0) {
		pr_info("unable to pin down %u user pages, %d.\n",
			pages_nr, rv);
		goto err_out;
	}
	/* Less pages pinned than wanted */
	if (rv != pages_nr) {
		pr_info("unable to pin down all %u user pages, %d.\n",
			pages_nr, rv);
		rv = -EFAULT;
		iocb->pages_nr = rv;
		goto err_out;
	}

	for (i = 1; i < pages_nr; i++) {
		if (iocb->pages[i - 1] == iocb->pages[i]) {
			pr_info("duplicate pages, %d, %d.\n",
				i - 1, i);
			rv = -EFAULT;
			iocb->pages_nr = pages_nr;
			goto err_out;
		}
	}

	sg = iocb->sgt->sgl;
	for (i = 0; i < pages_nr; i++, sg = sg_next(sg)) {
		unsigned int offset = offset_in_page(buf);
		unsigned int nbytes = min_t(unsigned int, PAGE_SIZE - offset,
						len);

		flush_dcache_page(iocb->pages[i]);
		sg_set_page(sg, iocb->pages[i], nbytes, offset);

		buf += nbytes;
		len -= nbytes;
	}

	iocb->pages_nr = pages_nr;
	return 0;

err_out:
	unmap_user_buf(iocb, write);

	return rv;
}

static ssize_t cdev_gen_read_write(struct file *file, char __user *buf,
		size_t count, loff_t *pos, bool write)
{
	struct qdma_cdev *xcdev = (struct qdma_cdev *)file->private_data;
	struct qdma_io_cb iocb;
	struct qdma_sg_req *req = &iocb.req;
	ssize_t res = 0;
	int rv;

	if (!xcdev) {
		pr_info("file 0x%p, xcdev NULL, 0x%p,%llu, pos %llu, W %d.\n",
			file, buf, (u64)count, (u64)*pos, write);
		return -EINVAL;
	}

	if (!xcdev->fp_rw) {
		pr_info("file 0x%p, %s, NO rw, 0x%p,%llu, pos %llu, W %d.\n",
			file, xcdev->name, buf, (u64)count, (u64)*pos, write);
		return -EINVAL;
	}

	pr_debug("%s, priv 0x%lx: buf 0x%p,%llu, pos %llu, W %d.\n",
		xcdev->name, xcdev->priv_data, buf, (u64)count, (u64)*pos,
		write);

	memset(&iocb, 0, sizeof(struct qdma_io_cb));
	iocb.buf = buf;
	iocb.len = count;
	iocb.sgt = &iocb.req.sgt;
	rv = map_user_buf_to_sgl(&iocb, write);
	if (rv < 0)
		return rv;

	req->write = write;
	req->dma_mapped = false;
	req->ep_addr = (u64)*pos;
	req->count = count;
	req->timeout_ms = 10 * 1000;	/* 10 seconds */
	req->fp_done = NULL;		/* blocking */

	res = xcdev->fp_rw(xcdev->xcb->xpdev->dev_hndl, xcdev->priv_data, req);

	unmap_user_buf(&iocb, write);
	iocb_release(&iocb);
	return res;
}

static ssize_t cdev_gen_write(struct file *file, const char __user *buf,
				size_t count, loff_t *pos)
{
	return cdev_gen_read_write(file, (char *)buf, count, pos, 1);
}

static ssize_t cdev_gen_read(struct file *file, char __user *buf,
				size_t count, loff_t *pos)
{
	return cdev_gen_read_write(file, (char *)buf, count, pos, 0);
}

static const struct file_operations cdev_gen_fops = {
	.owner = THIS_MODULE,
	.open = cdev_gen_open,
	.release = cdev_gen_close,
	.write = cdev_gen_write,
	.read = cdev_gen_read,
	.unlocked_ioctl = cdev_gen_ioctl,
	.llseek = cdev_gen_llseek,
};

/*
 * xcb: per pci device character device control info.
 * xcdev: per queue character device
 */
int qdma_cdev_display(void *p, char *buf)
{
	struct qdma_cdev *xcdev = (struct qdma_cdev *)p;

	return sprintf(buf, ", cdev %s", xcdev->name);
}

void qdma_cdev_destroy(struct qdma_cdev *xcdev)
{
	if (!xcdev) {
		pr_info("xcdev NULL.\n");
		return;
	}

	if (xcdev->sys_device)
		device_destroy(qdma_class, xcdev->cdevno);

	cdev_del(&xcdev->cdev);

	kfree(xcdev);
}

int qdma_cdev_create(struct qdma_cdev_cb *xcb, struct pci_dev *pdev,
			struct qdma_queue_conf *qconf, unsigned int minor,
			unsigned long qhndl, struct qdma_cdev **xcdev_pp,
			char *ebuf, int ebuflen)
{
	struct qdma_cdev *xcdev;
	int rv;

	xcdev = kzalloc(sizeof(struct qdma_cdev) + strlen(qconf->name) + 1,
			GFP_KERNEL);
	if (!xcdev) {
		pr_info("%s OOM %lu.\n", qconf->name, sizeof(struct qdma_cdev));
		if (ebuf && ebuflen) {
			rv = sprintf(ebuf, "%s cdev OOM %lu.\n",
				qconf->name, sizeof(struct qdma_cdev));
			ebuf[rv] = '\0';

		}
		return -ENOMEM;
	}

	spin_lock_init(&xcdev->lock);
	xcdev->cdev.owner = THIS_MODULE;
	xcdev->xcb = xcb;
	xcdev->priv_data = qhndl;
	strcpy(xcdev->name, qconf->name);

	xcdev->minor = minor;
	if (xcdev->minor >= xcb->cdev_minor_cnt) {
		pr_info("%s: no char dev. left.\n", qconf->name);
		if (ebuf && ebuflen) {
			rv = sprintf(ebuf, "%s cdev no cdev left.\n",
					qconf->name);
			ebuf[rv] = '\0';
		}
		rv = -ENOSPC;
		goto err_out;
	}
	xcdev->cdevno = MKDEV(xcb->cdev_major, xcdev->minor);

	cdev_init(&xcdev->cdev, &cdev_gen_fops);

	/* bring character device live */
	rv = cdev_add(&xcdev->cdev, xcdev->cdevno, 1);
	if (rv < 0) {
		pr_info("cdev_add failed %d, %s.\n", rv, xcdev->name);
		if (ebuf && ebuflen) {
			int l = sprintf(ebuf, "%s cdev add failed %d.\n",
					qconf->name, rv);
			ebuf[l] = '\0';
		}
		goto err_out;
	}

	/* create device on our class */
	if (qdma_class) {
		xcdev->sys_device = device_create(qdma_class, &(pdev->dev),
				xcdev->cdevno, NULL, "%s", xcdev->name);
		if (IS_ERR(xcdev->sys_device)) {
			rv = PTR_ERR(xcdev->sys_device);
			pr_info("%s device_create failed %d.\n",
				xcdev->name, rv);
			if (ebuf && ebuflen) {
				int l = sprintf(ebuf,
						"%s device_create failed %d.\n",
						qconf->name, rv);
				ebuf[l] = '\0';
			}
			goto del_cdev;
		}
	}

	xcdev->fp_rw = qdma_sg_req_submit;

	*xcdev_pp = xcdev;
	return 0;

del_cdev:
	cdev_del(&xcdev->cdev);

err_out:
	kfree(xcdev);
	return rv;
}

/*
 * per device initialization & cleanup
 */
void qdma_cdev_device_cleanup(struct qdma_cdev_cb *xcb)
{
	if (!xcb->cdev_major)
		return;

	unregister_chrdev_region(MKDEV(xcb->cdev_major, 0),
				xcb->cdev_minor_cnt);
	xcb->cdev_major = 0;
}

int qdma_cdev_device_init(struct qdma_cdev_cb *xcb)
{
	dev_t dev;
	int rv;

	spin_lock_init(&xcb->lock);
//	INIT_LIST_HEAD(&xcb->cdev_list);

	xcb->cdev_minor_cnt = QDMA_MINOR_MAX;

	if (xcb->cdev_major) {
		pr_info("major %d already exist.\n", xcb->cdev_major);
		return -EINVAL;
	}

	/* allocate a dynamically allocated char device node */
	rv = alloc_chrdev_region(&dev, 0, xcb->cdev_minor_cnt,
				QDMA_CDEV_CLASS_NAME);
	if (rv) {
		pr_info("unable to allocate cdev region %d.\n", rv);
		return rv;
	}
	xcb->cdev_major = MAJOR(dev);

	return 0;
}

/*
 * driver-wide Initialization & cleanup
 */

int qdma_cdev_init(void)
{
	qdma_class = class_create(THIS_MODULE, QDMA_CDEV_CLASS_NAME);
	if (IS_ERR(qdma_class)) {
		pr_info("%s: failed to create class 0x%lx.",
			QDMA_CDEV_CLASS_NAME, (unsigned long)qdma_class);
		qdma_class = NULL;
		return -1;
	}

	return 0;
}

void qdma_cdev_cleanup(void)
{
	if (qdma_class)
		class_destroy(qdma_class);
}
