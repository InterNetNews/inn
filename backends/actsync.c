/* @(#) $Id$ */
/* @(#) Under RCS control in /usr/local/news/src/inn/local/RCS/actsync.c,v */
/*
 * actsync - sync or merge two active files
 *
 * usage:
 *    actsync [-A][-b hostid][-d hostid][-g max][-i ignore_file][-I hostid][-k]
 *	      [-l hostid][-m][-n name][-o fmt][-p %][-q hostid][-s size]
 *	      [-t hostid][-T][-v verbose_lvl][-w sec][-z sec]
 *	      [host1] host2
 *
 *	-A		use authentication to server
 *	-b hostid	ignore *.bork.bork.bork groups from:	  (def: -b 0)
 *			    0   from neither host
 *			    1	from host1
 *			    2	from host2
 *			    12	from host1 and host2
 *			    21	from host1 and host2
 *	-d hostid	ignore groups with all numeric components (def: -d 0)
 *	-g max		ignore group >max levels (0=dont ignore)  (def: -g 0)
 *	-i ignore_file	file with list/types of groups to ignore  (def: no file)
 *	-I hostid	ignore_file applies only to hostid	  (def: -I 12)
 *	-k		keep host1 groups with errors  	 	  (def: remove)
 *	-l hostid	flag =group problems as errors	 	  (def: -l 12)
 *	-m		merge, keep group not on host2	 	  (def: sync)
 *	-n name		name given to ctlinnd newgroup commands   (def: actsync)
 *	-o fmt		type of output:				  (def: -o c)
 *			    a	output groups in active format
 *			    a1	like 'a', but output ignored non-err host1 grps
 *			    ak	like 'a', keep host2 hi/low values on new groups
 *			    aK	like 'a', use host2 hi/low values always
 *			    a1k	like both 'a1' and 'ak'
 *			    a1K	like both 'a1' and 'aK'
 *			    ak1	like both 'a1' and 'ak'
 *			    aK1	like both 'a1' and 'aK'
 *			    c	output in ctlinnd change commands
 *			    x	no output, safely exec ctlinnd commands
 *			    xi	no output, safely exec commands interactively
 *	-p %		min % host1 lines unchanged allowed	  (def: -p 96)
 *	-q hostid	silence errors from a host (see -b)	  (def: -q 0)
 *	-s size		ignore names longer than size (0=no lim)  (def: -s 0)
 *	-t hostid	ignore bad top level groups from:(see -b) (def: -t 2)
 *	-T		no new hierarchies                  	  (def: allow)
 *	-v verbose_lvl	verbosity level				  (def: -v 0)
 *			    0   no debug or status reports
 *			    1   summary if work done
 *			    2   summary & actions (if exec output) only if done
 *			    3   summary & actions (if exec output)
 *			    4   debug output plus all -v 3 messages
 *	-w sec		wait sec seconds before ctlinnd timing out (def: -w 30)
 *	-z sec		sleep sec seconds per exec if -o x	  (def: -z 4)
 *	host1		host to be changed 	            (def: local server)
 *	host2		reference host used in merge
 */
/* 
 * By: Landon Curt Noll  	chongo@toad.com		(chongo was here /\../\)
 *
 * Copyright (c) Landon Curt Noll, 1996.
 * All rights reserved.
 *
 * Permission to use and modify is hereby granted so long as this 
 * notice remains.  Use at your own risk.  No warranty is implied.
 */

#include "config.h"
#include "clibrary.h"
#include "portable/wait.h"
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>
#include <signal.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/hashtab.h"
#include "inn/qio.h"
#include "inn/libinn.h"
#include "inn/paths.h"

static const char usage[] = "\
Usage: actsync [-A][-b hostid][-d hostid][-i ignore_file][-I hostid][-k]\n\
        [-l hostid][-m][-n name][-o fmt][-p min_%_unchg][-q hostid]\n\
        [-s size][-t hostid][-T][-v verbose_lvl][-w sec][-z sec]\n\
        [host1] host2\n\
\n\
    -A          use authentication to server\n\
    -b hostid   ignore *.bork.bork.bork groups from:    (def: -b 0)\n\
                0       from neither host\n\
                1       from host1\n\
                2       from host2\n\
                12      from host1 and host2\n\
                21      from host1 and host2\n\
    -d hostid   ignore grps with all numeric components (def: -d 0)\n\
    -g max      ignore group >max levels (0=don't)      (def: -g 0)\n\
    -i file     file with groups to ignore              (def: no file)\n\
    -I hostid   ignore_file applies only to hostid      (def: -I 12)\n\
    -k          keep host1 groups with errors           (def: remove)\n\
    -l hostid   flag =group problems as errors          (def: -l 12)\n\
    -m          merge, keep group not on host2          (def: sync)\n\
    -n name     name given to ctlinnd newgroup cmds     (def: actsync)\n\
    -o fmt      type of output:                         (def: -o c)\n\
                a       output groups in active format\n\
                a1      like 'a', but output ignored non-err host1 grps\n\
                ak      like 'a', keep host2 hi/low values on new groups\n\
                aK      like 'a', use host2 hi/low values always\n\
                a1k     like both 'a1' and 'ak'\n\
                a1K     like both 'a1' and 'aK'\n\
                ak1     like both 'a1' and 'ak'\n\
                aK1     like both 'a1' and 'aK'\n\
                c       output in ctlinnd change commands\n\
                x       no output, safely exec ctlinnd commands\n\
                xi      no output, safely exec commands interactively\n\
    -p %        min % host1 lines unchanged allowed     (def: -p 96)\n\
    -q hostid   silence errors from a host (see -b)     (def: -q 0)\n\
    -s size     ignore names > than size (0=no lim)     (def: -s 0)\n\
    -t hostid   ignore bad top level grps from: (see -b)(def: -t 2)\n\
    -T          no new hierarchies                      (def: allow)\n\
    -v level    verbosity level                         (def: -v 0)\n\
                0       no debug or status reports\n\
                1       summary if work done\n\
                2       summary & actions (if exec output) only if done\n\
                3       summary & actions (if exec output)\n\
                4       debug output plus all -v 3 messages\n\
    -w sec      wait sec seconds before ctlinnd timing out (def: -w 30)\n\
    -z sec      sleep sec seconds per exec if -o x      (def: -z 4)\n\
\n\
    host1       host to be changed                      (def: local server)\n\
    host2       reference host used in merge\n";


/*
 * pat - internal ignore/check pattern
 *
 * A pattern, derived from an ignore file, will determine if a group
 * will be checked if it is on both hosts or ignored altogether.
 *
 * The type related to the 4th field of an active file.  Types may
 * currently be one of [ymjnx=].  If '=' is one of the types, an
 * optional equivalence pattern may be given in the 'epat' element.
 *
 * For example, to ignore "foo.bar.*", if it is junked or equated to
 * a group of the form "alt.*.foo.bar.*":
 *
 *	x.pat = "foo.bar.*";
 *	x.type = "j=";
 *	x.epat = "alt.*.foo.bar.*";
 *	x.ignore = 1;
 *
 * To further check "foo.bar.mod" if it is moderated:
 *
 *	x.pat = "foo.bar.mod";
 *	x.type = "m";
 *	x.epat = NULL;
 *	x.ignore = 0;
 *
 * The 'i' value means ignore, 'c' value means 'compare'.   The last pattern
 * that matches a group determines the fate of the group.  By default all
 * groups are included.
 */
struct pat {
    char *pat;		/* newsgroup pattern */
    int type_match;	/* 1 => match only if group type matches */
    int y_type;		/* 1 => match if a 'y' type group */
    int m_type;		/* 1 => match if a 'm' type group */
    int n_type;		/* 1 => match if a 'n' type group */
    int j_type;		/* 1 => match if a 'j' type group */
    int x_type;		/* 1 => match if a 'x' type group */
    int eq_type;	/* 1 => match if a 'eq' type group */
    char *epat;		/* =pattern to match, if non-NULL and = is in type */
    int ignore;		/* 0 => check matching group, 1 => ignore it */
};

/* internal representation of an active line */
struct grp {
    int ignore;		/* ignore reason, 0 => not ignore (see below) */
    int hostid;		/* HOSTID this group is from */
    int linenum;	/* >0 => active line number, <=0 => not a line */
    int output;		/* 1 => output to produce the merged active file */
    int remove;		/* 1 => remove this group */
    char *name;		/* newsgroup name */
    char *hi;		/* high article string */
    char *low;		/* low article string */
    char *type;		/* newsgroup type string */
    const char *outhi;	/* output high article string */
    const char *outlow;	/* output low article string */
    char *outtype;	/* output newsgroup type string */
};

/* structure used in the process of looking for =group type problems */
struct eqgrp {
    int skip;		/* 1 => skip this entry */
    struct grp *g;	/* =group that is being examined */
    char *eq;		/* current equivalence name */
};

/*
 * These ignore reasons are listed in order severity; from mild to severe.
 */
#define NOT_IGNORED	0x0000	/* newsgroup has not been ignored */
#define CHECK_IGNORE	0x0001	/* ignore file ignores this entry */
#define CHECK_TYPE	0x0002	/* group type is ignored */
#define CHECK_BORK	0x0004	/* group is a *.bork.bork.bork group */
#define CHECK_HIER	0x0008	/* -T && new group's hierarchy does not exist */
#define ERROR_LONGLOOP	0x0010	/* =name refers to long =grp chain or cycle */
#define ERROR_EQLOOP	0x0020	/* =name refers to itself in some way */
#define ERROR_NONEQ	0x0040	/* =name does not refer to a valid group */
#define ERROR_DUP	0x0080	/* newsgroup is a duplicate of another */
#define ERROR_EQNAME	0x0100	/* =name is a bad group name */
#define ERROR_BADTYPE	0x0200	/* newsgroup type is invalid */
#define ERROR_BADNAME	0x0400	/* newsgroup name is invalid */
#define ERROR_FORMAT	0x0800	/* entry line is malformed */

#define IS_IGNORE(ign) ((ign) & (CHECK_IGNORE|CHECK_TYPE|CHECK_BORK|CHECK_HIER))
#define IS_ERROR(ign) ((ign) & ~(CHECK_IGNORE|CHECK_TYPE|CHECK_BORK|CHECK_HIER))

#define NOHOST 0		/* neither host1 nor host2 */
#define HOSTID1 1		/* entry from the first host */
#define HOSTID2 2		/* entry from the second host */

#define CHUNK 5000		/* number of elements to alloc at a time */

#define TYPES "ymjnx="		/* group types (1st char of 4th active fld) */
#define TYPECNT (sizeof(TYPES)-1)

#define DEF_HI   "0000000000"	/* default hi string value for new groups */
#define DEF_LOW  "0000000001"	/* default low string value for new groups */
#define WATER_LEN 10		/* string length of hi/low water mark */

#define DEF_NAME "actsync"	/* default name to use for ctlinnd newgroup */

#define MIN_UNCHG (double)96.0	/* min % of host1 lines unchanged allowed */

#define DEV_NULL "/dev/null"	/* path to the bit bucket */
#define CTLINND_NAME "ctlinnd"	/* basename of ctlinnd command */

#define READ_SIDE 0		/* read side of a pipe */
#define WRITE_SIDE 1		/* write side of a pipe */

