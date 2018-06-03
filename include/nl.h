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

#ifndef __XDMA_NL_H__
#define __XDMA_NL_H__

#define XNL_NAME_PF		"xnl_pf"	/* no more than 15 characters */
#define XNL_NAME_VF		"xnl_vf"
#define XNL_VERSION		0x1

#define XNL_RESP_BUFLEN_MIN	256
#define XNL_RESP_BUFLEN_MAX	1024
#define XNL_ERR_BUFLEN		64

/*
 * Q parameters
 */
#define XNL_QIDX_INVALID	0xFFFF
enum q_parm_type {
	QPARM_IDX,
        QPARM_MODE,
        QPARM_DIR,
        QPARM_RNGSZ,
        QPARM_BUFSZ,
        QPARM_CDEV,
        QPARM_DESC,
        QPARM_WRB,
        QPARM_WRBSZ,

        QPARM_MAX,
};

#define XNL_F_QMODE_ST	0x1
#define XNL_F_QMODE_MM	0x2
#define XNL_F_QDIR_H2C	0x4
#define XNL_F_QDIR_C2H	0x8
#define XNL_F_CDEV	0x10

/*
 * attributes (variables):
 * the index in this enum is used as a reference for the type,
 * userspace application has to indicate the corresponding type
 * the policy is used for security considerations
 */
enum xnl_attr_t {
	XNL_ATTR_GENMSG,
	XNL_ATTR_DRV_INFO,

	XNL_ATTR_DEV_IDX,	/* qdmaN */
	XNL_ATTR_PCI_BUS,
	XNL_ATTR_PCI_DEV,
	XNL_ATTR_PCI_FUNC,

	XNL_ATTR_DEV_CFG_BAR,
	XNL_ATTR_DEV_USR_BAR,
	XNL_ATTR_DEV_QSET_MAX,

	XNL_ATTR_REG_BAR_NUM,
	XNL_ATTR_REG_ADDR,
	XNL_ATTR_REG_VAL,

	XNL_ATTR_QIDX,
	XNL_ATTR_QFLAG,
	XNL_ATTR_QRNGSZ,
	XNL_ATTR_QBUFSZ,

	XNL_ATTR_WRB_DESC_SIZE,

	XNL_ATTR_RANGE_START,
	XNL_ATTR_RANGE_END,

	XNL_ATTR_MAX,
};

typedef enum {
	XNL_ST_C2H_WRB_DESC_SIZE_8B,
	XNL_ST_C2H_WRB_DESC_SIZE_16B,
	XNL_ST_C2H_WRB_DESC_SIZE_32B,
	XNL_ST_C2H_WRB_DESC_SIZE_RSVD
} xnl_st_c2h_wrb_desc_size;

static const char xnl_attr_str[][12] = {
	"GENMSG",	/* XNL_ATTR_GENMSG */
	"DRV_INFO",	/* XNL_ATTR_DRV_INFO */

	"DEV_IDX",	/* XNL_ATTR_DEV_IDX */

	"DEV_PCIBUS",	/* XNL_ATTR_PCI_BUS */
	"DEV_PCIDEV",	/* XNL_ATTR_PCI_DEV */
	"DEV_PCIFUNC",	/* XNL_ATTR_PCI_FUNC */

	"DEV_CFG_BAR",	/* XNL_ATTR_DEV_CFG_BAR */
	"DEV_USR_BAR",	/* XNL_ATTR_DEV_USER_BAR */
	"DEV_QSETMAX",	/* XNL_ATTR_DEV_QSET_MAX */

	"REG_BAR",	/* XNL_ATTR_REG_BAR_NUM */
	"REG_ADDR",	/* XNL_ATTR_REG_ADDR */
	"REG_VAL",	/* XNL_ATTR_REG_VAL */

	"QFLAG",	/* XNL_ATTR_QFLAG */
	"QRINGSZ",	/* XNL_ATTR_QRNGSZ */
	"QBUFSZ",	/* XNL_ATTR_QBUFSZ */
	"QIDX",		/* XNL_ATTR_QIDX */

	"RANGE_START",	/* XNL_ATTR_RANGE_START */
	"RANGE_END",	/* XNL_ATTR_RANGE_END */
};

/* commands, 0 ~ 0x7F */
enum xnl_op_t {
	XNL_CMD_DEV_LIST,
	XNL_CMD_DEV_INFO,

	XNL_CMD_REG_DUMP,
	XNL_CMD_REG_RD,
	XNL_CMD_REG_WRT,

	XNL_CMD_Q_LIST,
	XNL_CMD_Q_ADD,
	XNL_CMD_Q_START,
	XNL_CMD_Q_STOP,
	XNL_CMD_Q_DEL,
	XNL_CMD_Q_DUMP,
	XNL_CMD_Q_DESC,
	XNL_CMD_Q_WRB,

	XNL_CMD_MAX,
};

static const char xnl_op_str[][12] = {
	"DEV_LIST",	/* XNL_CMD_DEV_LIST */
	"DEV_INFO",	/* XNL_CMD_DEV_INFO */

	"REG_DUMP",	/* XNL_CMD_REG_DUMP */
	"REG_READ",	/* XNL_CMD_REG_RD */
	"REG_WRITE",	/* XNL_CMD_REG_WRT */

	"Q_LIST",	/* XNL_CMD_Q_LIST */
	"Q_ADD",	/* XNL_CMD_Q_ADD */
	"Q_START",	/* XNL_CMD_Q_START */
	"Q_STOP",	/* XNL_CMD_Q_STOP */
	"Q_DEL",	/* XNL_CMD_Q_DEL */
	"Q_DUMP",	/* XNL_CMD_Q_DUMP */
	"Q_DESC",	/* XNL_CMD_Q_DESC */
	"Q_WRB",	/* XNL_CMD_Q_WRB */
};

#endif /* ifndef __XDMA_NL_H__ */
