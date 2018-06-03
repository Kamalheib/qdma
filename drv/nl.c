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

#include "nl.h"

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <net/genetlink.h>

#include "libqdma/libqdma_export.h"
#include "qdma_mod.h"

static int xnl_dev_list(struct sk_buff *skb_2, struct genl_info *info);

static struct nla_policy xnl_policy[XNL_ATTR_MAX] = {
	[XNL_ATTR_GENMSG] =	{ .type = NLA_NUL_STRING },

	[XNL_ATTR_DRV_INFO] =	{ .type = NLA_NUL_STRING },

	[XNL_ATTR_DEV_IDX] =	{ .type = NLA_U32 },
	[XNL_ATTR_PCI_BUS] =	{ .type = NLA_U32 },
	[XNL_ATTR_PCI_DEV] =	{ .type = NLA_U32 },
	[XNL_ATTR_PCI_FUNC] =	{ .type = NLA_U32 },
	[XNL_ATTR_DEV_CFG_BAR] = { .type = NLA_U32 },
	[XNL_ATTR_DEV_USR_BAR] = { .type = NLA_U32 },
	[XNL_ATTR_DEV_QSET_MAX] =	{ .type = NLA_U32 },

	[XNL_ATTR_REG_BAR_NUM] = { .type = NLA_U32 },
	[XNL_ATTR_REG_ADDR] =	{ .type = NLA_U32 },
	[XNL_ATTR_REG_VAL] =	{ .type = NLA_U32 },

	[XNL_ATTR_QFLAG] =	{ .type = NLA_U32 },
	[XNL_ATTR_QRNGSZ] =	{ .type = NLA_U32 },
	[XNL_ATTR_QBUFSZ] =	{ .type = NLA_U32 },
	[XNL_ATTR_QIDX] =	{ .type = NLA_U32 },
	[XNL_ATTR_WRB_DESC_SIZE] = { .type = NLA_U8 },
};

static int xnl_dev_list(struct sk_buff *, struct genl_info *);
static int xnl_dev_info(struct sk_buff *, struct genl_info *);
static int xnl_q_list(struct sk_buff *, struct genl_info *);
static int xnl_q_add(struct sk_buff *, struct genl_info *);
static int xnl_q_start(struct sk_buff *, struct genl_info *);
static int xnl_q_stop(struct sk_buff *, struct genl_info *);
static int xnl_q_del(struct sk_buff *, struct genl_info *);
static int xnl_q_dump(struct sk_buff *, struct genl_info *);
static int xnl_q_dump_desc(struct sk_buff *, struct genl_info *);
static int xnl_q_dump_wrb(struct sk_buff *, struct genl_info *);

struct genl_ops xnl_ops[] = {
	{
		.cmd = XNL_CMD_DEV_LIST,
		.policy = xnl_policy,
		.doit = xnl_dev_list,
	},
	{
		.cmd = XNL_CMD_DEV_INFO,
		.policy = xnl_policy,
		.doit = xnl_dev_info,
	},
	{
		.cmd = XNL_CMD_Q_LIST,
		.policy = xnl_policy,
		.doit = xnl_q_list,
	},
	{
		.cmd = XNL_CMD_Q_ADD,
		.policy = xnl_policy,
		.doit = xnl_q_add,
	},
	{
		.cmd = XNL_CMD_Q_START,
		.policy = xnl_policy,
		.doit = xnl_q_start,
	},
	{
		.cmd = XNL_CMD_Q_STOP,
		.policy = xnl_policy,
		.doit = xnl_q_stop,
	},
	{
		.cmd = XNL_CMD_Q_DEL,
		.policy = xnl_policy,
		.doit = xnl_q_del,
	},
	{
		.cmd = XNL_CMD_Q_DUMP,
		.policy = xnl_policy,
		.doit = xnl_q_dump,
	},
	{
		.cmd = XNL_CMD_Q_DESC,
		.policy = xnl_policy,
		.doit = xnl_q_dump_desc,
	},
	{
		.cmd = XNL_CMD_Q_WRB,
		.policy = xnl_policy,
		.doit = xnl_q_dump_wrb,
	},
};