#define EQ_LOOP 16		/* give up if =eq loop/chain is this long */
#define NOT_REACHED 127		/* exit value if unable to get active files */

#define NEWGRP_EMPTY 0		/* no new group dir was found */
#define NEWGRP_NOCHG 1		/* new group dir found but no hi/low change */
#define NEWGRP_CHG 2		/* new group dir found but no hi/low change */

/* -b macros */
#define BORK_CHECK(hostid)  \
    ((hostid == HOSTID1 && bork_host1_flag) || \
     (hostid == HOSTID2 && bork_host2_flag))

/* -d macros */
#define NUM_CHECK(hostid)  \
    ((hostid == HOSTID1 && num_host1_flag) || \
     (hostid == HOSTID2 && num_host2_flag))

/* -t macros */
#define TOP_CHECK(hostid)  \
    ((hostid == HOSTID1 && t_host1_flag) || \
     (hostid == HOSTID2 && t_host2_flag))

/* -o output types */
#define OUTPUT_ACTIVE 1		/* output in active file format */
#define OUTPUT_CTLINND 2	/* output in ctlinnd change commands */
#define OUTPUT_EXEC 3		/* no output, safely exec commands */
#define OUTPUT_IEXEC 4		/* no output, exec commands interactively */

/* -q macros */
#define QUIET(hostid)  \
    ((hostid == HOSTID1 && quiet_host1) || (hostid == HOSTID2 && quiet_host2))

/* -v verbosity level */
#define VER_MIN 0		/* minimum -v level */
#define VER_NONE 0		/* no -v output */
#define VER_SUMM_IF_WORK 1	/* output summary if actions were performed */
#define VER_REPT_IF_WORK 2	/* output summary & actions only if performed */
#define VER_REPORT 3		/* output summary & actions performed */
#define VER_FULL 4		/* output all summary, actins and debug */
#define VER_MAX 4		/* maximum -v level */
#define D_IF_SUMM (v_flag >= VER_SUMM_IF_WORK) /* true => give summary always */
#define D_REPORT (v_flag >= VER_REPT_IF_WORK)  /* true => give reports */
#define D_BUG (v_flag == VER_FULL)	       /* true => debug processing */
#define D_SUMMARY (v_flag >= VER_REPORT)       /* true => give summary always */

/* flag and arg related defaults */
int bork_host1_flag = 0; 	/* 1 => -b 1 or -b 12 or -b 21 given */
int bork_host2_flag = 0; 	/* 1 => -b 2 or -b 12 or -b 21 given */
int num_host1_flag = 0; 	/* 1 => -d 1 or -d 12 or -d 21 given */
int num_host2_flag = 0; 	/* 1 => -d 2 or -d 12 or -d 21 given */
char *ign_file = NULL;	 	/* default ignore file */
int ign_host1_flag = 1;		/* 1 => -i ign_file applies to host1 */
int ign_host2_flag = 1;		/* 1 => -i ign_file applies to host2 */
int g_flag = 0;			/* ignore grps deeper than > g_flag, 0=>dont */
int k_flag = 0;		 	/* 1 => -k given */
int l_host1_flag = HOSTID1;	/* HOSTID1 => host1 =group error detection */
int l_host2_flag = HOSTID2;	/* HOSTID2 => host2 =group error detection */
int m_flag = 0;			/* 1 => merge active files, don't sync */
const char *new_name = DEF_NAME;	/* ctlinnd newgroup name */
int o_flag = OUTPUT_CTLINND;	/* default output type */
double p_flag = MIN_UNCHG;	/* min % host1 lines allowed to be unchanged */
int host1_errs = 0;		/* errors found in host1 active file */
int host2_errs = 0;		/* errors found in host2 active file */
int quiet_host1 = 0; 		/* 1 => -q 1 or -q 12 or -q 21 given */
int quiet_host2 = 0;	 	/* 1 => -q 2 or -q 12 or -q 21 given */
int s_flag = 0;			/* max group size (length), 0 => do not check */
int t_host1_flag = 0;		/* 1 => -t 1 or -t 12 or -t 21 given */
int t_host2_flag = 1;		/* 1 => -t 2 or -t 12 or -t 21 given */
int no_new_hier = 0;		/* 1 => -T; no new hierarchies */
int host2_hilow_newgrp = 0;	/* 1 => use host2 hi/low on new groups */
int host2_hilow_all = 0;	/* 1 => use host2 hi/low on all groups */
int host1_ign_print = 0;	/* 1 => print host1 ignored groups too */
int v_flag = 0;			/* default verbosity level */
int w_flag = 30;        /* sleep w_flag sec before ctlinnd timing out */
int z_flag = 4;			/* sleep z_flag sec per exec if -o x */
int A_flag = 0;         /* 1 => authentication before LIST command */

/* forward declarations */
static void process_args(int argc, char *argv[], char **host1, char **host2);
static struct grp *get_active(char *host, int hostid, int *len, struct grp *,
                              int *errs);
static int bad_grpname(char *name, int num_chk);
static struct pat *get_ignore(char *filename, int *len);
static void ignore(struct grp *, int grplen, struct pat *, int iglen);
static int merge_cmp(const void *, const void *);
static void merge_grps(struct grp *, int grplen, char *host1, char *host2);
static int active_cmp(const void *, const void *);
static void output_grps(struct grp *, int grplen);
static void error_mark(struct grp *, int grplen, int hostid);
static int eq_merge_cmp(const void *, const void *);
static int mark_eq_probs(struct grp *, int grplen, int hostid, char *host1,
                         char *host2);
static int exec_cmd(int mode, const char *cmd, char *grp, char *type,
                    const char *who);
static int new_top_hier(char *name, struct hash *existing_hier);
static const void *string_key(const void *entry);
static bool string_equal(const void *key, const void *entry);

int
main(int argc, char *argv[])
{
    struct grp *grp;		/* struct grp array for host1 & host2 */
    struct pat *ignor;		/* ignore list from ignore file */
    int grplen;			/* length of host1/host2 group array */
    int iglen;			/* length of ignore list */
    char *host1;		/* host to change */
    char *host2;		/* comparison host */

    /* First thing, set up our identity. */
    message_program_name = "actsync";

    /* Read in default info from inn.conf. */
    if (!innconf_read(NULL))
        exit(1);
    process_args(argc, argv, &host1, &host2);

    /* obtain the active files */
    grp = get_active(host1, HOSTID1, &grplen, NULL, &host1_errs);
    grp = get_active(host2, HOSTID2, &grplen, grp, &host2_errs);

    /* ignore groups from both active files, if -i */
    if (ign_file != NULL) {

	/* read in the ignore file */
	ignor = get_ignore(ign_file, &iglen);

	/* ignore groups */
	ignore(grp, grplen, ignor, iglen);
    }

    /* compare groups from both hosts */
    merge_grps(grp, grplen, host1, host2);

    /* mark for removal, error groups from host1 if -e */
    if (! k_flag) {

	/* mark error groups for removal */
	error_mark(grp, grplen, HOSTID1);
    }

    /* output result of merge */
    output_grps(grp, grplen);

    /* all done */
    exit(0);
}

/*
 * process_args - process the command line arguments
 *
 * given:
 *	argc	arg count
 *	argv	the args
 *	host1	name of first host (may be 2nd if -R)
 *	host2	name of second host2 *may be 1st if -R)
 */
