
#include "nl_user.h"
#include "version.h"

static const char *progname;

/*
 * qset add h2c mm h2c_id
 */
static void __attribute__((noreturn)) usage(FILE *fp)
{
	fprintf(fp, "Usage: %s [dev|qdma[vf]<N>] [operation] \n", progname);
	fprintf(fp, "\tdev [operation]: system wide FPGA operations\n");
	fprintf(fp, 
		"\t\tlist                             list all qdma functions\n");
	fprintf(fp,
		"\tqdma[N] [operation]: per QDMA FPGA operations\n");
	fprintf(fp,
		"\t\tq list                           list all queues\n"
		"\t\tq add idx <N> [mode <mm|st>] [dir <h2c|c2h>] [cdev <0|1>]\n"
		"\t\t                                 add a queue\n"
		"\t\t                                    *mode default to mm\n"
		"\t\t                                    *dir default to h2c\n"
		"\t\tq start idx <N> [dir <h2c|c2h>]  start a queue\n"
		"\t\tq start idx <N> dir [<h2c|c2h>]  start a queue\n"
		"\t\tq stop idx <N> dir [<h2c|c2h>]   stop a queue\n"
		"\t\tq del idx <N> dir [<h2c|c2h>]    delete a queue\n"
		"\t\tq dump idx <N> dir [<h2c|c2h>]   dump queue param\n"
		"\t\tq dump idx <N> dir [<h2c|c2h>] desc <x> <y>\n"
		"\t\t                                 dump desc ring entry x ~ y\n"
		"\t\tq dump idx <N> dir [<h2c|c2h>] wrb <x> <y>\n"
		"\t\t                                 dump wrb ring entry x ~ y\n"
		);
	fprintf(fp,
		"\t\treg dump                         register dump\n"
		"\t\treg read [bar <N>] <addr>        read a register\n"
		"\t\treg write [bar <N>] <addr> <val> write a register\n");

	exit(fp == stderr ? 1 : 0);
}

static int arg_read_int(char *s, uint32_t *v)
{
    char *p;

    *v = strtoul(s, &p, 0);
    if (*p) {
        warnx("bad parameter \"%s\", integer expected", s);
        return -EINVAL;
    }
    return 0;
}

static int parse_ifname(char *name, struct xcmd_info *xcmd)
{
	int rv;
	int len = strlen(name);
	int pos, i;
	uint32_t v;

	/* qdmaN of qdmavfN*/
	if (len >= 10) {
		warnx("interface name %s too long, expect qdma<N>.\n", name);
		return -EINVAL;
	}
	if (strncmp(name, "qdma", 4)) {
		warnx("bad interface name %s, expect qdma<N>.\n", name);
		return -EINVAL;
	}
	if (name[4] == 'v' && name[5] == 'f') {
		xcmd->vf = 1;
		pos = 6;
	} else {
		xcmd->vf = 0;
		pos = 4;
	}
	for (i = pos; i < len; i++) {
		if (!isdigit(name[i])) {
			warnx("%s unexpected <qdmaN>, %d.\n", name, i);
			return -EINVAL;
		}
	}

	rv = arg_read_int(name + pos, &v);
	if (rv < 0)
		return rv;

	xcmd->if_idx = v;
	return 0;
}

#define get_next_arg(argc, argv, i) \
	if (++(*i) >= argc) { \
		warnx("%s missing parameter after \"%s\".\n", __FUNCTION__, argv[--(*i)]); \
		return -EINVAL; \
	}

#define __get_next_arg(argc, argv, i) \
	if (++i >= argc) { \
		warnx("%s missing parameter aft \"%s\".\n", __FUNCTION__, argv[--i]); \
		return -EINVAL; \
	}

static int next_arg_read_int(int argc, char *argv[], int *i, unsigned int *v)
{
	get_next_arg(argc, argv, i);
	return arg_read_int(argv[*i], v);
}

static int next_arg_read_pair(int argc, char *argv[], int *start, char *s,
			unsigned int *v, int optional)
{
	int rv;
	int i = *start;

	/* string followed by an int */
	if ((i + 2) >= argc) {
		if (optional) {
			warnx("No optional parm %s after \"%s\".\n", s, argv[i]);
			return 0;
		} else {
			warnx("missing parameter after \"%s\".\n", argv[i]);
			return -EINVAL;
		}
	}

	__get_next_arg(argc, argv, i);

	if (!strcmp(s, argv[i])) {
		get_next_arg(argc, argv, &i);
		*start = i;
		return arg_read_int(argv[i], v);
	}

	warnx("bad parameter, \"%s\".\n", argv[i]);
	return -EINVAL;
}

