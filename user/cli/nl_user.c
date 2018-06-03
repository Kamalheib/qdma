#include "nl_user.h"
#include "nl.h"

/* Generic macros for dealing with netlink sockets. Might be duplicated
 * elsewhere. It is recommended that commercial grade applications use
 * libnl or libnetlink and use the interfaces provided by the library
 */

static void xnl_close(struct xnl_cb *cb)
{
	close(cb->fd);
}

static int xnl_send(struct xnl_cb *cb, struct xnl_hdr *hdr)
{
	int rv;
	struct sockaddr_nl addr = {
		.nl_family = AF_NETLINK,
	};

	hdr->n.nlmsg_seq = cb->snd_seq;
	cb->snd_seq++;

	rv = sendto(cb->fd, (char *)hdr, hdr->n.nlmsg_len, 0,
			(struct sockaddr *)&addr, sizeof(addr));
        if (rv != hdr->n.nlmsg_len) {
		perror("nl send err");
		return -1;
	}

	return 0;
}

static int xnl_recv(struct xnl_cb *cb, struct xnl_hdr *hdr, int dlen, int print)
{
	int rv;
	struct sockaddr_nl addr = {
		.nl_family = AF_NETLINK,
	};

	memset(hdr, 0, sizeof(struct xnl_gen_msg) + dlen);

	rv = recv(cb->fd, hdr, dlen, 0);
	if (rv < 0) {
		perror("nl recv err");
		return -1;
	}
	/* as long as there is attribute, even if it is shorter than expected */
	if (!NLMSG_OK((&hdr->n), rv) && (rv <= sizeof(struct xnl_hdr))) {
		if (print)
			fprintf(stderr,
				"nl recv:, invalid message, cmd 0x%x, %d,%d.\n",
				hdr->g.cmd, dlen, rv);
		return -1;
	}

	if (hdr->n.nlmsg_type == NLMSG_ERROR) {
		if (print)
			fprintf(stderr, "nl recv, msg error, cmd 0x%x\n",
				hdr->g.cmd);
		return -1;
	}

	return 0;
}

static inline struct xnl_gen_msg *xnl_msg_alloc(int dlen)
{
	struct xnl_gen_msg *msg;

	msg = malloc(sizeof(struct xnl_gen_msg) + dlen);
	if (!msg) {
		fprintf(stderr, "%s: OOM, %d.\n", __FUNCTION__, dlen);
		return NULL;
	}

	memset(msg, 0, sizeof(struct xnl_gen_msg) + dlen);
	return msg;
}

static int xnl_connect(struct xnl_cb *cb, int vf)
{
	int fd;	
	struct sockaddr_nl addr;
	struct xnl_gen_msg *msg = xnl_msg_alloc(XNL_RESP_BUFLEN_MIN);
	struct xnl_hdr *hdr;
	struct nlattr *attr;
	int rv = -1;

	if (!msg) {
		fprintf(stderr, "%s, msg OOM.\n", __FUNCTION__);
		return -ENOMEM;
	}
	hdr = (struct xnl_hdr *)msg;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
	if (fd < 0) {
                perror("nl socket err");
		rv = fd;
		goto out;
        }
	cb->fd = fd;

	memset(&addr, 0, sizeof(struct sockaddr_nl));	
	addr.nl_family = AF_NETLINK;
	rv = bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_nl));
	if (rv < 0) {
		perror("nl bind err");
		goto out;
	}

	hdr->n.nlmsg_type = GENL_ID_CTRL;
	hdr->n.nlmsg_flags = NLM_F_REQUEST;
	hdr->n.nlmsg_pid = getpid();
	hdr->n.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);

        hdr->g.cmd = CTRL_CMD_GETFAMILY;
        hdr->g.version = XNL_VERSION;

	attr = (struct nlattr *)(msg->data);
	attr->nla_type = CTRL_ATTR_FAMILY_NAME;

	if (vf) {
        	attr->nla_len = strlen(XNL_NAME_VF) + 1 + NLA_HDRLEN;
        	strcpy((char *)(attr + 1), XNL_NAME_VF);

	} else {
        	attr->nla_len = strlen(XNL_NAME_PF) + 1 + NLA_HDRLEN;
        	strcpy((char *)(attr + 1), XNL_NAME_PF);
	}
        hdr->n.nlmsg_len += NLMSG_ALIGN(attr->nla_len);

	rv = xnl_send(cb, (struct xnl_hdr *)hdr);	
	if (rv < 0)
		goto out;

	rv = xnl_recv(cb, hdr, XNL_RESP_BUFLEN_MIN, 0);
	if (rv < 0)
		goto out;