static void
process_args(int argc, char *argv[], char **host1, char **host2)
{
    char *def_serv = NULL;	/* name of default server */
    int i;

    /* parse args */
    while ((i = getopt(argc,argv,"Ab:d:g:i:I:kl:mn:o:p:q:s:t:Tv:w:z:")) != EOF) {
	switch (i) {
	case 'A':
	    A_flag = 1;
	    break;
	case 'b':		/* -b {0|1|2|12|21} */
	    switch (atoi(optarg)) {
	    case 0:
		bork_host1_flag = 0;
		bork_host2_flag = 0;
		break;
	    case 1:
		bork_host1_flag = 1;
		break;
	    case 2:
		bork_host2_flag = 1;
		break;
	    case 12:
	    case 21:
		bork_host1_flag = 1;
		bork_host2_flag = 1;
		break;
	    default:
                warn("-b option must be 0, 1, 2, 12, or 21");
                die("%s", usage);
	    }
	    break;
	case 'd':		/* -d {0|1|2|12|21} */
	    switch (atoi(optarg)) {
	    case 0:
		num_host1_flag = 0;
		num_host2_flag = 0;
		break;
	    case 1:
		num_host1_flag = 1;
		break;
	    case 2:
		num_host2_flag = 1;
		break;
	    case 12:
	    case 21:
		num_host1_flag = 1;
		num_host2_flag = 1;
		break;
	    default:
                warn("-d option must be 0, 1, 2, 12, or 21");
		die("%s", usage);
	    }
	    break;
	case 'g':		/* -g max */
	    g_flag = atoi(optarg);
	    break;
	case 'i':		/* -i ignore_file */
	    ign_file = optarg;
	    break;
	case 'I':		/* -I {0|1|2|12|21} */
	    switch (atoi(optarg)) {
	    case 0:
		ign_host1_flag = 0;
		ign_host2_flag = 0;
		break;
	    case 1:
		ign_host1_flag = 1;
		ign_host2_flag = 0;
		break;
	    case 2:
		ign_host1_flag = 0;
		ign_host2_flag = 1;
		break;
	    case 12:
	    case 21:
		ign_host1_flag = 1;
		ign_host2_flag = 1;
		break;
	    default:
                warn("-I option must be 0, 1, 2, 12, or 21");
		die("%s", usage);
	    }
	    break;
	case 'k':		/* -k */
	    k_flag = 1;
	    break;
	case 'l':		/* -l {0|1|2|12|21} */
	    switch (atoi(optarg)) {
	    case 0:
		l_host1_flag = NOHOST;
		l_host2_flag = NOHOST;
		break;
	    case 1:
		l_host1_flag = HOSTID1;
		l_host2_flag = NOHOST;
		break;
	    case 2:
		l_host1_flag = NOHOST;
		l_host2_flag = HOSTID2;
		break;
	    case 12:
	    case 21:
		l_host1_flag = HOSTID1;
		l_host2_flag = HOSTID2;
		break;
	    default:
                warn("-l option must be 0, 1, 2, 12, or 21");
		die("%s", usage);
	    }
	    break;
	case 'm':		/* -m */
	    m_flag = 1;
	    break;
	case 'n':		/* -n name */
	    new_name = optarg;
	    break;
	case 'o':		/* -o out_type */
	    switch (optarg[0]) {
	    case 'a':
		o_flag = OUTPUT_ACTIVE;
		switch (optarg[1]) {
		case '1':
		    switch(optarg[2]) {
		    case 'K':	/* -o a1K */
			host1_ign_print = 1;
			host2_hilow_all = 1;
			host2_hilow_newgrp = 1;
			break;
		    case 'k':	/* -o a1k */
			host1_ign_print = 1;
			host2_hilow_newgrp = 1;
			break;
		    default:	/* -o a1 */
			host1_ign_print = 1;
			break;
		    }
		    break;
		case 'K':
		    switch(optarg[2]) {
		    case '1':	/* -o aK1 */
			host1_ign_print = 1;
			host2_hilow_all = 1;
			host2_hilow_newgrp = 1;
			break;
		    default:	/* -o aK */
			host2_hilow_all = 1;
			host2_hilow_newgrp = 1;
			break;
		    };
		    break;
		case 'k':
		    switch(optarg[2]) {
		    case '1':	/* -o ak1 */
			host1_ign_print = 1;
			host2_hilow_newgrp = 1;
			break;
		    default:	/* -o ak */
			host2_hilow_newgrp = 1;
			break;
		    };
		    break;
		case '\0':	/* -o a */
		    break;
		default:
                    warn("-o type must be a, a1, ak, aK, ak1, aK1, a1k or a1K");
		    die("%s", usage);
		}
		break;
	    case 'c':
		o_flag = OUTPUT_CTLINND;
		break;
	    case 'x':
		if (optarg[1] == 'i') {
		    o_flag = OUTPUT_IEXEC;
		} else {
		    o_flag = OUTPUT_EXEC;
		}
		break;
	    default:
                warn("-o type must be a, a1, ak, aK, ak1, aK1, a1k, a1K, c, x, or xi");
		die("%s", usage);
	    }
	    break;
	case 'p':		/* -p %_min_host1_change */
	    /* parse % into [0,100] */
	    p_flag = atof(optarg);
	    if (p_flag > (double)100.0) {
		p_flag = (double)100.0;
	    } else if (p_flag < (double)0.0) {
		p_flag = (double)0.0;
	    }
	    break;
	case 'q':		/* -q {0|1|2|12|21} */
	    switch (atoi(optarg)) {
	    case 0:
		quiet_host1 = 0;
		quiet_host2 = 0;
		break;
	    case 1:
		quiet_host1 = 1;
		break;
	    case 2:
		quiet_host2 = 1;
		break;
	    case 12:
	    case 21:
		quiet_host1 = 1;
		quiet_host2 = 1;
		break;
	    default:
                warn("-q option must be 0, 1, 2, 12, or 21");
		die("%s", usage);
	    }
	    break;
	case 's':		/* -s size */
	    s_flag = atoi(optarg);
	    break;
	case 't':		/* -t {0|1|2|12|21} */
	    switch (atoi(optarg)) {
	    case 0:
		t_host1_flag = NOHOST;
		t_host2_flag = NOHOST;
		break;
	    case 1:
		t_host1_flag = HOSTID1;
		t_host2_flag = NOHOST;
		break;
	    case 2:
		t_host1_flag = NOHOST;
		t_host2_flag = HOSTID2;
		break;
	    case 12:
	    case 21:
		t_host1_flag = HOSTID1;
		t_host2_flag = HOSTID2;
		break;
	    default:
                warn("-t option must be 0, 1, 2, 12, or 21");
		die("%s", usage);
	    }
	    break;
	case 'T':		/* -T */
	    no_new_hier = 1;
	    break;
	case 'v':		/* -v verbose_lvl */
	    v_flag = atoi(optarg);
	    if (v_flag < VER_MIN || v_flag > VER_MAX) {
                warn("-v level must be >= %d and <= %d", VER_MIN, VER_MAX);
		die("%s", usage);
	    }
	    break;
	case 'w':		/* -w sec */
	    w_flag = atoi(optarg);
        if (w_flag < 0) {
            warn("-w option must be a positive integer");
            die("%s", usage);
        }
	    break;
	case 'z':		/* -z sec */
	    z_flag = atoi(optarg);
	    break;
	default:
            warn("unknown flag");
	    die("%s", usage);
	}
    }

    /* process the remaining args */
    argc -= optind;
    argv += optind;
    *host1 = NULL;
    switch (argc) {
    case 1:
	/* assume host1 is the local server */
	*host2 = argv[0];
	break;
    case 2:
	*host1 = argv[0];
	*host2 = argv[1];
	break;
    default:
        warn("expected 1 or 2 host args, found %d", argc);
	die("%s", usage);
    }

    /* determine default host name if needed */
    if (*host1 == NULL || strcmp(*host1, "-") == 0) {
	def_serv = innconf->server;
	*host1 = def_serv;
    }
    if (*host2 == NULL || strcmp(*host2, "-") == 0) {
	def_serv = innconf->server;
	*host2 = def_serv;
    }
    if (*host1 == NULL || *host2 == NULL)
        die("unable to determine default server name");
    if (D_BUG && def_serv != NULL)
        warn("STATUS: using default server: %s", def_serv);

    /* processing done */
    return;
}

/*
 * get_active - get an active file from a host
 *
 * given:
 *	host	host to contact or file to read, NULL => local server
 *	hostid	HOST_ID of host
 *	len	pointer to length of grp return array
 *	grp	existing host array to add, or NULL
 *	errs	count of lines that were found to have some error
 *
 * returns;
 *	Pointer to an array of grp structures describing each active entry.
 *	Does not return on fatal error.
 *
 * If host starts with a '/' or '.', then it is assumed to be a local file.
 * In that case, the local file is opened and read.
 */
static struct grp *
get_active(char *host, int hostid, int *len, struct grp *grp, int *errs)
{
    FILE *active;		/* stream for fetched active data */
    FILE *FromServer;		/* stream from server */
    FILE *ToServer;		/* stream to server */
    QIOSTATE *qp;		/* QIO active state */
    char buff[8192+1];		/* QIO buffer */
    char *line;			/* the line just read */
    struct grp *ret;		/* array of groups to return */
    struct grp *cur;		/* current grp entry being formed */
    int max;			/* max length of ret */
    int cnt;			/* number of entries read */
    int ucnt;			/* number of entries to be used */
    int namelen;		/* length of newsgroup name */
    int is_file;		/* 1 => host is actually a filename */
    int num_check;		/* true => check for all numeric components */
    char *rhost;
    int rport;
    char *p;
    int i;

    /* firewall */
    if (len == NULL)
        die("internal error #1: len is NULL");
    if (errs == NULL)
        die("internal error #2: errs in NULL");
    if (D_BUG)
        warn("STATUS: obtaining active file from %s", host);

    /* setup return array if needed */
    if (grp == NULL) {
        ret = xmalloc(CHUNK * sizeof(struct grp));
	max = CHUNK;
	*len = 0;

    /* or prep to use the existing array */
    } else {
	ret = grp;
	max = ((*len + CHUNK-1)/CHUNK)*CHUNK;
    }

    /* check for host being a filename */
    if (host != NULL && (host[0] == '/' || host[0] == '.')) {

	/* note that host is actually a file */
	is_file = 1;

	/* setup to read the local file quickly */
	if ((qp = QIOopen(host)) == NULL)
            sysdie("cannot open active file");

    /* case: host is a hostname or NULL (default server) */
    } else {

	/* note that host is actually a hostname or NULL */
	is_file = 0;

        /* prepare remote host variables */
	if ((p = strchr(host, ':')) != NULL) {
		rport = atoi(p + 1);
		*p = '\0';
		rhost = xstrdup(host);
		*p = ':';
	} else {
		rhost = xstrdup(host);
		rport = NNTP_PORT;
	}

	/* open a connection to the server */
	buff[0] = '\0';
	if (NNTPconnect(rhost, rport, &FromServer, &ToServer, buff,
                        sizeof(buff)) < 0)
            die("cannot connect to server: %s",
                buff[0] ? buff : strerror(errno));

        if (A_flag && NNTPsendpassword(rhost, FromServer, ToServer) < 0)
            die("cannot authenticate to server");

	free(rhost);

	/* get the active data from the server */
	active = CAlistopen(FromServer, ToServer, NULL);
	if (active == NULL)
            sysdie("cannot retrieve data");

	/* setup to read the retrieved data quickly */
	if ((qp = QIOfdopen((int)fileno(active))) == NULL)
            sysdie("cannot read temp file");
    }

    /* scan server's output, processing appropriate lines */
    num_check = NUM_CHECK(hostid);
    for (cnt=0, ucnt=0; (line = QIOread(qp)) != NULL; ++(*len), ++cnt) {

	/* expand return array if needed */
	if (*len >= max) {
	    max += CHUNK;
            ret = xrealloc(ret, sizeof(struct grp) * max);
	}

	/* setup the next return element */
	cur = &ret[*len];
	cur->ignore = NOT_IGNORED;
	cur->hostid = hostid;
	cur->linenum = cnt+1;
	cur->output = 0;
	cur->remove = 0;
	cur->name = NULL;
	cur->hi = NULL;
	cur->low = NULL;
	cur->type = NULL;
	cur->outhi = NULL;
	cur->outlow = NULL;
	cur->outtype = NULL;

	/* obtain a copy of the current line */
        cur->name = xstrdup(line);

	/* get the group name */
	if ((p = strchr(cur->name, ' ')) == NULL) {
	    if (!QUIET(hostid))
                warn("line %d from %s is malformed, skipping line", cnt + 1,
                     host);

	    /* don't form an entry for this group */
	    --(*len);
	    continue;
	}
	*p = '\0';
	namelen = p - cur->name;

	/* find the other 3 fields, ignore if not found */
	cur->hi = p+1;
	if ((p = strchr(p + 1, ' ')) == NULL) {
	    if (!QUIET(hostid))
                warn("skipping malformed line %d (field 2) from %s", cnt + 1,
                     host);

	    /* don't form an entry for this group */
	    --(*len);
	    continue;
	}
	*p = '\0';
	cur->low = p+1;
	if ((p = strchr(p + 1, ' ')) == NULL) {
	    if (!QUIET(hostid))
                warn("skipping malformed line %d (field 3) from %s", cnt + 1,
                     host);

	    /* don't form an entry for this group */
	    --(*len);
	    continue;
	}
	*p = '\0';
	cur->type = p+1;
	if ((p = strchr(p + 1, ' ')) != NULL) {
	    if (!QUIET(hostid))
                warn("skipping line %d from %s, it has more than 4 fields",
                     cnt + 1, host);

	    /* don't form an entry for this group */
	    --(*len);
	    continue;
	}

	/* check for bad group name */
	if (bad_grpname(cur->name, num_check)) {
	    if (!QUIET(hostid))
                warn("line %d <%s> from %s has a bad newsgroup name",
                     cnt + 1, cur->name, host);
	    cur->ignore |= ERROR_BADNAME;
	    continue;
	}

	/* check for long name if requested */
	if (s_flag > 0 && strlen(cur->name) > (size_t)s_flag) {
	    if (!QUIET(hostid))
                warn("line %d <%s> from %s has a name that is too long",
                     cnt + 1, cur->name, host);
	    cur->ignore |= ERROR_BADNAME;
	    continue;
	}

	/* look for only a bad top level element if the proper -t was given */
	if (!TOP_CHECK(hostid)) {

	    /* look for a '.' in the name */
	    if (strcmp(cur->name, "junk") != 0 && 
	        strcmp(cur->name, "control") != 0 && 
	        strcmp(cur->name, "to") != 0 && 
	        strcmp(cur->name, "test") != 0 && 
	        strcmp(cur->name, "general") != 0 && 
		strchr(cur->name, '.') == NULL) {
		if (!QUIET(hostid))
                    warn("line %d <%s> from %s is an invalid top level name",
                         cnt + 1, cur->name, host);
		cur->ignore |= ERROR_BADNAME;
		continue;
	    }
	}

	/* look for *.bork.bork.bork groups if the proper -b was given */
	if (BORK_CHECK(cur->hostid)) {
	    int elmlen;		/* length of element */
	    char *q;		/* beyond end of element */

	    /* scan the name backwards */
	    q = &(cur->name[namelen]);
	    for (p = &(cur->name[namelen-1]); p >= cur->name; --p) {
		/* if '.', see if this is a bork element */
		if (*p == '.') {
		    /* see if the bork element is short enough */
		    elmlen = q-p;
		    if (3*elmlen <= q-cur->name) {
			/* look for a triple match */
			if (strncmp(p,p-elmlen,elmlen) == 0 &&
			    strncmp(p,p-(elmlen*2),elmlen) == 0) {
			    /* found a *.bork.bork.bork group */
			    cur->ignore |= CHECK_BORK;
			    break;
			}
		    }
		    /* note the end of a new element */
		    q = p;
		}
	    }
	}

	/* 
	 * check for bad chars in the hi water mark 
	 */
	for (p=cur->hi, i=0; *p && isascii(*p) && isdigit((int)*p); ++p, ++i) {
	}
	if (*p) {
	    if (!QUIET(hostid))
                warn("line %d <%s> from %s has non-digits in hi water",
                     cnt + 1, cur->name, cur->hi);
	    cur->ignore |= ERROR_FORMAT;
	    continue;
	}

	/*
	 * check for excessive hi water length
	 */
	if (i > WATER_LEN) {
	    if (!QUIET(hostid))
                warn("line %d <%s> from %s hi water len: %d < %d",
                     cnt + 1, cur->name, cur->hi, i, WATER_LEN);
	    cur->ignore |= ERROR_FORMAT;
	    continue;
	}

	/*
	 * if the hi water length is too small, malloc and resize
	 */
	if (i != WATER_LEN) {
            p = xmalloc(WATER_LEN + 1);
	    memcpy(p, cur->hi, ((i > WATER_LEN) ? WATER_LEN : i)+1);
	}

	/* 
	 * check for bad chars in the low water mark 
	 */
	for (p=cur->low, i=0; *p && isascii(*p) && isdigit((int)*p); ++p, ++i) {
	}
	if (*p) {
	    if (!QUIET(hostid))
                warn("line %d <%s> from %s has non-digits in low water",
		     cnt + 1, cur->name, cur->low);
	    cur->ignore |= ERROR_FORMAT;
	    continue;
	}

	/*
	 * check for excessive low water length
	 */
	if (i > WATER_LEN) {
	    if (!QUIET(hostid))
                warn("line %d <%s> from %s low water len: %d < %d",
		     cnt + 1, cur->name, cur->hi, i, WATER_LEN);
	    cur->ignore |= ERROR_FORMAT;
	    continue;
	}

	/*
	 * if the low water length is too small, malloc and resize
	 */
	if (i != WATER_LEN) {
            p = xmalloc(WATER_LEN + 1);
	    memcpy(p, cur->low, ((i > WATER_LEN) ? WATER_LEN : i)+1);
	}

	/* check for a bad group type */
	switch (cur->type[0]) {
	case 'y':
		/* of COURSE: collabra has incompatible flags. but it	*/
		/* looks like they can be fixed easily enough.		*/
		if (cur->type[1] == 'g') {
			cur->type[1] = '\0';
		}
	case 'm':
	case 'j':
	case 'n':
	case 'x':
	    if (cur->type[1] != '\0') {
		if (!QUIET(hostid))
                    warn("line %d <%s> from %s has a bad newsgroup type",
                         cnt + 1, cur->name, host);
		cur->ignore |= ERROR_BADTYPE;
	    }
	    break;
	case '=':
	    if (cur->type[1] == '\0') {
		if (!QUIET(hostid))
                    warn("line %d <%s> from %s has an empty =group name",
                         cnt + 1, cur->name, host);
		cur->ignore |= ERROR_BADTYPE;
	    }
	    break;
	default:
	    if (!QUIET(hostid))
                warn("line %d <%s> from %s has an unknown newsgroup type",
                     cnt + 1, cur->name, host);
	    cur->ignore |= ERROR_BADTYPE;
	    break;
	}
	if (cur->ignore & ERROR_BADTYPE) {
	    continue;
	}

	/* if an = type, check for bad = name */
	if (cur->type[0] == '=' && bad_grpname(&(cur->type[1]), num_check)) {
	    if (!QUIET(hostid))
                warn("line %d <%s> from %s is equivalenced to a bad name:"
                     " <%s>", cnt+1, cur->name, host,
		     (cur->type) ? cur->type : "NULL");
	    cur->ignore |= ERROR_EQNAME;
	    continue;
	}

	/* if an = type, check for long = name if requested */
	if (cur->type[0] == '=' && s_flag > 0 &&
	    strlen(&(cur->type[1])) > (size_t)s_flag) {
	    if (!QUIET(hostid))
                warn("line %d <%s> from %s is equivalenced to a long name:"
                     " <%s>", cnt+1, cur->name, host,
		     (cur->type) ? cur->type : "NULL");
	    cur->ignore |= ERROR_EQNAME;
	    continue;
	}

	/* count this entry which will be used */
	++ucnt;
    }
    if (D_BUG)
        warn("STATUS: read %d groups, will merge %d groups from %s",
             cnt, ucnt, host);

    /* count the errors */
    *errs = cnt - ucnt;
    if (D_BUG)
        warn("STATUS: found %d line errors from %s", *errs, host);

    /* determine why we stopped */
    if (QIOerror(qp))
        sysdie("cannot read temp file for %s at line %d", host, cnt);
    else if (QIOtoolong(qp))
        sysdie("line %d from host %s is too long", cnt, host);

    /* all done */
    if (is_file) {
	QIOclose(qp);
    } else {
	CAclose();
	fprintf(ToServer, "quit\r\n");
	fclose(ToServer);
	fgets(buff, sizeof buff, FromServer);
	fclose(FromServer);
    }
    return ret;
}