static struct genl_family xnl_family = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
	.id = 0,
#else
	.id = GENL_ID_GENERATE,
#endif
	.hdrsize = 0,
#ifdef __QDMA_VF__
	.name = XNL_NAME_VF,
#else
	.name = XNL_NAME_PF,
#endif
	.ops = xnl_ops,
	.n_ops = ARRAY_SIZE(xnl_ops),
	.maxattr = XNL_ATTR_MAX - 1,
};

static struct sk_buff *xnl_msg_alloc(enum xnl_op_t op, int min_sz,
				void **hdr, struct genl_info *info)
{
	struct sk_buff *skb;
	void *p;
	unsigned long sz = min_sz < NLMSG_GOODSIZE ? NLMSG_GOODSIZE : min_sz;

	skb = genlmsg_new(sz, GFP_KERNEL);
	if (!skb) {
		pr_info("OOM %lu.\n", sz);
		return NULL;
	}

	p = genlmsg_put(skb, 0, info->snd_seq + 1, &xnl_family, 0, op);
	if (!p) {
		pr_info("skb too small.\n");
		nlmsg_free(skb);
		return NULL;
	}

	*hdr = p;
	return skb;
}

static inline int xnl_msg_add_attr_str(struct sk_buff *skb,
					enum xnl_attr_t type, char *s)
{
	int rv;

	rv = nla_put_string(skb, type, s);
	if (rv != 0) {
		pr_info("nla_put_string return %d.\n", rv);
		return -EINVAL;
	}
	return 0;
}

static inline int xnl_msg_add_attr_uint(struct sk_buff *skb,
					enum xnl_attr_t type, u32 v)
{
	int rv;

	rv = nla_put_u32(skb, type, v);
	if (rv != 0) {
		pr_info("nla add dev_idx failed %d.\n", rv);
		return -EINVAL;
	}
	return 0;
}

static inline int xnl_msg_send(struct sk_buff *skb_tx, void *hdr,
				struct genl_info *info)
{
	int rv;

	genlmsg_end(skb_tx, hdr);

	rv = genlmsg_unicast(genl_info_net(info), skb_tx, info->snd_portid);
	if (rv)
		pr_info("send portid %d failed %d.\n", info->snd_portid, rv);

	return 0;
}

#ifdef DEBUG
static int xnl_dump_attrs(struct genl_info *info)
{
	int i;

	pr_info("snd_seq 0x%x, portid 0x%x.\n",
		info->snd_seq, info->snd_portid);
#if 0
	print_hex_dump_bytes("nlhdr", DUMP_PREFIX_OFFSET, info->nlhdr,
			sizeof(struct nlmsghdr));
	pr_info("\n");
	print_hex_dump_bytes("genlhdr", DUMP_PREFIX_OFFSET, info->genlhdr,
			sizeof(struct genlmsghdr));
	pr_info("\n");
#endif

	pr_info("nlhdr: len %u, type %u, flags 0x%x, seq 0x%x, pid %u.\n",
		info->nlhdr->nlmsg_len,
		info->nlhdr->nlmsg_type,
		info->nlhdr->nlmsg_flags,
		info->nlhdr->nlmsg_seq,
		info->nlhdr->nlmsg_pid);
	pr_info("genlhdr: cmd 0x%x %s, version %u, reserved 0x%x.\n",
		info->genlhdr->cmd, xnl_op_str[info->genlhdr->cmd],
		info->genlhdr->version,
		info->genlhdr->reserved);

	for (i = 0; i < XNL_ATTR_MAX; i++) {
		struct nlattr *na = info->attrs[i];

		if (na) {
			if (xnl_policy[i].type == NLA_NUL_STRING) {
				char *s = (char *)nla_data(na);

				if (s)
					pr_info("attr %d, %s, str %s.\n",
						i, xnl_attr_str[i], s);
				else
					pr_info("attr %d, %s, str NULL.\n",
						i, xnl_attr_str[i]);

			} else {
				u32 v = nla_get_u32(na);

				pr_info("attr %s, u32 0x%x.\n",
					xnl_attr_str[i], v);
			}
		}
	}

	return 0;
}
#else
#define xnl_dump_attrs(info)
#endif