#if 0
	/* family name */
        if (attr->nla_type == CTRL_ATTR_FAMILY_NAME)
		printf("family name: %s.\n", (char *)(attr + 1));
#endif

	attr = (struct nlattr *)((char *)attr + NLA_ALIGN(attr->nla_len));
	/* family ID */
        if (attr->nla_type == CTRL_ATTR_FAMILY_ID) {
		cb->family = *(__u16 *)(attr + 1);
//		printf("family id: 0x%x.\n", cb->family);
	}

	rv = 0;

out:
	free(msg);
	return rv;
}

static void xnl_msg_set_hdr(struct xnl_hdr *hdr, int family, int op)
{
	hdr->n.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
	hdr->n.nlmsg_type = family;
	hdr->n.nlmsg_flags = NLM_F_REQUEST;
	hdr->n.nlmsg_pid = getpid();

	hdr->g.cmd = op;
}

static int xnl_send_op(struct xnl_cb *cb, int op)
{
	struct xnl_hdr req;
	int rv;

	memset(&req, 0, sizeof(struct xnl_hdr));

	xnl_msg_set_hdr(&req, cb->family, op);

	rv = xnl_send(cb, &req);	

	return rv;
}

static int xnl_msg_add_int_attr(struct xnl_hdr *hdr, enum xnl_attr_t type,
				unsigned int v)
{
	struct nlattr *attr = (struct nlattr *)((char *)hdr + hdr->n.nlmsg_len);

        attr->nla_type = (__u16)type;
        attr->nla_len = sizeof(__u32) + NLA_HDRLEN;
	*(__u32 *)(attr+ 1) = v;

        hdr->n.nlmsg_len += NLMSG_ALIGN(attr->nla_len);
	return 0;
}

static int xnl_msg_add_str_attr(struct xnl_hdr *hdr, enum xnl_attr_t type,
				char *s)
{
	struct nlattr *attr = (struct nlattr *)((char *)hdr + hdr->n.nlmsg_len);
	int len = strlen(s);	

        attr->nla_type = (__u16)type;
        attr->nla_len = len + 1 + NLA_HDRLEN;

	strcpy((char *)(attr + 1), s);

        hdr->n.nlmsg_len += NLMSG_ALIGN(attr->nla_len);
	return 0;
}

static int recv_attrs(struct xnl_hdr *hdr, struct xcmd_info *xcmd)
{
	unsigned char *p = (unsigned char *)(hdr + 1);
	int maxlen = hdr->n.nlmsg_len - NLMSG_LENGTH(GENL_HDRLEN);

#if 0
	printf("nl recv, hdr len %d, data %d, gen op 0x%x, %s, ver 0x%x.\n",
		hdr->n.nlmsg_len, maxlen, hdr->g.cmd, xnl_op_str[hdr->g.cmd],
		hdr->g.version);
#endif

	xcmd->attr_mask = 0;
	while (maxlen > 0) {
		struct nlattr *na = (struct nlattr *)p;
		int len = NLA_ALIGN(na->nla_len);

		if (na->nla_type >= XNL_ATTR_MAX) {
			fprintf(stderr, "unknown attr type %d, len %d.\n",
				na->nla_type, na->nla_len);
			return -EINVAL;
		}

		xcmd->attr_mask |= 1 << na->nla_type;

		if (na->nla_type == XNL_ATTR_GENMSG) {
			printf("\n%s\n", (char *)(na + 1));

		} else if (na->nla_type == XNL_ATTR_DRV_INFO) {
			strncpy(xcmd->drv_str, (char *)(na + 1), 128);
		} else {
			xcmd->attrs[na->nla_type] = *(uint32_t *)(na + 1);
		}

		p += len;
		maxlen -= len;
	}

	return 0;
}