/*
 * bad_grpname - test if the string is a valid group name
 *
 * Newsgroup names must consist of only alphanumeric chars and
 * characters from the following regular expression:
 *
 *	[.+-_]
 *
 * One cannot have two '.'s in a row.  The first character must be
 * alphanumeric.  The character following a '.' must be alphanumeric.
 * The name cannot end in a '.' character.
 *
 * If we are checking for all numeric components, (see num_chk) then
 * a component cannot be all numeric.  I.e,. there must be a non-numeric
 * character in the name, there must be a non-numeric character between
 * the start and the first '.', there must be a non-numeric character
 * between two '.'s and there must be a non-numeric character between
 * the last '.' and the end.
 *
 * given:
 *	name	newsgroup name to check
 *	num_chk	true => all numeric newsgroups components are invalid
 *		false => do not check for numeric newsgroups
 *
 * returns:
 *	0	group is ok
 *	1	group is bad
 */
static int
bad_grpname(char *name, int num_chk)
{
    char *p;
    int non_num;	/* true => found a non-numeric, non-. character */
    int level;		/* group levels (.'s) */

    /* firewall */
    if (name == NULL) {
	return 1;
    }

    /* must start with an alphanumeric ascii character */
    if (!isascii(name[0])) {
	return 1;
    }
    /* set non_num as needed */
    if (isalpha((int)name[0])) {
	non_num = true;
    } else if ((int)isdigit((int)name[0])) {
	non_num = false;
    } else {
	return 1;
    }

    /* scan each char */
    level = 0;
    for (p=name+1; *p; ++p) {

	/* name must contain ASCII chars */
	if (!isascii(*p)) {
	    return 1;
	}

	/* alpha chars are ok */
	if (isalpha((int)*p)) {
	    non_num = true;
	    continue;
	}

	/* numeric chars are ok */
	if (isdigit((int)*p)) {
	    continue;
	}

	/* +, - and _ are ok */
	if (*p == '+' || *p == '-' || *p == '_') {
	    non_num = true;
	    continue;
	}

	/* check for the '.' case */
	if (*p == '.') {
	    /*
	     * look for groups that are too deep, if requested by -g
	     */
	    if (g_flag > 0 && ++level > g_flag) {
		/* we are too deep */
		return 1;
	    }

	    /*
	     * A '.' is ok as long as the next character is alphanumeric.
	     * This implies that '.' cannot be before a previous '.' and
	     * that it cannot be at the end.
	     *
	     * If we are checking for all numeric components, then
	     * '.' is ok if we saw a non-numeric char before the
	     * last '.', or before the beginning if no previous '.'
	     * has been seen.
	     */
	    if ((!num_chk || non_num) && isascii(*(p+1)) && isalnum((int)*(p+1))) {
		++p;		/* '.' is ok, and so is the next char */
		if (isdigit((int)*p)) {	/* reset non_num as needed */
		    non_num = false;
		} else {
		    non_num = true;
		}
		continue;
	    }
	}

	/* this character must be invalid */
	return 1;
    }
    if (num_chk && !non_num) {
	/* last component is all numeric */
	return 1;
    }

    /* the name must be ok */
    return 0;
}

/*
 * get_ignore - get the ignore list from an ignore file
 *
 * given:
 *	filename	name of the ignore file to read
 *	*len		pointer to length of ignore return array
 *
 * returns:
 *	returns a malloced ignore pattern array, changes len
 *
 * An ignore file is of the form:
 *
 *	# this is a comment which is ignored
 *	# comments begin at the first # character
 *	# comments may follow text on the same line
 *
 *	# blank lines are ignored too
 *
 *	# lines are [ic] <spaces-tabs> pattern [<spaces-tabs> type] ...
 *	i    foo.*		# ignore foo.* groups,
 *	c    foo.bar m		# but check foo.bar if moderated
 *	c    foo.keep.*		# and check foo.keep.*
 *	i    foo.keep.* j =alt.*      # except when foo.keep.* is junked
 *	                              #     or equivalenced to an alt.* group
 *
 * The 'i' value means ignore, 'c' value means 'compare'.   The last pattern
 * that matches a group determines the fate of the group.  By default all
 * groups are included.
 *
 * NOTE: Only one '=name' is allowed per line.
 *       "=" is considered to be equivalent to "=*".
 */
