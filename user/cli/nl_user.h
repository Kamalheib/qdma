#ifndef __NL_USER_H__
#define __NL_USER_H__

#include <endian.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <byteswap.h>
#include <inttypes.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/genetlink.h>

#include "nl.h"

typedef __uint8_t	u8;
typedef __uint16_t	u16;
typedef __uint32_t	u32;
typedef unsigned long long u64;


struct xdev_info {
	unsigned char bus;
	unsigned char dev;
	unsigned char func;
	unsigned char config_bar;
	unsigned char user_bar;
};

struct xcmd_reg {
	unsigned int sflags;
#define XCMD_REG_F_BAR_SET	0x1
#define XCMD_REG_F_REG_SET	0x2
#define XCMD_REG_F_VAL_SET	0x4
	unsigned int bar;
	unsigned int reg;
	unsigned int val;
};

struct xcmd_q_parm {
	unsigned int sflags;
	uint32_t flags;
	uint32_t ringsz;
	uint32_t bufsz;
	uint32_t idx;
	uint32_t range_start;
	uint32_t range_end;
	unsigned char entry_size;
};

struct xcmd_info {
	unsigned char vf:1;
	unsigned char op:7;
	unsigned char if_idx;
	unsigned char config_bar;
	unsigned char user_bar;
	unsigned short qmax;
	char ifname[8];
	union {
		struct xcmd_reg reg;
		struct xcmd_q_parm qparm;
	} u;
	uint32_t attr_mask;
	uint32_t attrs[XNL_ATTR_MAX];
	char drv_str[128];
};

struct xreg_info {
	const char name[32];
	uint32_t addr;
	unsigned int repeat;
	unsigned int step;
	unsigned char shift;
	unsigned char len;
	unsigned char filler[2];
};

struct xnl_cb {
	int fd;
	unsigned short family;
	unsigned int snd_seq;
	unsigned int rcv_seq;
};


/*
 * netlink message
 */
struct xnl_hdr {
	struct nlmsghdr n;
	struct genlmsghdr g;
};

struct xnl_gen_msg {
	struct xnl_hdr hdr;
	char data[0];
};

int parse_cmd(int argc, char *argv[], struct xcmd_info *xcmd);

#endif /* ifndef __NL_USER_H__ */