static int recv_nl_msg(struct xnl_hdr *hdr, struct xcmd_info *xcmd)
{
	unsigned int op = hdr->g.cmd;

	recv_attrs(hdr, xcmd);

	switch(op) {
	case XNL_CMD_DEV_LIST:
		break;
	case XNL_CMD_DEV_INFO:
		xcmd->config_bar = xcmd->attrs[XNL_ATTR_DEV_CFG_BAR];
		xcmd->user_bar = xcmd->attrs[XNL_ATTR_DEV_USR_BAR];
		xcmd->qmax = xcmd->attrs[XNL_ATTR_DEV_QSET_MAX];

		printf("qdma%s%d:\t%02x:%02x.%02x\t",
			xcmd->vf ? "vf" : "", xcmd->if_idx,
			xcmd->attrs[XNL_ATTR_PCI_BUS],
			xcmd->attrs[XNL_ATTR_PCI_DEV],
			xcmd->attrs[XNL_ATTR_PCI_FUNC]);
		printf("config bar: %d, user bar: %d, max #. QP: %d\n",
			xcmd->config_bar, xcmd->user_bar, xcmd->qmax);
		break;
	case XNL_CMD_Q_LIST:
		break;
	case XNL_CMD_Q_ADD:
		break;
	case XNL_CMD_Q_START:
		break;
	case XNL_CMD_Q_STOP:
		break;
	case XNL_CMD_Q_DEL:
		break;
	default:
		break;
	}

	return 0;
}