static struct pat *
get_ignore(char *filename, int *len)
{
    QIOSTATE *qp;		/* QIO ignore file state */
    char *line;			/* the line just read */
    struct pat *ret;		/* array of ignore patterns to return */
    struct pat *cur;		/* current pattern entry being formed */
    int max;			/* max length (in elements) of ret */
    int linenum;		/* current line number */
    char *p;
    int i;

    /* firewall */
    if (filename == NULL)
        die("internal error #3: filename is NULL");
    if (len == NULL)
        die("internal error #4: len is NULL");
    if (D_BUG)
        warn("STATUS: reading ignore file %s", filename);

    /* setup return array */
    ret = xmalloc(CHUNK * sizeof(struct grp));
    max = CHUNK;

    /* setup to read the ignore file data quickly */
    if ((qp = QIOopen(filename)) == NULL)
        sysdie("cannot read ignore file %s", filename);

    /* scan server's output, displaying appropriate lines */
    *len = 0;
    for (linenum = 1; (line = QIOread(qp)) != NULL; ++linenum) {

	/* expand return array if needed */
	if (*len >= max) {
	    max += CHUNK;
            ret = xrealloc(ret, sizeof(struct pat) * max);
	}

	/* remove any trailing comments */
	p = strchr(line, '#');
	if (p != NULL) {
	    *p = '\0';
	}

	/* remove any trailing spaces and tabs */
	for (p = &line[strlen(line)-1];
	     p >= line && (*p == ' ' || *p == '\t');
	     --p) {
	    *p = '\0';
	}

	/* ignore line if the remainder of the line is empty */
	if (line[0] == '\0') {
	    continue;
	}

	/* ensure that the line starts with an i or c token */
	if ((line[0] != 'i' && line[0] != 'c') ||
	    (line[1] != ' ' && line[1] != '\t'))
            die("first token is not i or c in line %d of %s", linenum,
                filename);

	/* ensure that the second newsgroup pattern token follows */
	p = strtok(line+2, " \t");
	if (p == NULL)
            die("did not find 2nd field in line %d of %s", linenum,
                filename);

	/* setup the next return element */
	cur = &ret[*len];
	cur->pat = NULL;
	cur->type_match = 0;
	cur->y_type = 0;
	cur->m_type = 0;
	cur->n_type = 0;
	cur->j_type = 0;
	cur->x_type = 0;
	cur->eq_type = 0;
	cur->epat = NULL;
	cur->ignore = (line[0] == 'i');

	/* obtain a copy of the newsgroup pattern token */
        cur->pat = xstrdup(p);

	/* process any other type tokens */
	for (p=strtok(NULL, " \t"), i=3;
	     p != NULL;
	     p=strtok(NULL, " \t"), ++i) {

	    /* ensure that this next token is a valid type */
	    switch (p[0]) {
	    case 'y':
	    case 'm':
	    case 'j':
	    case 'n':
	    case 'x':
		if (p[1] != '\0') {
                    warn("field %d on line %d of %s not a valid type",
                         i, linenum, filename);
                    die("valid types are a char from [ymnjx=] or =name");
		}
		break;
	    case '=':
		break;
	    default:
                warn("field %d on line %d of %s is not a valid type",
                     i, linenum, filename);
                die("valid types are a char from [ymnjx=] or =name");
            }

	    /* note that we have a type specific pattern */
	    cur->type_match = 1;

	    /* ensure that type is not a duplicate */
	    if ((p[0] == 'y' && cur->y_type) ||
	        (p[0] == 'm' && cur->m_type) ||
	        (p[0] == 'n' && cur->n_type) ||
	        (p[0] == 'j' && cur->j_type) ||
	        (p[0] == 'x' && cur->x_type) ||
	        (p[0] == '=' && cur->eq_type)) {
                warn("only one %c type allowed per line", p[0]);
                die("field %d on line %d of %s is a duplicate type",
                    i, linenum, filename);
	    }

	    /* note what we have seen */
	    switch (p[0]) {
	    case 'y':
		cur->y_type = 1;
		break;
	    case 'm':
		cur->m_type = 1;
		break;
	    case 'j':
		cur->j_type = 1;
		break;
	    case 'n':
		cur->n_type = 1;
		break;
	    case 'x':
		cur->x_type = 1;
		break;
	    case '=':
		cur->eq_type = 1;
		if (p[0] == '=' && p[1] != '\0')
                    cur->epat = xstrdup(p + 1);
		break;
	    }

	    /* object if too many fields */
	    if (i-3 > (int) TYPECNT)
                die("too many fields on line %d of %s", linenum, filename);
	}

	/* count another pat element */
	++(*len);
    }

    /* return the pattern array */
    return ret;
}

/*
 * ignore - ignore newsgroups given an ignore list
 *
 * given:
 *	grp	array of groups
 *	grplen	length of grp array in elements
 *	igcl	array of ignore
 *	iglen	length of igcl array in elements
 */
static void
ignore(struct grp *grp, int grplen, struct pat *igcl, int iglen)
{
    struct grp *gp;		/* current group element being examined */
    struct pat *pp;		/* current pattern element being examined */
    int g;			/* current group index number */
    int p;			/* current pattern index number */
    int ign;			/* 1 => ignore this group, 0 => check it */
    int icnt;			/* groups ignored */
    int ccnt;			/* groups to be checked */

    /* firewall */
    if (grp == NULL)
        die("internal error #5: grp is NULL");
    if (igcl == NULL)
        die("internal error $6: igcl is NULL");
    if (D_BUG)
        warn("STATUS: determining which groups to ignore");

    /* if nothing to do, return quickly */
    if (grplen <= 0 || iglen <= 0) {
	return;
    }

    /* examine each group */
    icnt = 0;
    ccnt = 0;
    for (g=0; g < grplen; ++g) {

	/* check the group to examine */
	gp = &grp[g];
	if (gp->ignore) {
	    /* already ignored no need to examine */
	    continue;
	}

	/* check group against all patterns */
	ign = 0;
	for (p=0, pp=igcl; p < iglen; ++p, ++pp) {

	    /* if pattern has a specific type, check it first */
	    if (pp->type_match) {

		/* specific type required, check for match */
		switch (gp->type[0]) {
		case 'y':
		    if (! pp->y_type) continue;  /* pattern does not apply */
		    break;
		case 'm':
		    if (! pp->m_type) continue;  /* pattern does not apply */
		    break;
		case 'n':
		    if (! pp->n_type) continue;  /* pattern does not apply */
		    break;
		case 'j':
		    if (! pp->j_type) continue;  /* pattern does not apply */
		    break;
		case 'x':
		    if (! pp->x_type) continue;  /* pattern does not apply */
		    break;
		case '=':
		    if (! pp->eq_type) continue;  /* pattern does not apply */
		    if (pp->epat != NULL && !uwildmat(&gp->type[1], pp->epat)) {
			/* equiv pattern doesn't match, patt does not apply */
			continue;
		    }
		    break;
		}
	    }

	    /* perform a match on group name */
	    if (uwildmat(gp->name, pp->pat)) {
		/* this pattern fully matches, use the ignore value */
		ign = pp->ignore;
	    }
	}

	/* if this group is to be ignored, note it */
	if (ign) {
	    switch (gp->hostid) {
	    case HOSTID1:
		if (ign_host1_flag) {
		    gp->ignore |= CHECK_IGNORE;
		    ++icnt;
		}
		break;
	    case HOSTID2:
		if (ign_host2_flag) {
		    gp->ignore |= CHECK_IGNORE;
		    ++icnt;
		}
		break;
	    default:
                die("newsgroup %s bad hostid: %d", gp->name, gp->hostid);
	    }
	} else {
	    ++ccnt;
	}
    }
    if (D_BUG)
        warn("STATUS: examined %d groups: %d ignored, %d to be checked",
             grplen, icnt, ccnt);
}

/*
 * merge_cmp - qsort compare function for later group merge
 *
 * given:
 *	a	group a to compare
 *	b	group b to compare
 *
 * returns:
 *	>0	a > b
 *	0	a == b elements match (fatal error if a and b are different)
 *	<0	a < b
 *
 * To speed up group comparison, we compare by the following items listed
 * in order of sorting:
 *
 *	group name
 *	hostid			(host1 ahead of host2)
 *	linenum			(active file line number)
 */
static int
merge_cmp(const void *arg_a, const void *arg_b)
{
    const struct grp *a = arg_a;	/* group a to compare */
    const struct grp *b = arg_b;	/* group b to compare */
    int i;

    /* firewall */
    if (a == b) {
	/* we guess this could happen */
	return(0);
    }

    /* compare group names */
    i = strcmp(a->name, b->name);
    if (i != 0) {
	return i;
    }

    /* compare hostid's */
    if (a->hostid != b->hostid) {
	if (a->hostid > b->hostid) {
	    return 1;
	} else {
	    return -1;
	}
    }

    /* compare active line numbers */
    if (a->linenum != b->linenum) {
	if (a->linenum > b->linenum) {
	    return 1;
	} else {
	    return -1;
	}
    }

    /* two different elements match, this should not happen! */
    die("two internal grp elements match!");
    /*NOTREACHED*/
}

/*
 * merge_grps - compare groups from both hosts
 *
 * given:
 *	grp	array of groups
 *	grplen	length of grp array in elements
 *	host1	name of host with HOSTID1
 *	host2	name of host with HOSTID2
 *
 * This routine will select which groups to output from a merged active file.
 */
static void
merge_grps(struct grp *grp, int grplen, char *host1, char *host2)
{
    int cur;		/* current group index being examined */
    int nxt;		/* next group index being examined */
    int outcnt;		/* groups to output */
    int rmcnt;		/* groups to remove */
    int h1_probs;	/* =type problem groups from host1 */
    int h2_probs;	/* =type problem groups from host2 */

    /* firewall */
    if (grp == NULL)
        die("internal error #7: grp is NULL");

    /* sort groups for the merge */
    if (D_BUG)
        warn("STATUS: sorting groups");
    qsort((char *)grp, grplen, sizeof(grp[0]), merge_cmp);

    /* mark =type problem groups from host2, if needed */
    h2_probs = mark_eq_probs(grp, grplen, l_host2_flag, host1, host2);

    /*
     * We will walk thru the sorted group array, looking for pairs
     * among the groups that we have not already ignored.
     *
     * If a host has duplicate groups, then the duplicates will
     * be next to each other.
     *
     * If both hosts have the name group, they will be next to each other.
     */
    if (D_BUG)
        warn("STATUS: merging groups");
    outcnt = 0;
    rmcnt = 0;
    for (cur=0; cur < grplen; cur=nxt) {

	/* determine the next group index */
	nxt = cur+1;

	/* skip if this group is ignored */
	if (grp[cur].ignore) {
	    continue;
	}
	/* assert: cur is not ignored */

	/* check for duplicate groups from the same host */
	while (nxt < grplen) {

	    /* mark the later as a duplicate */
	    if (grp[cur].hostid == grp[nxt].hostid &&
	        strcmp(grp[cur].name, grp[nxt].name) == 0) {
		grp[nxt].ignore |= ERROR_DUP;
		if (!QUIET(grp[cur].hostid))
                    warn("lines %d and %d from %s refer to the same group",
                         grp[cur].linenum, grp[nxt].linenum,
                         ((grp[cur].hostid == HOSTID1) ? host1 : host2));
		++nxt;
	    } else {
		break;
	    }
	}
	/* assert: cur is not ignored */
	/* assert: cur & nxt are not the same group from the same host */

	/* if nxt is ignored, look for the next non-ignored group */
	while (nxt < grplen && grp[nxt].ignore) {
	    ++nxt;
	}
	/* assert: cur is not ignored */
	/* assert: nxt is not ignored or is beyond end */
	/* assert: cur & nxt are not the same group from the same host */

	/* case: cur and nxt are the same group */
	if (nxt < grplen && strcmp(grp[cur].name, grp[nxt].name) == 0) {

	    /* assert: cur is HOSTID1 */
	    if (grp[cur].hostid != HOSTID1)
                die("internal error #8: grp[%d].hostid: %d != %d",
		    cur, grp[cur].hostid, HOSTID1);

	    /*
	     * Both hosts have the same group.  Make host1 group type
	     * match host2.  (it may already)
	     */
	    grp[cur].output = 1;
	    grp[cur].outhi = (host2_hilow_all ? grp[nxt].hi : grp[cur].hi);
	    grp[cur].outlow = (host2_hilow_all ? grp[nxt].low : grp[cur].low);
	    grp[cur].outtype = grp[nxt].type;
	    ++outcnt;

	    /* do not process nxt, skip to the one beyond */
	    ++nxt;

	/* case: cur and nxt are different groups */
	} else {

	    /*
	     * if cur is host2, then host1 doesn't have it, so output it
	     */
	    if (grp[cur].hostid == HOSTID2) {
		grp[cur].output = 1;
		grp[cur].outhi = (host2_hilow_newgrp ? grp[cur].hi : DEF_HI);
		grp[cur].outlow = (host2_hilow_newgrp ? grp[cur].low : DEF_LOW);
		grp[cur].outtype = grp[cur].type;
		++outcnt;

	    /*
	     * If cur is host1, then host2 doesn't have it.
	     * Mark for removal if -m was not given.
	     */
	    } else {
		grp[cur].output = 1;
		grp[cur].outhi = grp[cur].hi;
		grp[cur].outlow = grp[cur].low;
		grp[cur].outtype = grp[cur].type;
		if (! m_flag) {
		    grp[cur].remove = 1;
		    ++rmcnt;
		}
	    }

	    /* if no more groups to examine, we are done */
	    if (nxt >= grplen) {
		break;
	    }
	}
    }

    /* mark =type problem groups from host1, if needed */
    h1_probs = mark_eq_probs(grp, grplen, l_host1_flag, host1, host2);

    /* all done */
    if (D_BUG) {
        warn("STATUS: sort-merge passed thru %d groups", outcnt);
        warn("STATUS: sort-merge marked %d groups for removal", rmcnt);
	warn("STATUS: marked %d =type error groups from host1", h1_probs);
        warn("STATUS: marked %d =type error groups from host2", h2_probs);
    }
    return;
}