static int xnl_respond_buffer(struct genl_info *info, char *buf, int buflen)
{
	struct sk_buff *skb;
	void *hdr;
	int rv;

	skb = xnl_msg_alloc(info->genlhdr->cmd, buflen, &hdr, info);
	if (!skb)
		return -ENOMEM;

	rv = xnl_msg_add_attr_str(skb, XNL_ATTR_GENMSG, buf);
	if (rv != 0) {
		pr_err("xnl_msg_add_attr_str() failed: 0x%x", rv);
		return rv;
	}

	rv = xnl_msg_send(skb, hdr, info);

	return rv;
}

static char *xnl_mem_alloc(int l, struct genl_info *info)
{
	char ebuf[XNL_ERR_BUFLEN];
	char *buf = kmalloc(l, GFP_KERNEL);
	int rv;

	if (buf) {
		memset(buf, 0, l);
		return buf;
	}

	pr_info("xnl OOM %d.\n", l);

	rv = snprintf(ebuf, XNL_ERR_BUFLEN, "ERR! xnl OOM %d.\n", l);
	ebuf[rv] = '\0';

	xnl_respond_buffer(info, ebuf, XNL_ERR_BUFLEN);

	return NULL;
}

static struct xlnx_pci_dev *xnl_rcv_check_xpdev(struct genl_info *info,
					struct qdma_dev_conf **conf_pp)
{
	u32 idx;
	struct xlnx_pci_dev *xpdev;
	struct qdma_dev_conf *conf;
	char ebuf[XNL_ERR_BUFLEN];
	int rv;

	if (info == NULL)
		return NULL;

	idx = nla_get_u32(info->attrs[XNL_ATTR_DEV_IDX]);

	xpdev = xpdev_find_by_idx(idx, ebuf, XNL_ERR_BUFLEN);
	if (xpdev) {
		conf = qdma_device_get_config(xpdev->dev_hndl, NULL, 0);
		if (conf)  {
			if (conf_pp)
				*conf_pp = conf;
			return xpdev;
		}
	}

	rv = snprintf(ebuf, XNL_ERR_BUFLEN, "ERR! qdma %u %s.\n",
			idx, xpdev ? "ERR" : "NO match");
	ebuf[rv] = '\0';

	xnl_respond_buffer(info, ebuf, XNL_ERR_BUFLEN);
	return NULL;
}

static int qconf_get(struct qdma_queue_conf *qconf, struct genl_info *info,
			char *buf, int buflen)
{
	u32 f = nla_get_u32(info->attrs[XNL_ATTR_QFLAG]);
	int len;

	if ((f & XNL_F_QMODE_ST) && (f & XNL_F_QMODE_MM)) {
		len = sprintf(buf, "ERR! Both ST and MM mode set.\n");
		buf[len] = '\0';
		goto err_out;
	} else if (!(f & XNL_F_QMODE_ST) && !(f & XNL_F_QMODE_MM)) {
		/* default to MM */
		f |= XNL_F_QMODE_MM;
	}

	if ((f & XNL_F_QDIR_H2C) && (f & XNL_F_QDIR_C2H)) {
		len = sprintf(buf, "ERR! Both H2C and C2H mode set.\n");
		buf[len] = '\0';
		goto err_out;
	} else if (!(f & XNL_F_QDIR_H2C) && !(f & XNL_F_QDIR_C2H)) {
		/* default to H2C */
		f |= XNL_F_QDIR_H2C;
	}

	memset(qconf, 0, sizeof(*qconf));
	//qconf.qsize = nla_get_u32(info->attrs[XNL_ATTR_QRNGSZ]);
	qconf->st = (f & XNL_F_QMODE_ST) ? 1 : 0;
	qconf->c2h = (f & XNL_F_QDIR_C2H) ? 1 : 0;

	qconf->qidx = nla_get_u32(info->attrs[XNL_ATTR_QIDX]);
	if (qconf->qidx == XNL_QIDX_INVALID)
		qconf->qidx = QDMA_QUEUE_IDX_INVALID;

	return 0;

err_out:
	xnl_respond_buffer(info, buf, buflen);
	return -EINVAL;
}