static int parse_reg_cmd(int argc, char *argv[], int i, struct xcmd_info *xcmd)
{
	struct xcmd_reg	*regcmd = &xcmd->u.reg;
	int rv;

	/*
	 * reg dump
	 * reg read [bar <N>] <addr> 
	 * reg write [bar <N>] <addr> <val> 
	 */

	memset(regcmd, 0, sizeof(struct xcmd_reg));
	if (!strcmp(argv[i], "dump")) {
		xcmd->op = XNL_CMD_REG_DUMP;

		i++;

	} else if (!strcmp(argv[i], "read")) {
		xcmd->op = XNL_CMD_REG_RD;

		//xcmd->u.reg.bar = xdrv->config_bar;
		get_next_arg(argc, argv, &i);
		if (!strcmp(argv[i], "bar")) {
			rv = next_arg_read_int(argc, argv, &i, &regcmd->bar);
			if (rv < 0)
				return rv;
			regcmd->sflags |= XCMD_REG_F_BAR_SET;
			get_next_arg(argc, argv, &i);
		}
		rv = arg_read_int(argv[i], &regcmd->reg);
		if (rv < 0)
			return rv;
		regcmd->sflags |= XCMD_REG_F_REG_SET;

		i++;

	} else if (!strcmp(argv[i], "write")) {
		xcmd->op = XNL_CMD_REG_WRT;

		//xcmd->u.reg.bar = xdrv->config_bar;
		get_next_arg(argc, argv, &i);
		if (!strcmp(argv[i], "bar")) {
			rv = next_arg_read_int(argc, argv, &i, &regcmd->bar);
			if (rv < 0)
				return rv;
			regcmd->sflags |= XCMD_REG_F_BAR_SET;
			get_next_arg(argc, argv, &i);
		}
		rv = arg_read_int(argv[i], &xcmd->u.reg.reg);
		if (rv < 0)
			return rv;
		regcmd->sflags |= XCMD_REG_F_REG_SET;

		rv = next_arg_read_int(argc, argv, &i, &xcmd->u.reg.val);
		if (rv < 0)
			return rv;
		regcmd->sflags |= XCMD_REG_F_VAL_SET;

		i++;
	}

	return i;
}

static int read_range(int argc, char *argv[], int i, unsigned int *v1,
			unsigned int *v2)
{
	int rv;

	/* range */
	rv = arg_read_int(argv[i], v1);
	if (rv < 0)
		return rv;

	get_next_arg(argc, argv, &i);
	rv = arg_read_int(argv[i], v2);
	if (rv < 0)
		return rv;

	if (v2 < v1) {
		warnx("invalid range %u ~ %u.\n", *v1, *v2);
		return -EINVAL;
	}

	return ++i;
}

static char qparm_type_str[][8] = {
	"idx",
	"mode",
	"dir",
	"rngsz",
	"bufsz",
	"cdev",
	"desc",
	"wrb",
};