/*
 * active_cmp - qsort compare function for active file style output
 *
 * given:
 *	a	group a to compare
 *	b	group b to compare
 *
 * returns:
 *	>0	a > b
 *	0	a == b elements match (fatal error if a and b are different)
 *	<0	a < b
 *
 * This sort will sort groups so that the lines that will be output
 * are host1 lines followed by host2 lines.  Thus, we will sort by
 * the following keys:
 *
 *	hostid			(host1 ahead of host2)
 *	linenum			(active file line number)
 */
static int
active_cmp(const void *arg_a, const void *arg_b)
{
    const struct grp *a = arg_a;	/* group a to compare */
    const struct grp *b = arg_b;	/* group b to compare */

    /* firewall */
    if (a == b) {
	/* we guess this could happen */
	return(0);
    }

    /* compare hostid's */
    if (a->hostid != b->hostid) {
	if (a->hostid > b->hostid) {
	    return 1;
	} else {
	    return -1;
	}
    }

    /* compare active line numbers */
    if (a->linenum != b->linenum) {
	if (a->linenum > b->linenum) {
	    return 1;
	} else {
	    return -1;
	}
    }

    /* two different elements match, this should not happen! */
    die("two internal grp elements match!");
    /*NOTREACHED*/
}

/*
 * output_grps - output the result of the merge
 *
 * given:
 *	grp	array of groups
 *	grplen	length of grp array in elements
 */
static void
output_grps(struct grp *grp, int grplen)
{
    int add;		/* number of groups added */
    int change;		/* number of groups changed */
    int remove;		/* number of groups removed */
    int no_new_dir;	/* number of new groups with missing/empty dirs */
    int new_dir;	/* number of new groups, non-empty dir no water chg */
    int water_change;	/* number of new groups where hi&low water changed */
    int work;		/* adds + changes + removals */
    int same;		/* the number of groups the same */
    int ignore;		/* host1 newsgroups to ignore */
    int not_done;	/* exec errors and execs not performed */
    int rm_cycle;	/* 1 => removals only, 0 => adds & changes only */
    int sleep_msg;	/* 1 => -o x sleep message was given */
    int top_ignore;	/* number of groups ignored because of no top level */
    int restore;	/* host1 groups restored due to -o a1 */
    double host1_same;	/* % of host1 that is the same */
    struct hash *existing_hier;	/* hash of existing hierarchies for -T */
    char *p, *q;
    int i;

    /* firewall */
    if (grp == NULL)
        die("internal error #9: grp is NULL");

    /*
     * If -a1 was given, mark for output any host1 newsgroup that was
     * simply ignored due to the -i ign_file.
     */
    if (host1_ign_print) {
	restore = 0;
	for (i=0; i < grplen; ++i) {
	    if (grp[i].hostid == HOSTID1 && 
		(grp[i].ignore == CHECK_IGNORE ||
		 grp[i].ignore == CHECK_TYPE ||
		 grp[i].ignore == (CHECK_IGNORE|CHECK_TYPE))) {
		/* force group to output and not be ignored */
		grp[i].ignore = 0;
		grp[i].output = 1;
		grp[i].remove = 0;
		grp[i].outhi = grp[i].hi;
		grp[i].outlow = grp[i].low;
		grp[i].outtype = grp[i].type;
		++restore;
	    }
	}
	if (D_BUG)
            warn("STATUS: restored %d host1 groups", restore);
    }

    /*
     * If -T, ignore new top level groups from host2
     */
    if (no_new_hier) {
	existing_hier = hash_create(32, hash_string, string_key,
		string_equal, free);
	for (i=0; i < grplen; ++i) {
	    if (grp[i].hostid == HOSTID2)
		continue;
	    p = xstrdup(grp[i].name);
	    q = strchr(p, '.');
	    if (q != NULL)
		*q = '\0';
	    if (!hash_insert(existing_hier, p, p))
		free(p);
	}
	top_ignore = 0;
	for (i=0; i < grplen; ++i) {
	    /* look at new newsgroups */
	    if (grp[i].hostid == HOSTID2 &&
		grp[i].output != 0 &&
		new_top_hier(grp[i].name, existing_hier)) {
		 /* no top level ignore this new group */
		 grp[i].ignore |= CHECK_HIER;
		 grp[i].output = 0;
		 if (D_BUG)
                     warn("ignore new newsgroup: %s, new hierarchy",
                          grp[i].name);
		 ++top_ignore;
	    }
	}
	hash_free(existing_hier);
	if (D_SUMMARY)
            warn("STATUS: ignored %d new newsgroups due to new hierarchy",
                 top_ignore);
    }

    /* sort by active file order if active style output (-a) */
    if (o_flag == OUTPUT_ACTIVE) {
	if (D_BUG)
            warn("STATUS: sorting groups in output order");
	qsort((char *)grp, grplen, sizeof(grp[0]), active_cmp);
    }

    /*
     * Determine the % of lines from host1 active file that remain unchanged
     * ignoring any low/high water mark changes.
     *
     * Determine the number of old groups that will remain the same
     * the number of new groups that will be added.
     */
    add = 0;
    change = 0;
    remove = 0;
    same = 0;
    ignore = 0;
    no_new_dir = 0;
    new_dir = 0;
    water_change = 0;
    for (i=0; i < grplen; ++i) {
	/* skip non-output ...  */
	if (grp[i].output == 0) {
	    if (grp[i].hostid == HOSTID1) {
		++ignore;
	    }
	    continue;

	/* case: group needs removal */
	} else if (grp[i].remove) {
	    ++remove;

	/* case: group is from host2, so we need a newgroup */
	} else if (grp[i].hostid == HOSTID2) {
	    ++add;

	/* case: group is from host1, but the type changed */
	} else if (grp[i].type != grp[i].outtype &&
		   strcmp(grp[i].type,grp[i].outtype) != 0) {
	    ++change;

	/* case: group did not change */
	} else {
	    ++same;
	}
    }
    work = add+change+remove;
    if (same+work+host1_errs <= 0) {
	/* no lines, no work, no errors == nothing changed == 100% the same */
	host1_same = (double)100.0;
    } else {
	/* calculate % unchanged */
	host1_same = (double)100.0 *
		     ((double)same / (double)(same+work+host1_errs));
    }
    if (D_BUG) {
        warn("STATUS: same=%d add=%d, change=%d, remove=%d",
             same, add, change, remove);
        warn("STATUS: ignore=%d, work=%d, err=%d",
             ignore, work, host1_errs);
        warn("STATUS: same+work+err=%d, host1_same=%.2f%%",
             same+work+host1_errs, host1_same);
    }

    /* 
     * Bail out if we too few lines in host1 active file (ignoring
     * low/high water mark changes) remaining unchanged.
     *
     * We define change as:
     *
     *	line errors from host1 active file
     *	newsgroups to be added to host1
     *	newsgroups to be removed from host1
     *	newsgroups to be change in host1
     */
    if (host1_same < p_flag) {
        warn("HALT: lines unchanged: %.2f%% < min change limit: %.2f%%",
             host1_same, p_flag);
        warn("    No output or commands executed.  Determine if the degree");
        warn("    of changes is okay and re-execute with a lower -p value");
        die("    or with the problem fixed.");
    }

    /*
     * look at all groups
     *
     * If we are not producing active file output, we must do removals
     * before we do any adds and changes.
     *
     * We recalculate the work stats in finer detail as well as noting how
     * many actions were successful.
     */
    add = 0;
    change = 0;
    remove = 0;
    same = 0;
    ignore = 0;
    work = 0;
    not_done = 0;
    sleep_msg = 0;
    rm_cycle = ((o_flag == OUTPUT_ACTIVE) ? 0 : 1);
    do {
	for (i=0; i < grplen; ++i) {

	    /* if -o Ax, output ignored non-error groups too */

	    /*
	     * skip non-output ...
	     *
	     * but if '-a' and active output mode, then don't skip ignored,
	     * non-error, non-removed groups from host1
	     */
	    if (grp[i].output == 0) {
                /* Ignored groups should only be counted once. */
		if (!rm_cycle && grp[i].hostid == HOSTID1) {
		    ++ignore;
		}
		continue;
	    }

	    /* case: output active lines */
	    if (o_flag == OUTPUT_ACTIVE) {

		/* case: group needs removal */
		if (grp[i].remove) {
		    ++remove;
		    ++work;

		/* case: group will be kept */
		} else {

		    /* output in active file format */
		    printf("%s %s %s %s\n",
			grp[i].name,  grp[i].outhi, grp[i].outlow,
			grp[i].outtype);

		    /* if -v level is high enough, do group accounting */
		    if (D_IF_SUMM) {

			/* case: group is from host2, so we need a newgroup */
			if (grp[i].hostid == HOSTID2) {
			    ++add;
			    ++work;

			/* case: group is from host1, but the type changed */
			} else if (grp[i].type != grp[i].outtype &&
				   strcmp(grp[i].type,grp[i].outtype) != 0) {
			    ++change;
			    ++work;

			/* case: group did not change */
			} else {
			    ++same;
			}
		    }
		}

	    /* case: output ctlinnd commands */
	    } else if (o_flag == OUTPUT_CTLINND) {

		/* case: group needs removal */
		if (grp[i].remove) {

		    /* output rmgroup */
		    if (rm_cycle) {
			printf("%s rmgroup %s\n", CTLINND_NAME, grp[i].name);
			++remove;
			++work;
		    }

		/* case: group is from host2, so we need a newgroup */
		} else if (grp[i].hostid == HOSTID2) {

		    /* output newgroup */
		    if (! rm_cycle) {
			printf("%s newgroup %s %s %s\n",
			    CTLINND_NAME, grp[i].name, grp[i].outtype, new_name);
			++add;
			++work;
		    }

		/* case: group is from host1, but the type changed */
		} else if (grp[i].type != grp[i].outtype &&
			   strcmp(grp[i].type,grp[i].outtype) != 0) {

		    /* output changegroup */
		    if (! rm_cycle) {
			printf("%s changegroup %s %s\n",
			    CTLINND_NAME, grp[i].name, grp[i].outtype);
			++change;
			++work;
		    }

		/* case: group did not change */
		} else {
		    if (! rm_cycle) {
			++same;
		    }
		}

	    /* case: exec ctlinnd commands */
	    } else if (o_flag == OUTPUT_EXEC || o_flag == OUTPUT_IEXEC) {

		/* warn about sleeping if needed and first time */
		if (o_flag == OUTPUT_EXEC && z_flag > 0 && sleep_msg == 0) {
		    if (D_SUMMARY)
                        warn("will sleep %d seconds before each fork/exec",
                             z_flag);
		    sleep_msg = 1;
		}

		/* case: group needs removal */
		if (grp[i].remove) {

		    /* exec rmgroup */
		    if (rm_cycle) {
			if (D_REPORT && o_flag == OUTPUT_EXEC)
                            warn("rmgroup %s", grp[i].name);
			if (! exec_cmd(o_flag, "rmgroup",
			    grp[i].name, NULL, NULL)) {
			    ++not_done;
			} else {
			    ++remove;
			    ++work;
			}
		    }

		/* case: group is from host2, so we need a newgroup */
		} else if (grp[i].hostid == HOSTID2) {

		    /* exec newgroup */
		    if (!rm_cycle) {
			if (D_REPORT && o_flag == OUTPUT_EXEC)
                            warn("newgroup %s %s %s",
                                 grp[i].name, grp[i].outtype, new_name);
			if (! exec_cmd(o_flag, "newgroup", grp[i].name,
				 grp[i].outtype, new_name)) {
			    ++not_done;
			} else {
			    ++add;
			    ++work;
			}
		    }

		/* case: group is from host1, but the type changed */
		} else if (grp[i].type != grp[i].outtype &&
			   strcmp(grp[i].type,grp[i].outtype) != 0) {

		    /* exec changegroup */
		    if (!rm_cycle) {
			if (D_REPORT && o_flag == OUTPUT_EXEC)
                            warn("changegroup %s %s",
                                 grp[i].name, grp[i].outtype);
			if (! exec_cmd(o_flag, "changegroup", grp[i].name,
				 grp[i].outtype, NULL)) {
			    ++not_done;
			} else {
			    ++change;
			    ++work;
			}
		    }

		/* case: group did not change */
		} else {
		    if (! rm_cycle) {
			++same;
		    }
		}
	    }
	}
    } while (--rm_cycle >= 0);

    /* final accounting, if -v */
    if (D_SUMMARY || (D_IF_SUMM && (work > 0 || not_done > 0))) {
        warn("STATUS: %d group(s)", add+remove+change+same);
        warn("STATUS: %d group(s)%s added", add,
             ((o_flag == OUTPUT_EXEC || o_flag == OUTPUT_IEXEC) ?
              "" : " to be"));
        warn("STATUS: %d group(s)%s removed",	remove,
             ((o_flag == OUTPUT_EXEC || o_flag == OUTPUT_IEXEC) ?
              "" : " to be"));
        warn("STATUS: %d group(s)%s changed", change,
             ((o_flag == OUTPUT_EXEC || o_flag == OUTPUT_IEXEC) ?
              "" : " to be"));
        warn("STATUS: %d group(s) %s the same", same,
             ((o_flag == OUTPUT_EXEC || o_flag == OUTPUT_IEXEC) ?
              "remain" : "are"));
        warn("STATUS: %.2f%% of lines unchanged", host1_same);
        warn("STATUS: %d group(s) ignored", ignore);
	if (o_flag == OUTPUT_EXEC || o_flag == OUTPUT_IEXEC)
            warn("STATUS: %d exec(s) not performed", not_done);
    }
}