static struct xlnx_qdata *xnl_rcv_check_qidx(struct genl_info *info,
				struct xlnx_pci_dev *xpdev,
				struct qdma_queue_conf *qconf, char *buf,
				int buflen)
{
	char ebuf[XNL_ERR_BUFLEN];
	struct xlnx_qdata *qdata = xpdev_queue_get(xpdev, qconf->qidx,
					qconf->c2h, 1, ebuf, XNL_ERR_BUFLEN);

	if (!qdata) {
		int l =snprintf(ebuf, XNL_ERR_BUFLEN, "ERR! qidx %u invalid.\n",
				qconf->qidx);

		ebuf[l] = '\0';
		xnl_respond_buffer(info, ebuf, XNL_ERR_BUFLEN);
	}

	return qdata;
}

static int xnl_dev_list(struct sk_buff *skb2, struct genl_info *info)
{
	char *buf;
	int rv;

	if (info == NULL)
		return -EINVAL;

	xnl_dump_attrs(info);

	buf = xnl_mem_alloc(XNL_RESP_BUFLEN_MAX, info);
	if (!buf)
		return -ENOMEM;

	rv = xpdev_list_dump(buf, XNL_RESP_BUFLEN_MAX);
	if (rv < 0) {
		pr_err("xpdev_list_dump() failed: 0x%x", rv);
		goto free_msg_buff;
	}
	rv = xnl_respond_buffer(info, buf, XNL_RESP_BUFLEN_MAX);

free_msg_buff:
	kfree(buf);
	return rv;
}

static int xnl_dev_info(struct sk_buff *skb2, struct genl_info *info)
{
	struct sk_buff *skb;
	void *hdr;
	struct xlnx_pci_dev *xpdev;
	struct pci_dev *pdev;
	struct qdma_dev_conf *conf;
	int rv;

	if (info == NULL)
		return -EINVAL;

	xnl_dump_attrs(info);

	xpdev = xnl_rcv_check_xpdev(info, &conf);
	if (!xpdev)
		return -EINVAL;
	pdev = xpdev->pdev;

	skb = xnl_msg_alloc(XNL_CMD_DEV_INFO, 0, &hdr, info);
	if (!skb)
		return -ENOMEM;

	rv = xnl_msg_add_attr_uint(skb, XNL_ATTR_PCI_BUS,
				pdev->bus->number);
	if (rv < 0) {
		pr_err("xnl_msg_add_attr_uint() failed: 0x%x", rv);
		return rv;
	}
	rv = xnl_msg_add_attr_uint(skb, XNL_ATTR_PCI_DEV,
				PCI_SLOT(pdev->devfn));
	if (rv < 0) {
		pr_err("xnl_msg_add_attr_uint() failed: 0x%x", rv);
		return rv;
	}
	rv = xnl_msg_add_attr_uint(skb, XNL_ATTR_PCI_FUNC,
				PCI_FUNC(pdev->devfn));
	if (rv < 0) {
		pr_err("xnl_msg_add_attr_uint() failed: 0x%x", rv);
		return rv;
	}
	rv = xnl_msg_add_attr_uint(skb, XNL_ATTR_DEV_CFG_BAR,
				conf->bar_num_config);
	if (rv < 0) {
		pr_err("xnl_msg_add_attr_uint() failed: 0x%x", rv);
		return rv;
	}
	rv = xnl_msg_add_attr_uint(skb, XNL_ATTR_DEV_USR_BAR,
				conf->bar_num_user);
	if (rv < 0) {
		pr_err("xnl_msg_add_attr_uint() failed: 0x%x", rv);
		return rv;
	}
	rv = xnl_msg_add_attr_uint(skb, XNL_ATTR_DEV_QSET_MAX, conf->qsets_max);
	if (rv < 0) {
		pr_err("xnl_msg_add_attr_uint() failed: 0x%x", rv);
		return rv;
	}
	rv = xnl_msg_send(skb, hdr, info);
	return rv;
}

