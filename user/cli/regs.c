#include <sys/mman.h>
#include "nl_user.h"

/*
 * Register I/O through mmap of BAR0.
 */

/* /sys/bus/pci/devices/0000:<bus>:<dev>.<func>/resource<bar#> */
#define get_syspath_bar_mmap(s, bus,dev,func,bar) \
	snprintf(s, sizeof(s), \
		"/sys/bus/pci/devices/0000:%02x:%02x.%x/resource%u", \
		bus, dev, func, bar)

static uint32_t *mmap_bar(char *fname, size_t len, int prot)
{
	int fd;
	uint32_t *bar;

	fd = open(fname, (prot & PROT_WRITE) ? O_RDWR : O_RDONLY);
	if (fd < 0)
		return NULL;

	bar = mmap(NULL, len, prot, MAP_SHARED, fd, 0);
	close(fd);

	return bar == MAP_FAILED ? NULL : bar;
}

static uint32_t reg_read_mmap(struct xdev_info *xdev, unsigned char barno,
				uint32_t addr)
{
	uint32_t val, *bar;
	char fname[256];

	get_syspath_bar_mmap(fname, xdev->bus, xdev->dev, xdev->func, barno);

	bar = mmap_bar(fname, addr + 4, PROT_READ);
	if (!bar)
		err(1, "register read");

	val = bar[addr / 4];
	munmap(bar, addr + 4);
	return le32toh(val);
}

static void reg_write_mmap(struct xdev_info *xdev, unsigned char barno,
				uint32_t addr, uint32_t val)
{
	uint32_t *bar;
	char fname[256];

	get_syspath_bar_mmap(fname, xdev->bus, xdev->dev, xdev->func, barno);

	bar = mmap_bar(fname, addr + 4, PROT_WRITE);
	if (!bar)
		err(1, "register write");

	bar[addr / 4] = htole32(val);
	munmap(bar, addr + 4);
}

static void reg_dump_mmap(struct xdev_info *xdev, unsigned char barno,
			struct xreg_info *reg_list, unsigned int max)
{
	struct xreg_info *xreg = reg_list;
	uint32_t *bar;
	char fname[256];

	get_syspath_bar_mmap(fname, xdev->bus, xdev->dev, xdev->func, barno);

	bar = mmap_bar(fname, max, PROT_READ);
	if (!bar)
		err(1, "register dump");

	for (xreg = reg_list; strlen(xreg->name); xreg++) {
		if (!xreg->len) {
			if (xreg->repeat) {
				int i;
				int cnt = xreg->repeat;
				uint32_t addr = xreg->addr;
				int step = xreg->step ? xreg->step : 4;

				for (i = 0; i < cnt; i++, addr += step) {
					uint32_t val = le32toh(bar[addr / 4]);
					char name[40]; 
					int l = sprintf(name, "%s_%d",
							xreg->name, i);

					name[l] = '\0';
					printf("[%#7x] %-47s %#-10x %u\n",
						addr, name, val, val);
				}
				
			} else {
				uint32_t addr = xreg->addr;
				uint32_t val = le32toh(bar[addr / 4]);

				printf("[%#7x] %-47s %#-10x %u\n",
					addr, xreg->name, val, val);
			}
		} else {
			uint32_t addr = xreg->addr;
			uint32_t val = le32toh(bar[addr / 4]);
			uint32_t v = (val >> xreg->shift) &
					((1 << xreg->len) - 1);

			printf("    %*u:%u %-47s %#-10x %u\n",
				xreg->shift < 10 ? 3 : 2,
				xreg->shift + xreg->len - 1,
				xreg->shift, xreg->name, v, v);
		}
	}

	munmap(bar, max);
}

static inline void print_seperator(void)
{
	char buffer[81];

	memset(buffer, '#', 80);
	buffer[80] = '\0';

	fprintf(stdout, "%s\n", buffer);
}

int reg_proc_cmd(struct xcmd_info *xcmd)
{
	struct xcmd_reg *regcmd = &xcmd->u.reg;
	struct xdev_info xdev;
	unsigned int mask = (1 << XNL_ATTR_PCI_BUS) | (1 << XNL_ATTR_PCI_DEV) |
			(1 << XNL_ATTR_PCI_FUNC) | (1 << XNL_ATTR_DEV_CFG_BAR) |
			(1 << XNL_ATTR_DEV_USR_BAR);
	unsigned int barno;
	uint32_t v;

	if ((xcmd->attr_mask & mask) != mask) {
		fprintf(stderr, "%s: device info missing, 0x%x/0x%x.\n",
			__FUNCTION__, xcmd->attr_mask, mask);
		return -EINVAL;
	}

	memset(&xdev, 0, sizeof(struct xdev_info));
	xdev.bus = xcmd->attrs[XNL_ATTR_PCI_BUS];
	xdev.dev = xcmd->attrs[XNL_ATTR_PCI_DEV];
	xdev.func = xcmd->attrs[XNL_ATTR_PCI_FUNC];
	xdev.config_bar = xcmd->attrs[XNL_ATTR_DEV_CFG_BAR];
	xdev.user_bar = xcmd->attrs[XNL_ATTR_DEV_USR_BAR];

	barno = (regcmd->sflags & XCMD_REG_F_BAR_SET) ?
			 regcmd->bar : xdev.config_bar;

	switch (xcmd->op) {
	case XNL_CMD_REG_RD:
		v = reg_read_mmap(&xdev, barno, regcmd->reg); 
		fprintf(stdout,
			"qdma%d, %02x:%02x.%02x, bar#%u, 0x%x = 0x%x.\n",
			xcmd->if_idx, xdev.bus, xdev.dev, xdev.func, barno,
			regcmd->reg, v);
		break;
	case XNL_CMD_REG_WRT:
		reg_write_mmap(&xdev, barno, regcmd->reg, regcmd->val); 
		v = reg_read_mmap(&xdev, barno, regcmd->reg); 
		fprintf(stdout,
			"qdma%d, %02x:%02x.%02x, bar#%u, reg 0x%x -> 0x%x, read back 0x%x.\n",
			xcmd->if_idx, xdev.bus, xdev.dev, xdev.func, barno,
			regcmd->reg, regcmd->val, v);
		break;
	case XNL_CMD_REG_DUMP:
	{
		extern struct xreg_info qdma_config_regs[];
		extern unsigned int qdma_config_bar_max_addr;
		extern unsigned int qdma_user_max_addr;
		extern struct xreg_info qdma_user_regs[];

		print_seperator();
		fprintf(stdout,
			"###\t\tqdma%d, pci %02x:%02x.%02x, reg dump\n",
			xcmd->if_idx, xdev.bus, xdev.dev, xdev.func);
		print_seperator();

		fprintf(stdout, "\nUSER BAR #%d\n", xdev.user_bar);
		reg_dump_mmap(&xdev, xdev.user_bar, qdma_user_regs,
				qdma_user_max_addr);

		fprintf(stdout, "\nCONFIG BAR #%d\n", xdev.config_bar);
		reg_dump_mmap(&xdev, xdev.config_bar, qdma_config_regs,
				qdma_config_bar_max_addr);
		break;
	}
	default:
		break;
	}
	return 0;
}