/*
 * error_mark - mark for removal, error groups from a given host
 *
 * given:
 *	grp	array of groups
 *	grplen	length of grp array in elements
 *	hostid	host to mark error groups for removal
 */
static void
error_mark(struct grp *grp, int grplen, int hostid)
{
    int i;
    int errcnt;

    /* firewall */
    if (grp == NULL)
        die("internal error #11: grp is NULL");

    /* loop thru groups, looking for error groups from a given host */
    errcnt = 0;
    for (i=0; i < grplen; ++i) {

	/* skip if not from hostid */
	if (grp[i].hostid != hostid) {
	    continue;
	}

	/* mark for removal if an error group not already removed */
	if (IS_ERROR(grp[i].ignore)) {

	    /* mark for removal */
	    if (grp[i].output != 1 || grp[i].remove != 1) {
		grp[i].output = 1;
		grp[i].remove = 1;
	    }
	    ++errcnt;
	}
    }

    /* all done */
    if (D_SUMMARY || (D_IF_SUMM && errcnt > 0))
        warn("STATUS: marked %d error groups for removal", errcnt);
    return;
}

/*
 * eq_merge_cmp - qsort compare function for =type group processing
 *
 * given:
 *	a	=group a to compare
 *	b	=group b to compare
 *
 * returns:
 *	>0	a > b
 *	0	a == b elements match (fatal error if a and b are different)
 *	<0	a < b
 *
 * To speed up group comparison, we compare by the following items listed
 * in order of sorting:
 *
 *	skip			(non-skipped groups after skipped ones)
 *	group equiv name
 *	group name
 *	hostid			(host1 ahead of host2)
 *	linenum			(active file line number)
 */
static int
eq_merge_cmp(const void *arg_a, const void *arg_b)
{
    const struct eqgrp *a = arg_a;	/* group a to compare */
    const struct eqgrp *b = arg_b;	/* group b to compare */
    int i;

    /* firewall */
    if (a == b) {
	/* we guess this could happen */
	return(0);
    }

    /* compare skip values */
    if (a->skip != b->skip) {
	if (a->skip > b->skip) {
	    /* a is skipped, b is not */
	    return 1;
	} else {
	    /* b is skipped, a is not */
	    return -1;
	}
    }

    /* compare the names the groups are equivalenced to */
    i = strcmp(a->eq, b->eq);
    if (i != 0) {
	return i;
    }

    /* compare the group names themselves */
    i = strcmp(a->g->name, b->g->name);
    if (i != 0) {
	return i;
    }

    /* compare hostid's */
    if (a->g->hostid != b->g->hostid) {
	if (a->g->hostid > b->g->hostid) {
	    return 1;
	} else {
	    return -1;
	}
    }

    /* compare active line numbers */
    if (a->g->linenum != b->g->linenum) {
	if (a->g->linenum > b->g->linenum) {
	    return 1;
	} else {
	    return -1;
	}
    }

    /* two different elements match, this should not happen! */
    die("two internal eqgrp elements match!");
}

/*
 * mark_eq_probs - mark =type groups from a given host that have problems
 *
 * given:
 *	grp	 sorted array of groups
 *	grplen	 length of grp array in elements
 *	hostid	 host to mark error groups for removal, or NOHOST
 *	host1	name of host with HOSTID1
 *	host2	name of host with HOSTID2
 *
 * This function assumes that the grp array has been sorted by name.
 */
static int
mark_eq_probs(struct grp *grp, int grplen, int hostid, char *host1,
              char *host2)
{
    struct eqgrp *eqgrp;	/* =type pointer array */
    int eq_cnt;			/* number of =type groups from host */
    int new_eq_cnt;		/* number of =type groups remaining */
    int missing;		/* =type groups equiv to missing groups */
    int cycled;			/* =type groups equiv to themselves */
    int chained;		/* =type groups in long chain or loop */
    int cmp;			/* strcmp of two names */
    int step;			/* equiv loop step */
    int i;
    int j;

    /* firewall */
    if (grp == NULL)
        die("internal error #12: grp is NULL");
    if (hostid == NOHOST) {
	/* nothing to detect, nothing else to do */
	return 0;
    }

    /* count the =type groups from hostid that are not in error */
    eq_cnt = 0;
    for (i=0; i < grplen; ++i) {
	if (grp[i].hostid == hostid &&
	    ! IS_ERROR(grp[i].ignore) &&
	    grp[i].type != NULL &&
	    grp[i].type[0] == '=') {
	    ++eq_cnt;
	}
    }
    if (D_BUG && hostid != NOHOST)
        warn("STATUS: host%d has %d =type groups", hostid, eq_cnt);

    /* if no groups, then there is nothing to do */
    if (eq_cnt == 0) {
	return 0;
    }

    /* setup the =group record array */
    eqgrp = xmalloc(eq_cnt * sizeof(eqgrp[0]));
    for (i=0, j=0; i < grplen && j < eq_cnt; ++i) {
	if (grp[i].hostid == hostid &&
	    ! IS_ERROR(grp[i].ignore) &&
	    grp[i].type != NULL &&
	    grp[i].type[0] == '=') {

	    /* initialize record */
	    eqgrp[j].skip = 0;
	    eqgrp[j].g = &grp[i];
	    eqgrp[j].eq = &(grp[i].type[1]);
	    ++j;
	}
    }

    /*
     * try to resolve =type groups in at least EQ_LOOP equiv links
     */
    new_eq_cnt = eq_cnt;
    missing = 0;
    cycled = 0;
    for (step=0; step < EQ_LOOP && new_eq_cnt >= 0; ++step) {

	/* sort the =group record array */
	qsort((char *)eqgrp, eq_cnt, sizeof(eqgrp[0]), eq_merge_cmp);

	/* look for the groups to which =type group point at */
	eq_cnt = new_eq_cnt;
	for (i=0, j=0; i < grplen && j < eq_cnt; ++i) {

	    /* we will skip any group in error or from the wrong host */
	    if (grp[i].hostid != hostid || IS_ERROR(grp[i].ignore)) {
		continue;
	    }

	    /* we will skip any skipped eqgrp's */
	    if (eqgrp[j].skip) {
		/* try the same group against the next eqgrp */
		--i;
		++j;
		continue;
	    }

	    /* compare the =name of the eqgrp with the name of the grp */
	    cmp = strcmp(grp[i].name, eqgrp[j].eq);

	    /* case: this group is pointed at by an eqgrp */
	    if (cmp == 0) {

		 /* see if we have looped around to the original group name */
		 if (strcmp(grp[i].name, eqgrp[j].g->name) == 0) {

		    /* note the detected loop */
		    if (! QUIET(hostid))
                        warn("%s from %s line %d =loops around to itself",
                             eqgrp[j].g->name,
                             ((eqgrp[j].g->hostid == HOSTID1) ? host1 : host2),
                             eqgrp[j].g->linenum);
		     eqgrp[j].g->ignore |= ERROR_EQLOOP;

		    /* the =group is bad, so we don't need to bother with it */
		    eqgrp[j].skip = 1;
		    --new_eq_cnt;
		    ++cycled;
		    --i;
		    ++j;
		    continue;
		}

		/* if =group refers to a valid group, we are done with it */
		if (grp[i].type != NULL && grp[i].type[0] != '=') {
		    eqgrp[j].skip = 1;
		    --new_eq_cnt;
		/* otherwise note the equiv name */
		} else {
		    eqgrp[j].eq = &(grp[i].type[1]);
		}
		--i;
		++j;

	    /* case: we missed the =name */
	    } else if (cmp > 0) {

		/* mark the eqgrp in error */
		eqgrp[j].g->ignore |= ERROR_NONEQ;
		if (! QUIET(hostid))
                    warn("%s from %s line %d not equiv to a valid group",
                         eqgrp[j].g->name,
                         ((eqgrp[j].g->hostid == HOSTID1) ? host1 : host2),
                         eqgrp[j].g->linenum);

		/* =group is bad, so we don't need to bother with it anymore */
		eqgrp[j].skip = 1;
		--new_eq_cnt;
		++missing;
		++j;
	    }
	}

	/* any remaining non-skipped eqgrps are bad */
	while (j < eq_cnt) {

	    /* mark the eqgrp in error */
	    eqgrp[j].g->ignore |= ERROR_NONEQ;
	    if (! QUIET(hostid))
                warn("%s from %s line %d isn't equiv to a valid group",
                     eqgrp[j].g->name,
                     ((hostid == HOSTID1) ? host1 : host2),
                     eqgrp[j].g->linenum);

	    /* the =group is bad, so we don't need to bother with it anymore */
	    eqgrp[j].skip = 1;
	    --new_eq_cnt;
	    ++missing;
	    ++j;
	}
    }

    /* note groups that are in a long chain or loop */
    chained = new_eq_cnt;
    qsort((char *)eqgrp, eq_cnt, sizeof(eqgrp[0]), eq_merge_cmp);
    for (j=0; j < new_eq_cnt; ++j) {

	/* skip if already skipped */
	if (eqgrp[j].skip == 1) {
	    continue;
	}

	/* mark as a long loop group */
	eqgrp[j].g->ignore |= ERROR_LONGLOOP;
	if (! QUIET(hostid))
            warn("%s from %s line %d in a long equiv chain or loop > %d",
                 eqgrp[j].g->name,
                 ((hostid == HOSTID1) ? host1 : host2),
                 eqgrp[j].g->linenum, EQ_LOOP);
    }

    /* all done */
    if (D_BUG) {
        warn("%d =type groups from %s are not equiv to a valid group",
             missing, ((hostid == HOSTID1) ? host1 : host2));
        warn("%d =type groups from %s are equiv to themselves",
             cycled, ((hostid == HOSTID1) ? host1 : host2));
        warn("%d =type groups from %s are in a long chain or loop > %d",
             chained, ((hostid == HOSTID1) ? host1 : host2), EQ_LOOP);
    }
    free(eqgrp);
    return missing+cycled+chained;
}