static int xnl_q_list(struct sk_buff *skb2, struct genl_info *info)
{
	struct xlnx_pci_dev *xpdev;
	char *buf;
	int rv;

	if (info == NULL)
		return 0;

	xnl_dump_attrs(info);

	xpdev = xnl_rcv_check_xpdev(info, NULL);
	if (!xpdev)
		return 0;

	buf = xnl_mem_alloc(XNL_RESP_BUFLEN_MAX, info);
	if (!buf)
		return -ENOMEM;

	rv = qdma_queue_list(xpdev->dev_hndl, buf, XNL_RESP_BUFLEN_MAX);
	if (rv < 0) {
		pr_err("qdma_queue_list() failed: 0x%x", rv);
		goto free_msg_buff;
	}
	rv = xnl_respond_buffer(info, buf, XNL_RESP_BUFLEN_MAX);

free_msg_buff:
	kfree(buf);
	return rv;
}

static int xnl_q_add(struct sk_buff *skb2, struct genl_info *info)
{
	struct xlnx_pci_dev *xpdev;
	struct qdma_queue_conf qconf;
	char buf[XNL_RESP_BUFLEN_MIN];
	int rv;

	if (info == NULL)
		return 0;

	xnl_dump_attrs(info);

	xpdev = xnl_rcv_check_xpdev(info, NULL);
	if (!xpdev)
		return 0;

	rv = qconf_get(&qconf, info, buf, XNL_RESP_BUFLEN_MIN);
	if (rv < 0)
		return rv;

	if (qconf.c2h && qconf.st)
	    qconf.st_c2h_wrb_desc_size =
			    nla_get_u8(info->attrs[XNL_ATTR_WRB_DESC_SIZE]);

	rv = xpdev_queue_add(xpdev, &qconf, buf, XNL_RESP_BUFLEN_MIN);
	if (rv < 0) {
		pr_err("xpdev_queue_add() failed: 0x%x", rv);
		return rv;
	}
	rv = xnl_respond_buffer(info, buf, XNL_RESP_BUFLEN_MIN);
	return rv;
}

static int xnl_q_start(struct sk_buff *skb2, struct genl_info *info)
{
	struct xlnx_pci_dev *xpdev;
	struct qdma_queue_conf qconf;
	char buf[XNL_RESP_BUFLEN_MIN];
	struct xlnx_qdata *qdata;
	int rv;

	if (info == NULL)
		return 0;

	xnl_dump_attrs(info);

	xpdev = xnl_rcv_check_xpdev(info, NULL);
	if (!xpdev)
		return -EINVAL;

	rv = qconf_get(&qconf, info, buf, XNL_RESP_BUFLEN_MIN);
	if (rv < 0)
		return rv;

	qdata = xnl_rcv_check_qidx(info, xpdev, &qconf, buf,
				XNL_RESP_BUFLEN_MIN);
	if (!qdata)
		return -EINVAL;

	rv = qdma_queue_start(xpdev->dev_hndl, qdata->qhndl, buf,
				XNL_RESP_BUFLEN_MIN);
	if (rv < 0) {
		pr_err("qdma_queue_start() failed: 0x%x", rv);
		return rv;
	}
	rv = xnl_respond_buffer(info, buf, XNL_RESP_BUFLEN_MIN);

	return rv;
}