static int xnl_send_cmd(struct xnl_cb *cb, struct xcmd_info *xcmd)
{
	struct xnl_gen_msg *msg;
	struct xnl_hdr *hdr;
	struct nlattr *attr;
	int dlen = XNL_RESP_BUFLEN_MIN;
	int i;
	int rv;
	xnl_st_c2h_wrb_desc_size wrb_desc_size;

#if 0
	printf("%s: op %s, 0x%x, ifname %s.\n", __FUNCTION__,
		xnl_op_str[xcmd->op], xcmd->op, xcmd->ifname);
#endif
	if (xcmd->op == XNL_CMD_DEV_LIST || xcmd->op == XNL_CMD_Q_LIST ||
	    xcmd->op == XNL_CMD_Q_DUMP || xcmd->op == XNL_CMD_Q_DESC ||
	    xcmd->op == XNL_CMD_Q_WRB)
		dlen = XNL_RESP_BUFLEN_MAX;

	msg = xnl_msg_alloc(dlen);
	if (!msg) {
		fprintf(stderr, "%s: OOM, %s, op %s,0x%x.\n", __FUNCTION__,
			xcmd->ifname, xnl_op_str[xcmd->op], xcmd->op);
		return -ENOMEM;
	}

	hdr = (struct xnl_hdr *)msg;
	attr = (struct nlattr *)(msg->data);

	xnl_msg_set_hdr(hdr, cb->family, xcmd->op);

	xnl_msg_add_int_attr(hdr, XNL_ATTR_DEV_IDX, xcmd->if_idx); 

	switch(xcmd->op) {
        case XNL_CMD_DEV_LIST:
        case XNL_CMD_DEV_INFO:
        case XNL_CMD_Q_LIST:
		/* no parameter */
		break;
        case XNL_CMD_Q_ADD:
		xnl_msg_add_int_attr(hdr, XNL_ATTR_QIDX, xcmd->u.qparm.idx);
		xnl_msg_add_int_attr(hdr, XNL_ATTR_QFLAG, xcmd->u.qparm.flags);
		xnl_msg_add_int_attr(hdr, XNL_ATTR_QRNGSZ, xcmd->u.qparm.ringsz);
		xnl_msg_add_int_attr(hdr, XNL_ATTR_QBUFSZ, xcmd->u.qparm.bufsz);
		if ((xcmd->u.qparm.sflags & (1 << QPARM_WRBSZ)))
		        xnl_msg_add_int_attr(hdr,
		                             XNL_ATTR_WRB_DESC_SIZE,
		                             xcmd->u.qparm.entry_size);
		break;
        case XNL_CMD_Q_START:
        case XNL_CMD_Q_STOP:
        case XNL_CMD_Q_DEL:
        case XNL_CMD_Q_DUMP:
		xnl_msg_add_int_attr(hdr, XNL_ATTR_QIDX, xcmd->u.qparm.idx);
		xnl_msg_add_int_attr(hdr, XNL_ATTR_QFLAG, xcmd->u.qparm.flags);
		break;
        case XNL_CMD_Q_DESC:
        case XNL_CMD_Q_WRB:
		xnl_msg_add_int_attr(hdr, XNL_ATTR_QIDX, xcmd->u.qparm.idx);
		xnl_msg_add_int_attr(hdr, XNL_ATTR_QFLAG, xcmd->u.qparm.flags);
		xnl_msg_add_int_attr(hdr, XNL_ATTR_RANGE_START,
					xcmd->u.qparm.range_start);
		xnl_msg_add_int_attr(hdr, XNL_ATTR_RANGE_END,
					xcmd->u.qparm.range_end);
		break;
	default:
		break;
	}

	rv = xnl_send(cb, hdr);	
	if (rv < 0)
		goto out;

	rv = xnl_recv(cb, hdr, dlen, 1);
	if (rv < 0)
		goto out;

	rv = recv_nl_msg(hdr, xcmd);
out:
	free(msg);
	return rv;
}

int reg_proc_cmd(struct xcmd_info *xcmd);
int main(int argc, char *argv[])
{
	struct xnl_cb cb;
	struct xcmd_info xcmd;
	unsigned char op;
	int rv = 0;

	memset(&xcmd, 0, sizeof(xcmd));

	rv = parse_cmd(argc, argv, &xcmd);
	if (rv < 0)
		return rv;

#if 0
	printf("cmd op %s, 0x%x, ifname %s.\n",
		xnl_op_str[xcmd.op], xcmd.op, xcmd.ifname);
#endif

	memset(&cb, 0, sizeof(struct xnl_cb));

	if (xcmd.op == XNL_CMD_DEV_LIST) {
		/* try pf nl server */
		rv = xnl_connect(&cb, 0);
		if (!rv)
			rv = xnl_send_cmd(&cb, &xcmd);
		xnl_close(&cb);

		/* try vf nl server */
		memset(&cb, 0, sizeof(struct xnl_cb));
		rv = xnl_connect(&cb, 1);
		if (!rv)
			rv = xnl_send_cmd(&cb, &xcmd);
		xnl_close(&cb);

		goto close;
	}

	/* for all other command, query the target device info first */
	rv = xnl_connect(&cb, xcmd.vf);
	if (rv < 0)
		goto close;

 	op = xcmd.op;
	xcmd.op = XNL_CMD_DEV_INFO;
	rv = xnl_send_cmd(&cb, &xcmd);
	xcmd.op = op;
	if (rv < 0)
		goto close;

	if ((xcmd.op == XNL_CMD_REG_DUMP) || (xcmd.op == XNL_CMD_REG_RD) ||
	    (xcmd.op == XNL_CMD_REG_WRT))
		rv = reg_proc_cmd(&xcmd);
	else
		rv = xnl_send_cmd(&cb, &xcmd);

close:
	xnl_close(&cb);
	return rv;
}