static int read_qparm(int argc, char *argv[], int i, struct xcmd_q_parm *qparm,
			unsigned int f_arg_required)
{
	int rv;
	uint32_t v1, v2;
	unsigned int f_arg_set = 0;;
	unsigned int mask;

	/*
	 * idx <val>
	 * ringsz <val>
	 * bufsz <val>
	 * mode <mm|st>
	 * dir <h2c|c2h>
	 * cdev <0|1>
	 * desc <x> <y>
	 * wrb <x> <y>
	 * wrbsz <0|1|2|3>
	 */

	qparm->idx = XNL_QIDX_INVALID;

	while (i < argc) {
		if (!strcmp(argv[i], "idx")) {
			rv = next_arg_read_int(argc, argv, &i, &v1);
			if (rv < 0)
				return rv;

			qparm->idx = v1;
			f_arg_set |= 1 << QPARM_IDX;
			i++;

		} else if (!strcmp(argv[i], "mode")) {
			get_next_arg(argc, argv, (&i));

			if (!strcmp(argv[i], "mm")) {
				qparm->flags |= XNL_F_QMODE_MM;
			} else if (!strcmp(argv[i], "st")) {
				qparm->flags |= XNL_F_QMODE_ST;
			} else {
				warnx("unknown q mode %s.\n", argv[i]);
				return -EINVAL;
			}
			f_arg_set |= 1 << QPARM_MODE;
			i++;

		} else if (!strcmp(argv[i], "dir")) {
			get_next_arg(argc, argv, (&i));

			if (!strcmp(argv[i], "h2c")) {
				qparm->flags |= XNL_F_QDIR_H2C;
			} else if (!strcmp(argv[i], "c2h")) {
				qparm->flags |= XNL_F_QDIR_C2H;
			} else {
				warnx("unknown q dir %s.\n", argv[i]);
				return -EINVAL;
			}
			f_arg_set |= 1 << QPARM_DIR;
			i++;

		} else if (!strcmp(argv[i], "cdev")) {
			rv = next_arg_read_int(argc, argv, &i, &v1);
			if (rv < 0)
				return rv;

			if (v1 != 0 && v1 != 1) {
				warnx("unknown q cdev %s, exp  <0|1>.\n",
					argv[i]);
				return -EINVAL;
			}

			if (v1)
				qparm->flags |= XNL_F_CDEV;

			f_arg_set |= 1 << QPARM_CDEV;
			i++;

		} else if (!strcmp(argv[i], "bufsz")) {
			rv = next_arg_read_int(argc, argv, &i, &v1);
			if (rv < 0)
				return rv;

			qparm->bufsz = v1;

			f_arg_set |= 1 << QPARM_BUFSZ;
			i++;
		
		} else if (!strcmp(argv[i], "ringsz")) {
			rv = next_arg_read_int(argc, argv, &i, &v1);
			if (rv < 0)
				return rv;

			qparm->ringsz = v1;
			f_arg_set |= 1 << QPARM_RNGSZ;
			i++;

		} else if (!strcmp(argv[i], "desc")) {
			get_next_arg(argc, argv, &i);
			rv = read_range(argc, argv, i, &qparm->range_start,
					&qparm->range_end);
			if (rv < 0)
				return rv;
			i = rv;
			f_arg_set |= 1 << QPARM_DESC;

		} else if (!strcmp(argv[i], "wrb")) {
		    get_next_arg(argc, argv, &i);
		    rv = read_range(argc, argv, i, &qparm->range_start,
			    &qparm->range_end);
		    if (rv < 0)
			return rv;
		    i = rv;
		    f_arg_set |= 1 << QPARM_WRB;

		} else if (!strcmp(argv[i], "wrbsz")) {
		    get_next_arg(argc, argv, &i);
		    sscanf(argv[i], "%hhu", &qparm->entry_size);
		    f_arg_set |= 1 << QPARM_WRBSZ;
		    i++;
		} else {
			warnx("unknown q parameter %s.\n", argv[i]);
			return -EINVAL;
		}
	}
	/* check for any missing mandatory parameters */
	mask = f_arg_set & f_arg_required;
	if (mask != f_arg_required) {
		int i;
		unsigned int bit_mask = 1;

		mask = (mask ^ f_arg_required) & f_arg_required;	

		for (i = 0; i < QPARM_MAX; i++, bit_mask <<= 1) {
			if (!(bit_mask & f_arg_required))
				continue;
			warnx("missing q parameter %s.\n", qparm_type_str[i]);
			return -EINVAL;
		}
	}

	/* error checking of parameter values */
	if ((f_arg_set & 1 << QPARM_MODE)) {
		mask = XNL_F_QMODE_MM | XNL_F_QMODE_ST;
		if ((qparm->flags & mask) == mask) {
			warnx("mode mm/st cannot be combined.\n");
			return -EINVAL;
		}
	} else {
		/* default to MM */
		f_arg_set |= 1 << QPARM_MODE;
		qparm->flags |=  XNL_F_QMODE_MM;
	}

	if ((f_arg_set & 1 << QPARM_DIR)) {
		mask = XNL_F_QDIR_H2C | XNL_F_QDIR_C2H;
		if ((qparm->flags & mask) == mask) {
			warnx("dir h2c/c2h cannot be combined.\n");
			return -EINVAL;
		}
	} else {
		/* default to H2C */
		f_arg_set |= 1 << QPARM_DIR;
		qparm->flags |=  XNL_F_QDIR_H2C;
	}

	qparm->sflags = f_arg_set;

	return argc;
}