static int xnl_q_stop(struct sk_buff *skb2, struct genl_info *info)
{
	struct xlnx_pci_dev *xpdev;
	struct qdma_queue_conf qconf;
	char buf[XNL_RESP_BUFLEN_MIN];
	struct xlnx_qdata *qdata;
	int rv;

	if (info == NULL)
		return 0;

	xnl_dump_attrs(info);

	xpdev = xnl_rcv_check_xpdev(info, NULL);
	if (!xpdev)
		return -EINVAL;

	rv = qconf_get(&qconf, info, buf, XNL_RESP_BUFLEN_MIN);
	if (rv < 0)
		return rv;
		
	qdata = xnl_rcv_check_qidx(info, xpdev, &qconf, buf,
				XNL_RESP_BUFLEN_MIN);
	if (!qdata)
		return -EINVAL;

	rv = qdma_queue_stop(xpdev->dev_hndl, qdata->qhndl, buf,
				XNL_RESP_BUFLEN_MIN);
	if (rv < 0) {
		pr_err("qdma_queue_stop() failed: 0x%x", rv);
		return rv;
	}
	rv = xnl_respond_buffer(info, buf, XNL_RESP_BUFLEN_MIN);
	return rv;
}

static int xnl_q_del(struct sk_buff *skb2, struct genl_info *info)
{
	struct xlnx_pci_dev *xpdev;
	struct qdma_queue_conf qconf;
	char buf[XNL_RESP_BUFLEN_MIN];
	int rv;

	if (info == NULL)
		return 0;

	xnl_dump_attrs(info);

	xpdev = xnl_rcv_check_xpdev(info, NULL);
	if (!xpdev)
		return -EINVAL;

	rv = qconf_get(&qconf, info, buf, XNL_RESP_BUFLEN_MIN);
	if (rv < 0)
		return rv;
		
	rv = xpdev_queue_delete(xpdev, qconf.qidx, qconf.c2h, buf,
				XNL_RESP_BUFLEN_MIN);
	if (rv < 0) {
		pr_err("xpdev_queue_delete() failed: 0x%x", rv);
		return rv;
	}
	rv = xnl_respond_buffer(info, buf, XNL_RESP_BUFLEN_MIN);
	return rv;
}

static int xnl_q_dump(struct sk_buff *skb2, struct genl_info *info)
{
	struct xlnx_pci_dev *xpdev;
	struct qdma_queue_conf qconf;
	struct xlnx_qdata *qdata;
	char *buf;
	char ebuf[XNL_RESP_BUFLEN_MIN];
	int rv;

	if (info == NULL)
		return 0;

	xnl_dump_attrs(info);

	xpdev = xnl_rcv_check_xpdev(info, NULL);
	if (!xpdev)
		return -EINVAL;

	rv = qconf_get(&qconf, info, ebuf, XNL_RESP_BUFLEN_MIN);
	if (rv < 0)
		return rv;
		
	qdata = xnl_rcv_check_qidx(info, xpdev, &qconf, ebuf,
				XNL_RESP_BUFLEN_MIN);
	if (!qdata)
		return -EINVAL;

	buf = xnl_mem_alloc(XNL_RESP_BUFLEN_MAX, info);
	if (!buf)
		return -ENOMEM;

	rv = qdma_queue_dump(xpdev->dev_hndl, qdata->qhndl, buf,
				XNL_RESP_BUFLEN_MAX);
	if (rv < 0) {
		pr_err("qdma_queue_dump() failed: 0x%x", rv);
		goto free_msg_buff;
	}
	rv = xnl_respond_buffer(info, buf, XNL_RESP_BUFLEN_MAX);

free_msg_buff:
	kfree(buf);
	return rv;
}

