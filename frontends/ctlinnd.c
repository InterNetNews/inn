/*  $Id$
**
**  Send control messages to the InterNetNews daemon.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inndcomm.h"
#include "libinn.h"
#include "paths.h"


/*
**  Datatype for an entry in the command table.
*/
typedef struct _COMMAND {
    const char *Command;
    const char *Text;
    int		argc;
    char	Letter;
    bool	Glue;
} COMMAND;


static COMMAND	Commands[] = {
    {	"addhist",	"id arr exp post token...\tAdd history line",
	5,	SC_ADDHIST,	true	},
    {	"allow",	"reason...\t\t\tAllow remote connections",
	1,	SC_ALLOW,	true	},
    {	"begin",	"site\t\t\tStart newly-added site",
	1,	SC_BEGIN,	false	},
    {	"cancel",	"id\t\t\tCancel message locally",
	1,	SC_CANCEL,	false	},
    {	"changegroup",	"group rest\tChange mode of group",
	2,	SC_CHANGEGROUP,	false	},
    {	"checkfile",	"\t\t\tCheck syntax of newsfeeds file",
	0,	SC_CHECKFILE,	false	},
    {	"drop",		"site\t\t\tStop feeding site",
	1,	SC_DROP,	false		},
    {	"feedinfo",		"site\t\t\tPrint state of feed to site*",
	1,	SC_FEEDINFO,	false		},
#if defined(DO_TCL)
    {	"tcl",			"flag\t\t\tEnable or disable Tcl filtering",
	1,	SC_FILTER,	false		},
#endif /* defined(DO_TCL) */
    {	"flush",	"site\t\t\tFlush feed for site*",
	1,	SC_FLUSH,	false	},
    {	"flushlogs",	"\t\t\tFlush log files",
	0,	SC_FLUSHLOGS,	false	},
    {	"go",		"reason...\t\t\tRestart after pause or throttle",
	1,	SC_GO,		true	},
    {	"hangup",	"channel\t\tHangup specified incoming channel",
	1,	SC_HANGUP,	false	},
    {	"logmode",		"\t\t\t\tSend server mode to syslog",
	0,	SC_LOGMODE,	false		},
    {	"mode",		"\t\t\t\tPrint operating mode",
	0,	SC_MODE,	false		},
    {	"name",		"nnn\t\t\tPrint name of specified channel*",
	1,	SC_NAME,	false		},
    {	"newgroup",	"group rest creator\tCreate new group",
	3,	SC_NEWGROUP,	false	},
    {	"param",	"letter value\t\tChange command-line parameters",
	2,	SC_PARAM,	false	},
    {	"pause",	"reason...\t\tShort-term pause in accepting articles",
	1,	SC_PAUSE,	true	},
#if defined(DO_PERL)
    {	"perl",			"flag\t\t\tEnable or disable Perl filtering",
	1,	SC_PERL,	false	},
#endif /* defined(DO_PERL) */
#if defined(DO_PYTHON)
    {	"python",		"flag\t\t\tEnable or disable Python filtering",
	1,	SC_PYTHON,	false	},
#endif /* (DO_PYTHON) */
    {	"readers",	"flag text...\t\tEnable or disable newsreading",
	2,	SC_READERS,	true	},
    {	"reject",	"reason...\t\t\tReject remote connections",
	1,	SC_REJECT,	true	},
    {	"reload",	"what reason...\t\tRe-read config files*",
	2,	SC_RELOAD,	true	},
    {	"renumber",	"group\t\tRenumber the active file*",
	1,	SC_RENUMBER,	false	},
    {	"reserve",	"reason...\t\tReserve the next pause or throttle",
	1,	SC_RESERVE,	true	},
    {	"rmgroup",	"group\t\t\tRemove named group",
	1,	SC_RMGROUP,	false	},
    {	"send",		"feed text...\t\tSend text to exploder feed",
	2,	SC_SEND,	true	},
    {	"shutdown",	"reason...\t\tShut down server",
	1,	SC_SHUTDOWN,	true	},
    {	"stathist",	"filename|off\t\tLog into filename some history stats",
	1,	SC_STATHIST,	false	},
    {	"status",	"interval|off\t\tTurn innd status generation on or off",
	1,	SC_STATUS,	false	},
    {	"kill",	"signal site\t\tSend signal to site's process",
	2,	SC_SIGNAL,	false	},
    {	"throttle",	"reason...\t\tStop accepting articles",
	1,	SC_THROTTLE,	true	},
    {   "timer",	"interval|off\t\tTurn performance monitoring on or off",
	1,	SC_TIMER,	false	},
    {	"trace",	"innd|#|nnrpd flag\tTurn tracing on or off",
	2,	SC_TRACE,	false	},
    {	"xabort",	"text...\t\tAbort the server",
	1,	SC_XABORT,	true	},
    { "lowmark",	"filename\t\tReset active file low article marks",
	1,	SC_LOWMARK,	false	},
    { "renumberlow",	"filename\t\tReset active file low article marks",
	1,	SC_LOWMARK,	false	},
    {	"xexec",	"path\t\t\tExec new server",
	1,	SC_XEXEC,	false	}
};



/*
**  Print a help summary.
*/
static void
Help(char *p)
{
    COMMAND	*cp;

    if (p == NULL) {
	printf("Command summary:\n");
	for (cp = Commands; cp < ARRAY_END(Commands); cp++)
	    printf("  %s %s\n", cp->Command, cp->Text);
	printf("*   Empty string means all sites/groups/etc.\n");
	printf("... All trailing words are glued together.\n");
	exit(0);
    }
    for (cp = Commands; cp < ARRAY_END(Commands); cp++)
	if (strcmp(p, cp->Command) == 0) {
	    printf("Command usage:\n");
	    printf("  %s %s\n", cp->Command, cp->Text);
	    exit(0);
	}
    printf("No such command.\n");
    exit(0);
}