/*
 * exec_cmd - exec a ctlinnd command in forked process
 *
 * given:
 *	mode	OUTPUT_EXEC or OUTPUT_IEXEC (interactive mode)
 *	cmd	"changegroup", "newgroup", "rmgroup"
 *	grp	name of group
 *	type	type of group or NULL
 *	who	newgroup creator or NULL
 *
 * returns:
 *	1	exec was performed
 *	0	exec was not performed
 */
static int
exec_cmd(int mode, const char *cmd, char *grp, char *type, const char *who)
{
    FILE *ch_stream = NULL;	/* stream from a child process */
    char buf[BUFSIZ+1];		/* interactive buffer */
    int pid;			/* pid of child process */
    int io[2];			/* pair of pipe descriptors */
    int status;			/* wait status */
    int exitval;		/* exit status of the child */
    char *w_string = NULL;     /* will contain "-t "+w_flag */
    char *p;

    /* firewall */
    if (cmd == NULL || grp == NULL)
        die("internal error #13, cmd or grp is NULL");

    /* if interactive, ask the question */
    if (mode == OUTPUT_IEXEC) {

	/* ask the question */
	fflush(stdin);
	fflush(stdout);
	fflush(stderr);
	if (type == NULL) {
	    printf("%s %s  [yn]? ", cmd, grp);
	} else if (who == NULL) {
	    printf("%s %s %s  [yn]? ", cmd, grp, type);
	} else {
	    printf("%s %s %s %s  [yn]? ", cmd, grp, type, who);
	}
	fflush(stdout);
	buf[0] = '\0';
	buf[BUFSIZ] = '\0';
	p = fgets(buf, BUFSIZ, stdin);
	if (p == NULL) {
	    /* EOF/ERROR on interactive input, silently stop processing */
	    exit(43);
	}

	/* if non-empty line doesn't start with 'y' or 'Y', skip command */
	if (buf[0] != 'y' && buf[0] != 'Y' && buf[0] != '\n') {
	    /* indicate nothing was done */
	    return 0;
	}
    }

    /* build a pipe for output from child interactive mode */
    if (mode == OUTPUT_IEXEC) {
	if (pipe(io) < 0)
            sysdie("pipe create failed");

    /* setup a fake pipe to /dev/null for non-interactive mode */
    } else {
	io[READ_SIDE] = open(DEV_NULL, 0);
	if (io[READ_SIDE] < 0)
            sysdie("unable to open %s for reading", DEV_NULL);
	io[WRITE_SIDE] = open(DEV_NULL, 1);
	if (io[WRITE_SIDE] < 0)
            sysdie("unable to open %s for writing", DEV_NULL);
    }

    /* pause if in non-interactive mode so as to not busy-out the server */
    if (mode == OUTPUT_EXEC && z_flag > 0) {
	if (D_BUG)
            warn("sleeping %d seconds before fork/exec", z_flag);
	    /* be sure they know what we are stalling */
	    fflush(stderr);
	sleep(z_flag);
    }

    /* fork the child process */
    fflush(stdout);
    fflush(stderr);
    pid = fork();
    if (pid == -1)
        sysdie("fork failed");

    /* case: child process */
    if (pid == 0) {

	/*
	 * prep file descriptors
	 */
	fclose(stdin);
	close(io[READ_SIDE]);
	if (dup2(io[WRITE_SIDE], 1) < 0)
            sysdie("child: dup of write I/O pipe to stdout failed");
	if (dup2(io[WRITE_SIDE], 2) < 0)
            sysdie("child: dup of write I/O pipe to stderr failed");

	/* exec the ctlinnd command */
	p = concatpath(innconf->pathbin, INN_PATH_CTLINND);
	asprintf(&w_string, "-t %d", w_flag);

	if (type == NULL) {
	    execl(p,
		  CTLINND_NAME, w_string, cmd, grp, (char *) 0);
	} else if (who == NULL) {
	    execl(p,
		  CTLINND_NAME, w_string, cmd, grp, type, (char *) 0);
	} else {
	    execl(p,
		  CTLINND_NAME, w_string, cmd, grp, type, who, (char *) 0);
	}
	/* child exec failed */
        sysdie("child process exec failed");

    /* case: parent process */
    } else {

	/* prep file descriptors */
	if (mode != OUTPUT_IEXEC) {
	    close(io[READ_SIDE]);
	}
	close(io[WRITE_SIDE]);

	/* print a line from the child, if interactive */
	if (mode == OUTPUT_IEXEC) {

	    /* read what the child says */
	    buf[0] = '\0';
	    buf[BUFSIZ] = '\0';
	    ch_stream = fdopen(io[READ_SIDE], "r");
	    if (ch_stream == NULL)
                sysdie("fdopen of pipe failed");
	    p = fgets(buf, BUFSIZ, ch_stream);

	    /* print what the child said, if anything */
	    if (p != NULL) {
		if (buf[strlen(buf)-1] == '\n')
                    buf[strlen(buf)-1] = '\0';
                warn("    %s", buf);
	    }
	}

	/* look for abnormal child termination/status */
	errno = 0;
	while (wait(&status) < 0) {
	    if (errno == EINTR) {
		/* just an interrupt, try to wait again */
		errno = 0;
	    } else {
                sysdie("wait returned -1");
	    }
	}
	if (mode == OUTPUT_IEXEC) {
	    /* close the pipe now that we are done with reading it */
	    fclose(ch_stream);
	}
	if (WIFSTOPPED(status)) {
            warn("    %s %s %s%s%s%s%s stopped",
                 CTLINND_NAME, cmd, grp,
                 (type ? "" : " "), (type ? type : ""),
                 (who ? "" : " "), (who ? who : ""));
	    /* assume no work was done */
	    return 0;
	}
	if (WIFSIGNALED(status)) {
            warn("    %s %s %s%s%s%s%s killed by signal %d",
                 CTLINND_NAME, cmd, grp,
                 (type ? "" : " "), (type ? type : ""),
                 (who ? "" : " "), (who ? who : ""), WTERMSIG(status));
	    /* assume no work was done */
	    return 0;
	}
	if (!WIFEXITED(status)) {
            warn("    %s %s %s%s%s%s%s returned unknown wait status: 0x%x",
                 CTLINND_NAME, cmd, grp,
                 (type ? "" : " "), (type ? type : ""),
                 (who ? "" : " "), (who ? who : ""), status);
	    /* assume no work was done */
	    return 0;
	}
	exitval = WEXITSTATUS(status);
	if (exitval != 0) {
            warn("    %s %s %s%s%s%s%s exited with status: %d",
                 CTLINND_NAME, cmd, grp,
                 (type ? "" : " "), (type ? type : ""),
                 (who ? "" : " "), (who ? who : ""), exitval);
	    /* assume no work was done */
	    return 0;
	}
    }

    /* all done */
    return 1;
}

/*
 * new_top_hier - determine if the newsgroup represents a new hierarchy
 *
 * Determine of the newsgroup name is a new hierarchy.
 *
 * given:
 *	name	name of newsgroup to check
 *	existing_hier	hash table of existing hierarchies
 *
 * returns:
 *	false	hierarchy already exists
 *	true	hierarchy does not exist, name represents a new hierarchy
 */
static int
new_top_hier(char *name, struct hash *existing_hier)
{
    int result;			/* return result */
    char *dot;

    /*
     * temp change name to just the top level
     */
    dot = strchr(name, '.');
    if (dot != NULL) {
	*dot = '\0';
    }

    /*
     * determine if we can find this top level hierarchy directory
     */
    result = (hash_lookup(existing_hier, name) == NULL);

    /* restore name */
    if (dot != NULL) {
	*dot = '.';
    }

    /*
     * return the result
     */
    return result;
}

/*
 * string_key - identity function, for use with hashtab library
 *
 * Returns its only argument.
 *
 * given:
 *	entry	void* pointer representing a string
 *
 * returns:
 *	the same void* pointer
 */
static const void *
string_key(const void *entry)
{
    return entry;
}

/*
 * string_equal - string comparison function, for use with hashtab library
 *
 * Compares two strings.
 *
 * given:
 *	key	void* pointer representing a hash table key
 *	entry	void* pointer representing a hash table entry
 *
 * returns:
 *	0	arguments are not equal
 *	1	arguments are equal
 */
static bool
string_equal(const void *key, const void *entry)
{
    const char *p, *q;

    p = key;
    q = entry;
    return !strcmp(p, q);
}