static int xnl_q_dump_desc(struct sk_buff *skb2, struct genl_info *info)
{
	struct xlnx_pci_dev *xpdev;
	struct qdma_queue_conf qconf;
	struct xlnx_qdata *qdata;
	u32 v1;
	u32 v2;
	char *buf;
	char ebuf[XNL_RESP_BUFLEN_MIN];
	int rv;

	if (info == NULL)
		return 0;

	xnl_dump_attrs(info);

	v1 = nla_get_u32(info->attrs[XNL_ATTR_RANGE_START]);
	v2 = nla_get_u32(info->attrs[XNL_ATTR_RANGE_END]);

	xpdev = xnl_rcv_check_xpdev(info, NULL);
	if (!xpdev)
		return -EINVAL;

	rv = qconf_get(&qconf, info, ebuf, XNL_RESP_BUFLEN_MIN);
	if (rv < 0)
		return rv;
		
	qdata = xnl_rcv_check_qidx(info, xpdev, &qconf, ebuf,
				XNL_RESP_BUFLEN_MIN);
	if (!qdata)
		return -EINVAL;

	buf = xnl_mem_alloc(XNL_RESP_BUFLEN_MAX, info);
	if (!buf) {
		rv = sprintf(ebuf, "%s OOM %d.\n",
				__func__, XNL_RESP_BUFLEN_MAX);

		ebuf[rv] = '\0';
		xnl_respond_buffer(info, ebuf, XNL_RESP_BUFLEN_MIN);
		return -ENOMEM;
	}

	rv = qdma_queue_dump_desc(xpdev->dev_hndl, qdata->qhndl, v1, v2, buf,
				 XNL_RESP_BUFLEN_MAX);
	if (rv < 0) {
		pr_err("qdma_queue_dump_desc() failed: 0x%x", rv);
		goto free_msg_buff;
	}
	rv = xnl_respond_buffer(info, buf, XNL_RESP_BUFLEN_MAX);

free_msg_buff:
	kfree(buf);
	return rv;
}

static int xnl_q_dump_wrb(struct sk_buff *skb2, struct genl_info *info)
{
	struct xlnx_pci_dev *xpdev;
	struct qdma_queue_conf qconf;
	struct xlnx_qdata *qdata;
	u32 v1;
	u32 v2;
	char *buf;
	char ebuf[XNL_RESP_BUFLEN_MIN];
	int rv;

	if (info == NULL)
		return 0;

	xnl_dump_attrs(info);

	v1 = nla_get_u32(info->attrs[XNL_ATTR_RANGE_START]);
	v2 = nla_get_u32(info->attrs[XNL_ATTR_RANGE_END]);

	xpdev = xnl_rcv_check_xpdev(info, NULL);
	if (!xpdev)
		return -EINVAL;

	rv = qconf_get(&qconf, info, ebuf, XNL_RESP_BUFLEN_MIN);
	if (rv < 0)
		return rv;
		
	qdata = xnl_rcv_check_qidx(info, xpdev, &qconf, ebuf,
				XNL_RESP_BUFLEN_MIN);
	if (!qdata)
		return -EINVAL;

	buf = xnl_mem_alloc(XNL_RESP_BUFLEN_MAX, info);
	if (!buf) {
		rv = sprintf(ebuf, "%s OOM %d.\n",
				__func__, XNL_RESP_BUFLEN_MAX);
		ebuf[rv] = '\0';
		xnl_respond_buffer(info, ebuf, XNL_RESP_BUFLEN_MIN);
		return -ENOMEM;
	}

	rv = qdma_queue_dump_wrb(xpdev->dev_hndl, qdata->qhndl, v1, v2, buf,
				 XNL_RESP_BUFLEN_MAX);
	if (rv < 0) {
		pr_err("qdma_queue_dump_wrb() failed: 0x%x", rv);
		goto free_msg_buff;
	}
	rv = xnl_respond_buffer(info, buf, XNL_RESP_BUFLEN_MAX);

free_msg_buff:
	kfree(buf);
	return rv;
}

int xlnx_nl_init(void)
{
	int rv;

	rv = genl_register_family(&xnl_family);
	if (rv)
		pr_info("genl_register_family failed %d.\n", rv);

	return rv;
}

void  xlnx_nl_exit(void)
{
	int rv;

	rv = genl_unregister_family(&xnl_family);
	if (rv)
		pr_info("genl_unregister_family failed %d.\n", rv);
}