/*
**  Print a command-usage message and exit.
*/
static void
WrongArgs(COMMAND *cp)
{
    printf("Wrong number of arguments -- usage:\n");
    printf("  %s %s\n", cp->Command, cp->Text);
    exit(1);
}


/*
**  Print an error message and exit.
*/
static void
Failed(const char *p)
{
    if (ICCfailure)
        syswarn("cannot %s (%s failure)", p, ICCfailure);
    else
        syswarn("cannot %s", p);
    ICCclose();
    exit(1);
}


/*
**  Print an error reporting incorrect usage.
*/
static void
Usage(const char *what)
{
    fprintf(stderr, "Usage error (%s) -- try -h for help.\n", what);
    exit(1);
}


int main(int ac, char *av[])
{
    static char		Y[] = "y";
    static char		EMPTY[] = "";
    COMMAND	        *cp;
    char	        *p;
    int	                i;
    bool		Silent;
    bool		NeedHelp;
    char		*reply;
    char		*new;
    int			length;
    char		*nv[4];
    struct stat		Sb;
    char		buff[SMBUF];

    /* First thing, set up our identity. */
    message_program_name = "ctlinnd";

    /* Set defaults. */
    if (!innconf_read(NULL))
        exit(1);
    Silent = false;
    NeedHelp = false;
    ICCsettimeout(CTLINND_TIMEOUT);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "hst:")) != EOF)
	switch (i) {
	default:
	    Usage("bad flags");
	    /* NOTREACHED */
	case 'h':		/* Get help			*/
	    NeedHelp = true;
	    break;
	case 's':		/* Silent -- no output		*/
	    Silent = true;
	    break;
	case 't':		/* Time to wait for reply	*/
	    ICCsettimeout(atoi(optarg));
	    break;
	}
    ac -= optind;
    av += optind;
    if (NeedHelp)
	Help(av[0]);
    if (ac == 0)
	Usage("missing command");

    /* Look up the command word and move to the arguments. */
    if (strcmp(av[0], "help") == 0)
	Help(av[1]);
    for (cp = Commands; cp < ARRAY_END(Commands); cp++)
	if (strcmp(av[0], cp->Command) == 0)
	    break;
    if (cp == ARRAY_END(Commands))
	Usage("unknown command");
    ac--;
    av++;

    /* Check argument count. */
    if (cp->Letter == SC_NEWGROUP) {
	/* Newgroup command has defaults. */
	switch (ac) {
	default:
	    WrongArgs(cp);
	    /* NOTREACHED */
	case 1:
	    nv[0] = av[0];
	    nv[1] = Y;
	    nv[2] = EMPTY;
	    nv[3] = NULL;
	    av = nv;
	    break;
	case 2:
	    nv[0] = av[0];
	    nv[1] = av[1];
	    nv[2] = EMPTY;
	    nv[3] = NULL;
	    av = nv;
	    break;
	case 3:
	    break;
	}
	ac = 3;
    }
    else if (ac > cp->argc && cp->Glue) {
	/* Glue any extra words together. */
	for (length = 0, i = cp->argc - 1; (p = av[i++]) != NULL; )
	    length += strlen(p) + 1;
        new = xmalloc(length);
        *new = '\0';
	for (i = cp->argc - 1; av[i]; i++) {
	    if (i >= cp->argc)
                strlcat(new, " ", length);
            strlcat(new, av[i], length);
	}
	av[cp->argc - 1] = new;
	av[cp->argc] = NULL;
    }
    else if (ac != cp->argc)
	/* All other commands must have the right number of arguments. */
	WrongArgs(cp);

    /* For newgroup and changegroup, make sure the mode is valid. */
    if (cp->Letter == SC_NEWGROUP || cp->Letter == SC_CHANGEGROUP) {
	switch (av[1][0]) {
	default:
	    Usage("Bad group mode");
	    /* NOTREACHED */
	case NF_FLAG_ALIAS:
	case NF_FLAG_EXCLUDED:
	case NF_FLAG_MODERATED:
	case NF_FLAG_OK:
	case NF_FLAG_NOLOCAL:
	case NF_FLAG_IGNORE:
	    break;
	}
    }

    /* Make sure there are no separators in the parameters. */
    for (i = 0; (p = av[i++]) != NULL; )
	if (strchr(p, SC_SEP) != NULL)
            die("illegal character \\%03o in %s", SC_SEP, p);

    /* Do the real work. */
    if (ICCopen() < 0)
	Failed("setup communication");
    i = ICCcommand(cp->Letter, (const char **) av, &reply);
    if (i < 0) {
	i = errno;
	p = concatpath(innconf->pathrun, _PATH_SERVERPID);
	if (stat(p, &Sb) < 0)
            warn("no innd.pid file; did server die?");
        free(p);
	snprintf(buff, sizeof(buff), "send \"%s\" command", cp->Command);
	errno = i;
	Failed(buff);
    }

    if (reply) {
	/* Skip "<exitcode><space>" part of reply. */
	for (p = reply; *p && CTYPE(isdigit, *p); p++)
	    continue;
	while (*p && ISWHITE(*p))
	    p++;
	if (i != 0)
            warn("%s", p);
	else if (!Silent)
	    printf("%s\n", p);
    }

    if (ICCclose() < 0)
	Failed("end communication");

    exit(i);
    /* NOTREACHED */
}