static int parse_q_cmd(int argc, char *argv[], int i, struct xcmd_info *xcmd)
{
	struct xcmd_q_parm *qparm = &xcmd->u.qparm;
	int rv;

	/*
	 * q list
	 * q add idx <N> mode <mm|st> [dir <h2c|c2h>] [cdev <0|1>] [wrbsz <0|1|2|3>]
	 * q start idx <N> dir <h2c|c2h>
	 * q stop idx <N> dir <h2c|c2h>
	 * q del idx <N> dir <h2c|c2h>
	 * q dump idx <N> dir <h2c|c2h>
	 * q dump idx <N> dir <h2c|c2h> desc <x> <y>
	 * q dump idx <N> dir <h2c|c2h> wrb <x> <y>
	 */

	if (!strcmp(argv[i], "list")) {
		xcmd->op = XNL_CMD_Q_LIST;
		return ++i;
	} else if (!strcmp(argv[i], "add")) {
		xcmd->op = XNL_CMD_Q_ADD;
		get_next_arg(argc, argv, &i);
		rv = read_qparm(argc, argv, i, qparm, 0);

		if (qparm->flags & (XNL_F_QDIR_C2H | XNL_F_QMODE_ST) ==
				(XNL_F_QDIR_C2H | XNL_F_QMODE_ST)) {
				if (!(qparm->sflags & (1 << QPARM_WRBSZ))) {
						/* default to rsv */
						qparm->entry_size = 3;
						qparm->sflags |=
							(1 << QPARM_WRBSZ);
				}
		}
	} else if (!strcmp(argv[i], "start")) {
		xcmd->op = XNL_CMD_Q_START;
		get_next_arg(argc, argv, &i);
		rv = read_qparm(argc, argv, i, qparm, (1 << QPARM_IDX));

	} else if (!strcmp(argv[i], "stop")) {
		xcmd->op = XNL_CMD_Q_STOP;
		get_next_arg(argc, argv, &i);
		rv = read_qparm(argc, argv, i, qparm, (1 << QPARM_IDX));

	} else if (!strcmp(argv[i], "del")) {
		xcmd->op = XNL_CMD_Q_DEL;
		get_next_arg(argc, argv, &i);
		rv = read_qparm(argc, argv, i, qparm, (1 << QPARM_IDX));

	} else if (!strcmp(argv[i], "dump")) {
		xcmd->op = XNL_CMD_Q_DUMP;
		get_next_arg(argc, argv, &i);
		rv = read_qparm(argc, argv, i, qparm, (1 << QPARM_IDX));
	}
	
	if (rv < 0)
		return rv;
	i = rv;

	if (xcmd->op == XNL_CMD_Q_DUMP) {
		unsigned int mask = (1 << QPARM_DESC) | (1 << QPARM_WRB);

		if ((qparm->sflags & mask) == mask) {
			warnx("dump wrb/desc cannot be combined.\n");
			return -EINVAL;
		}
		if ((qparm->sflags & (1 << QPARM_DESC)))
			xcmd->op = XNL_CMD_Q_DESC;
		else if ((qparm->sflags & (1 << QPARM_WRB)))
			xcmd->op = XNL_CMD_Q_WRB;
	}

	return i;
}

static int parse_dev_cmd(int argc, char *argv[], int i, struct xcmd_info *xcmd)
{
	if (!strcmp(argv[i], "list")) {
		xcmd->op = XNL_CMD_DEV_LIST;
		i++;
	}
	return i;
}

int parse_cmd(int argc, char *argv[], struct xcmd_info *xcmd)
{
	char *ifname;
	int i;
	int rv;

	memset(xcmd, 0, sizeof(struct xcmd_info));

	progname = argv[0];

	if (argc == 1) 
		usage(stderr);

	if (argc == 2) {
		if (!strcmp(argv[1], "?") || !strcmp(argv[1], "-h") ||
		    !strcmp(argv[1], "help") || !strcmp(argv[1], "--help"))
			usage(stdout);

		if (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version")) {
			printf("%s version %s\n", PROGNAME, VERSION);
			printf("%s\n", COPYRIGHT);
			exit(0);
		}
	}

	if (!strcmp(argv[1], "dev")) {
		rv = parse_dev_cmd(argc, argv, 2, xcmd);
		goto done;
	}

	/* which dma fpga */
	ifname = argv[1];
	rv = parse_ifname(ifname, xcmd);
	if (rv < 0)
		return rv;

	if (argc == 2) {
		rv = 2;
		xcmd->op = XNL_CMD_DEV_INFO;
		goto done;
	}

	i = 3;
	if (!strcmp(argv[2], "reg")) {
		rv = parse_reg_cmd(argc, argv, i, xcmd);
	} else if (!strcmp(argv[2], "q")) {
		rv = parse_q_cmd(argc, argv, i, xcmd);
	} else {
		warnx("bad parameter \"%s\".\n", argv[2]);
		return -EINVAL;
	}

done:
	if (rv < 0)
		return rv;
	i = rv;
	
	if (i < argc) {
		warnx("unexpected parameter \"%s\".\n", argv[i]);
		return -EINVAL;
	}
	return 0;
}
