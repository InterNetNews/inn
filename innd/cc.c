/*  $Id$
**
**  Routines for the control channel.
**
**  Create a Unix-domain datagram socket that processes on the local server
**  send messages to.  The control channel is used only by ctlinnd to tell
**  the server to perform special functions.  We use datagrams so that we
**  don't need to do an accept() and tie up another descriptor.  recvfrom
**  seems to be broken on several systems, so the client passes in the
**  socket name.
**
**  This module completely rips away all pretense of software layering.
*/

#include "config.h"
#include "clibrary.h"

#ifdef HAVE_UNIX_DOMAIN_SOCKETS
# include <sys/un.h>
#endif

#include "inn/innconf.h"
#include "inn/qio.h"
#include "innd.h"
#include "inndcomm.h"
#include "innperl.h"

/*
**  An entry in the dispatch table.  The name, and implementing function,
**  of every command we support.
*/
typedef struct _CCDISPATCH {
    char Name;
    int	argc;
    const char * (*Function)(char *av[]);
} CCDISPATCH;


static const char *	CCallow(char *av[]);
static const char *	CCbegin(char *av[]);
static const char *	CCchgroup(char *av[]);
static const char *	CCdrop(char *av[]);
static const char *	CCfeedinfo(char *av[]);
static const char *	CCflush(char *av[]);
static const char *	CCflushlogs(char *unused[]);
static const char *	CCgo(char *av[]);
static const char *	CChangup(char *av[]);
static const char *	CCreserve(char *av[]);
static const char *	CClogmode(char *unused[]);
static const char *	CCmode(char *unused[]);
static const char *	CCname(char *av[]);
static const char *	CCnewgroup(char *av[]);
static const char *	CCparam(char *av[]);
static const char *	CCpause(char *av[]);
static const char *	CCreaders(char *av[]);
static const char *	CCreject(char *av[]);
static const char *	CCreload(char *av[]);
static const char *	CCrenumber(char *av[]);
static const char *	CCrmgroup(char *av[]);
static const char *	CCsend(char *av[]);
static const char *	CCshutdown(char *av[]);
static const char *	CCsignal(char *av[]);
static const char *	CCstathist(char *av[]);
static const char *	CCstatus(char *av[]);
static const char *	CCthrottle(char *av[]);
static const char *	CCtimer(char *av[]);
static const char *	CCtrace(char *av[]);
static const char *	CCxabort(char *av[]);
static const char *	CCxexec(char *av[]);
static const char *	CCfilter(char *av[]);
static const char *	CCperl(char *av[]);
static const char *	CCpython(char *av[]);
static const char *	CClowmark(char *av[]);


static char		*CCpath = NULL;
static char		**CCargv;
static char		CCnosite[] = "1 No such site";
static char		CCwrongtype[] = "1 Wrong site type";
static char		CCnogroup[] = "1 No such group";
static char		CCnochannel[] = "1 No such channel";
static char		CCnoreason[] = "1 Empty reason";
static char		CCbigreason[] = "1 Reason too long";
static char		CCnotrunning[] = "1 Must be running";
static struct buffer	CCreply;
static CHANNEL		*CCchan;
static int		CCwriter;
static CCDISPATCH	CCcommands[] = {
    {	SC_ADDHIST,	5, CCaddhist	},
    {	SC_ALLOW,	1, CCallow	},
    {	SC_BEGIN,	1, CCbegin	},
    {	SC_CANCEL,	1, CCcancel	},
    {	SC_CHANGEGROUP,	2, CCchgroup	},
    {	SC_CHECKFILE,	0, CCcheckfile	},
    {	SC_DROP,	1, CCdrop	},
    {	SC_FEEDINFO,	1, CCfeedinfo	},
    {	SC_FILTER,	1, CCfilter     },
    {	SC_PERL,	1, CCperl       },
    {	SC_PYTHON,	1, CCpython     },
    {	SC_FLUSH,	1, CCflush	},
    {	SC_FLUSHLOGS,	0, CCflushlogs	},
    {	SC_GO,		1, CCgo		},
    {	SC_HANGUP,	1, CChangup	},
    {	SC_LOGMODE,	0, CClogmode	},
    {	SC_MODE,	0, CCmode	},
    {	SC_NAME,	1, CCname	},
    {	SC_NEWGROUP,	3, CCnewgroup	},
    {	SC_PARAM,	2, CCparam	},
    {	SC_PAUSE,	1, CCpause	},
    {	SC_READERS,	2, CCreaders	},
    {	SC_REJECT,	1, CCreject	},
    {	SC_RENUMBER,	1, CCrenumber	},
    {	SC_RELOAD,	2, CCreload	},
    {	SC_RESERVE,	1, CCreserve	},
    {	SC_RMGROUP,	1, CCrmgroup	},
    {	SC_SEND,	2, CCsend	},
    {	SC_SHUTDOWN,	1, CCshutdown	},
    {	SC_SIGNAL,	2, CCsignal	},
    {	SC_STATHIST,	1, CCstathist	},
    {	SC_STATUS,	1, CCstatus	},
    {	SC_THROTTLE,	1, CCthrottle	},
    {   SC_TIMER,       1, CCtimer      },
    {	SC_TRACE,	2, CCtrace	},
    {	SC_XABORT,	1, CCxabort	},
    {	SC_LOWMARK,	1, CClowmark	},
    {	SC_XEXEC,	1, CCxexec	}
};

static RETSIGTYPE CCresetup(int unused);


void
CCcopyargv(char *av[])
{
    char	**v;
    int		i;

    /* Get the vector size. */
    for (i = 0; av[i]; i++)
	continue;

    /* Get the vector, copy each element. */
    for (v = CCargv = xmalloc((i + 1) * sizeof(char *)); *av; av++) {
	/* not to renumber */
	if (strncmp(*av, "-r", 2) == 0)
	    continue;
	*v++ = xstrdup(*av);
    }
    *v = NULL;
}


/*
**  Return a string representing our operating mode.
*/
static const char *
CCcurrmode(void)
{
    static char		buff[32];

    /* Server's mode. */
    switch (Mode) {
    default:
	snprintf(buff, sizeof(buff), "Unknown %d", Mode);
	return buff;
    case OMrunning:
	return "running";
    case OMpaused:
	return "paused";
    case OMthrottled:
	return "throttled";
    }
}


/*
**  Add <> around Message-ID if needed.
*/
static const char *
CCgetid(char *p, const char **store)
{
    static char	NULLMESGID[] = "1 Empty Message-ID";
    static struct buffer Save = { 0, 0, 0, NULL };
    int i;

    if (*p == '\0')
	return NULLMESGID;
    if (*p == '<') {
	if (p[1] == '\0' || p[1] == '>')
	    return NULLMESGID;
	*store = p;
	return NULL;
    }

    /* Make sure the Message-ID buffer has room. */
    i = 1 + strlen(p) + 1 + 1;
    buffer_resize(&Save, i);
    *store = Save.data;
    snprintf(Save.data, Save.size, "<%s>", p);
    return NULL;
}


/*
**  Abort and dump core.
*/
static const char *
CCxabort(char *av[])
{
    syslog(L_FATAL, "%s abort %s", LogName, av[0]);
    abort();
    syslog(L_FATAL, "%s cant abort %m", LogName);
    CleanupAndExit(1, av[0]);
    /* NOTREACHED */
}


/*
**  Do the work needed to add a history entry.
*/
const char *
CCaddhist(char *av[])
{
    static char		DIGITS[] = "0123456789";
    ARTDATA		Data;
    const char *	p, *msgid;
    bool		ok;
    TOKEN		token;

    /* You must pass a <message-id> ID, the history API will hash it as it
     * wants */
    if ((p = CCgetid(av[0], &msgid)) != NULL)
	return p;

    /* If paused, don't try to use the history database since expire may be
       running */
    if (Mode == OMpaused)
	return "1 Server paused";

    /* If throttled by admin, briefly open the history database. */
    if (Mode != OMrunning) {
	if (ThrottledbyIOError)
	    return "1 Server throttled";
	InndHisOpen();
    }

    if (HIScheck(History, msgid)) {
	if (Mode != OMrunning) InndHisClose();
	return "1 Duplicate";
    }
    if (Mode != OMrunning) InndHisClose();
    if (strspn(av[1], DIGITS) != strlen(av[1]))
	return "1 Bad arrival date";
    Data.Arrived = atol(av[1]);
    if (strspn(av[2], DIGITS) != strlen(av[2]))
	return "1 Bad expiration date";
    Data.Expires = atol(av[2]);
    if (strspn(av[3], DIGITS) != strlen(av[3]))
	return "1 Bad posted date";
    Data.Posted = atol(av[3]);

    token = TextToToken(av[4]);
    if (Mode == OMrunning)
	ok = InndHisWrite(msgid, Data.Arrived, Data.Posted,
			  Data.Expires, &token);
    else {
	/* Possible race condition, but documented in ctlinnd manpage. */
	InndHisOpen();
	ok = InndHisWrite(msgid, Data.Arrived, Data.Posted,
			  Data.Expires, &token);
	InndHisClose();
    }
    return ok ? NULL : "1 Write failed";
}


/*
**  Do the work to allow foreign connectiosn.
*/
static const char *
CCallow(char *av[])
{
    char	*p;

    if (RejectReason == NULL)
	return "1 Already allowed";
    p = av[0];
    if (*p && strcmp(p, RejectReason) != 0)
	return "1 Wrong reason";
    free(RejectReason);
    RejectReason = NULL;
    return NULL;
}


/*
**  Do the work needed to start feeding a (new) site.
*/
static const char *
CCbegin(char *av[])
{
    SITE	*sp;
    int		i;
    int		length;
    char	*p;
    const char	*p1;
    char	**strings;
    NEWSGROUP	*ngp;
    const char	*error;
    char	*subbed;
    char	*poison;

    /* If site already exists, drop it. */
    if (SITEfind(av[0]) != NULL && (p1 = CCdrop(av)) != NULL)
	return p1;

    /* Find the named site. */
    length = strlen(av[0]);
    for (strings = SITEreadfile(true), i = 0; (p = strings[i]) != NULL; i++)
	if ((p[length] == NF_FIELD_SEP || p[length] == NF_SUBFIELD_SEP)
	 && strncasecmp(p, av[0], length) == 0) {
	    p = xstrdup(p);
	    break;
	}
    if (p == NULL)
	return CCnosite;

    if (p[0] == 'M' && p[1] == 'E' && p[2] == NF_FIELD_SEP)
	sp = &ME;
    else {
	/* Get space for the new site entry, and space for it in all
	 * the groups. */
	for (i = nSites, sp = Sites; --i >= 0; sp++)
	    if (sp->Name == NULL)
		break;
	if (i < 0) {
	    nSites++;
            Sites = xrealloc(Sites, nSites * sizeof(SITE));
	    sp = &Sites[nSites - 1];
	    sp->Next = sp->Prev = NOSITE;
	    for (i = nGroups, ngp = Groups; --i >= 0; ngp++) {
                ngp->Sites = xrealloc(ngp->Sites, nSites * sizeof(int));
                ngp->Poison = xrealloc(ngp->Poison, nSites * sizeof(int));
	    }
	}
    }

    /* Parse. */
    subbed = xmalloc(nGroups);
    poison = xmalloc(nGroups);
    error = SITEparseone(p, sp, subbed, poison);
    free(subbed);
    free(poison);
    if (error != NULL) {
	free((void *)p);
	syslog(L_ERROR, "%s bad_newsfeeds %s", av[0], error);
	return "1 Parse error";
    }

    if (sp != &ME && (!SITEsetup(sp) || !SITEfunnelpatch()))
	return "1 Startup error";
    SITEforward(sp, "begin");
    return NULL;
}


/*
**  Common code to change a group's flags.
*/
static const char *
CCdochange(NEWSGROUP *ngp, char *Rest)
{
    int			length;
    char		*p;

    if (ngp->Rest[0] == Rest[0]) {
	length = strlen(Rest);
	if (ngp->Rest[length] == '\n' && strncmp(ngp->Rest, Rest, length) == 0)
	    return "0 Group status unchanged";
    }
    if (Mode != OMrunning)
	return CCnotrunning;

    p = xstrdup(ngp->Name);
    if (!ICDchangegroup(ngp, Rest)) {
	syslog(L_NOTICE, "%s cant change_group %s to %s", LogName, p, Rest);
	free(p);
	return "1 Change failed (probably can't write active?)";
    }
    syslog(L_NOTICE, "%s change_group %s to %s", LogName, p, Rest);
    free(p);
    return NULL;
}


/*
**  Change the mode of a newsgroup.
*/
static const char *
CCchgroup(char *av[])
{
    NEWSGROUP	*ngp;
    char	*Rest;

    if ((ngp = NGfind(av[0])) == NULL)
	return CCnogroup;
    Rest = av[1];
    if (Rest[0] != NF_FLAG_ALIAS) {
	Rest[1] = '\0';
	if (CTYPE(isupper, Rest[0]))
	    Rest[0] = tolower(Rest[0]);
    }
    return CCdochange(ngp, Rest);
}


/*
**  Cancel a message.
*/
const char *
CCcancel(char *av[])
{
    ARTDATA	Data;
    const char *	p, *msgid;

    Data.Posted = Data.Arrived = Now.time;
    Data.Expires = 0;
    Data.Feedsite = "?";
    if ((p = CCgetid(av[0], &msgid)) != NULL)
	return p;

    Data.HdrContent[HDR__MESSAGE_ID].Value = (char *)msgid;
    Data.HdrContent[HDR__MESSAGE_ID].Length = strlen(msgid);
    if (Mode == OMrunning)
	ARTcancel(&Data, msgid, true);
    else {
	/* If paused, don't try to use the history database since expire may be
	   running */
	if (Mode == OMpaused)
	    return "1 Server paused";
	if (ThrottledbyIOError)
	    return "1 Server throttled";
	/* Possible race condition, but documented in ctlinnd manpage. */
	InndHisOpen();
	ARTcancel(&Data, msgid, true);
	InndHisClose();
    }
    if (innconf->logcancelcomm)
	syslog(L_NOTICE, "%s cancelled %s", LogName, msgid);
    return NULL;
}


/*
**  Syntax-check the newsfeeds file.
*/
const char *
CCcheckfile(char *unused[])
{
  char		**strings;
  char		*p;
  int		i;
  int		errors;
  const char *	error;
  SITE		fake;
  bool		needheaders, needoverview, needpath, needstoredgroup;
  bool		needreplicdata;

  unused = unused;		/* ARGSUSED */
  /* Parse all site entries. */
  strings = SITEreadfile(false);
  fake.Buffer.size = 0;
  fake.Buffer.data = NULL;
  /* save global variables not to be changed */
  needheaders = NeedHeaders;
  needoverview = NeedOverview;
  needpath = NeedPath;
  needstoredgroup = NeedStoredGroup;
  needreplicdata = NeedReplicdata;
  for (errors = 0, i = 0; (p = strings[i]) != NULL; i++) {
    if ((error = SITEparseone(p, &fake, (char *)NULL, (char *)NULL)) != NULL) {
      syslog(L_ERROR, "%s bad_newsfeeds %s", MaxLength(p, p), error);
      errors++;
    }
    SITEfree(&fake);
  }
  free(strings);
  /* restore global variables not to be changed */
  NeedHeaders = needheaders;
  NeedOverview = needoverview;
  NeedPath = needpath;
  NeedStoredGroup = needstoredgroup;
  NeedReplicdata = needreplicdata;

  if (errors == 0)
    return NULL;

  buffer_sprintf(&CCreply, false, "1 Found %d errors -- see syslog", errors);
  return CCreply.data;
}


/*
**  Drop a site.
*/
static const char *
CCdrop(char *av[])
{
    SITE	*sp;
    NEWSGROUP	*ngp;
    int		*ip;
    int		idx;
    int		i;
    int		j;

    if ((sp = SITEfind(av[0])) == NULL)
	return CCnosite;

    SITEdrop(sp);

    /* Loop over all groups, and if the site is in a group, clobber it. */
    for (idx = sp - Sites, i = nGroups, ngp = Groups; --i >= 0; ngp++) {
	for (j = ngp->nSites, ip = ngp->Sites; --j >= 0; ip++)
	    if (*ip == idx)
		*ip = NOSITE;
	for (j = ngp->nPoison, ip = ngp->Poison; --j >= 0; ip++)
	    if (*ip == idx)
		*ip = NOSITE;
    }

	return NULL;
}

/*
**  Return info on the feeds for one, or all, sites
*/
static const char *
CCfeedinfo(char *av[])
{
    SITE	*sp;
    char	*p;
    int		i;

    buffer_set(&CCreply, "0 ", 2);
    p = av[0];
    if (*p != '\0') {
	if ((sp = SITEfind(p)) == NULL)
	    return "1 No such site";

	SITEinfo(&CCreply, sp, true);
	while ((sp = SITEfindnext(p, sp)) != NULL)
	    SITEinfo(&CCreply, sp, true);
    }
    else
	for (i = nSites, sp = Sites; --i >= 0; sp++)
	    if (sp->Name)
		SITEinfo(&CCreply, sp, false);

    buffer_append(&CCreply, "", 1);
    return CCreply.data;
}


static const char *
CCfilter(char *av[] UNUSED)
{
#ifdef DO_TCL
    char	*p;

    switch (av[0][0]) {
    default:
	return "1 Bad flag";
    case 'y':
	if (TCLFilterActive)
	    return "1 tcl filter already enabled";
	TCLfilter(true);
	break;
    case 'n':
	if (!TCLFilterActive)
	    return "1 tcl filter already disabled";
	TCLfilter(false);
	break;
    }
    return NULL;
#else
    return "1 TCL filtering support not compiled in";
#endif
}


static const char *
CCperl(char *av[] UNUSED)
{
#ifdef DO_PERL
    switch (av[0][0]) {
    default:
	return "1 Bad flag";
    case 'y':
	if (PerlFilterActive)
	    return "1 Perl filter already enabled";
        else if (!PerlFilter(true))
            return "1 Perl filter not defined";
	break;
    case 'n':
	if (!PerlFilterActive)
	    return "1 Perl filter already disabled";
        PerlFilter(false);
	break;
    }
    return NULL;
#else
    return "1 Perl filtering support not compiled in";
#endif
}


static const char *
CCpython(char *av[] UNUSED)
{
#ifdef DO_PYTHON
    return PYcontrol(av);
#else
    return "1 Python filtering support not compiled in";
#endif
}


/*
**  Flush all sites or one site.
*/
static const char *
CCflush(char *av[])
{
    SITE	*sp;
    int		i;
    char	*p;

    p = av[0];
    if (*p == '\0') {
	ICDwrite();
	for (sp = Sites, i = nSites; --i >= 0; sp++)
	    SITEflush(sp, true);
	syslog(L_NOTICE, "%s flush_all", LogName);
    }
    else  {
	if ((sp = SITEfind(p)) == NULL)
	    return CCnosite;
	syslog(L_NOTICE, "%s flush", sp->Name);
	SITEflush(sp, true);
    }
    return NULL;
}


/*
**  Flush the log files.
*/
static const char *
CCflushlogs(char *unused[])
{
    unused = unused;		/* ARGSUSED */

    if (Debug)
	return "1 In debug mode";

    ICDwrite();
    syslog(L_NOTICE, "%s flushlogs %s", LogName, CCcurrmode());
    ReopenLog(Log);
    ReopenLog(Errlog);
    return NULL;
}


/*
**  Leave paused or throttled mode.
*/
static const char *
CCgo(char *av[])
{
    static char	YES[] = "y";
    char	*p;

    p = av[0];
    if (Reservation && strcmp(p, Reservation) == 0) {
	free(Reservation);
	Reservation = NULL;
    }
    if (RejectReason && strcmp(p, RejectReason) == 0) {
	free(RejectReason);
	RejectReason = NULL;
    }

    if (Mode == OMrunning)
	return "1 Already running";
    if (*p && strcmp(p, ModeReason) != 0)
	return "1 Wrong reason";

#if defined(DO_PERL)
    PLmode(Mode, OMrunning, p);
#endif /* defined(DO_PERL) */
#if defined(DO_PYTHON)
    PYmode(Mode, OMrunning, p);
#endif /* defined(DO_PYTHON) */
    
    free(ModeReason);
    ModeReason = NULL;
    Mode = OMrunning;
    ThrottledbyIOError = false;

    if (NNRPReason && !innconf->readerswhenstopped) {
	av[0] = YES;
	av[1] = p;
	CCreaders(av);
    }
    if (ErrorCount < 0)
	ErrorCount = IO_ERROR_COUNT;
    InndHisOpen();
    syslog(L_NOTICE, "%s running", LogName);
    if (ICDneedsetup)
	ICDsetup(true);
    SCHANwakeup(&Mode);
    return NULL;
}


/*
**  Hangup a channel.
*/
static const char *
CChangup(char *av[])
{
    CHANNEL	*cp;
    int		fd;
    char	*p;
    int		i;

    /* Parse the argument, a channel number. */
    for (p = av[0], fd = 0; *p; p++) {
	if (!CTYPE(isdigit, *p))
	    return "1 Bad channel number";
	fd = fd * 10 + *p - '0';
    }

    /* Loop over all channels for the desired one. */
    for (i = 0; (cp = CHANiter(&i, CTany)) != NULL; )
	if (cp->fd == fd) {
	    p = CHANname(cp);
	    switch (cp->Type) {
	    default:
		snprintf(CCreply.data, CCreply.size, "1 Can't close %s", p);
		return CCreply.data;
	    case CTexploder:
	    case CTprocess:
	    case CTfile:
	    case CTnntp:
	    case CTreject:
		syslog(L_NOTICE, "%s hangup", p);
		CHANclose(cp, p);
		return NULL;
	    }
	}
    return "1 Not active";
}


/*
**  Read a file containing lines of the form "newsgroup lowmark" and reset
**  the low article number for the specified groups.
*/
static const char *
CClowmark(char *av[])
{
    long lo;
    char *line, *cp;
    const char  *ret = NULL;
    QIOSTATE *qp;
    NEWSGROUP *ngp;

    if (Mode != OMrunning)
	return CCnotrunning;
    if (ICDneedsetup)
	return "1 Must first reload newsfeeds";
    if ((qp = QIOopen(av[0])) == NULL) {
	syslog(L_ERROR, "%s cant open %s %m", LogName, av[0]);
	return "1 Cannot read input file";
    }
    while ((line = QIOread(qp)) != NULL) {
	if (QIOerror(qp))
		break;
	if (QIOtoolong(qp)) {
	    ret = "1 Malformed input line (too long)";
	    break;
	}
	while (ISWHITE(*line))
	    line++;
	for (cp = line; *cp && !ISWHITE(*cp); cp++)
	    ;
	if (*cp == '\0') {
	    ret = "1 Malformed input line (only one field)";
	    break;
	}
	*cp++ = '\0';
	while (ISWHITE(*cp))
	    cp++;
	if (strspn(cp, "0123456789") != strlen(cp)) {
	    ret = "1 Malformed input line (non-digit in low mark)";
	    break;
	}
	if ((lo = atol(cp)) == 0 && (cp[0] != '0' || cp[1] != '\0')) {
	    ret = "1 Malformed input line (bad low mark)";
	    break;
	}
        if ((ngp = NGfind(line)) == NULL) {
	    /* ret = CCnogroup; break; */
	    continue;
	}
        if (!NGlowmark(ngp, lo)) {
	    ret = "1 Cannot set low mark - see syslog";
	    break;
	}
    }
    if (ret == NULL && QIOerror(qp)) {
	syslog(L_ERROR, "%s cant read %s %m", LogName, av[0]);
	ret = "1 Error reading input file";
    }
    QIOclose(qp);
    ICDwrite();
    return ret;
}


/*
**  Return our operating mode.
*/
static const char *
CCmode(char *unused[] UNUSED)
{
    int count, index;

#ifdef DO_PERL
    char *stats;
#endif

    buffer_sprintf(&CCreply, false, "0 Server ");

    /* Server's mode. */
    switch (Mode) {
    default:
        buffer_sprintf(&CCreply, true, "Unknown %d\n", Mode);
	break;
    case OMrunning:
        buffer_sprintf(&CCreply, true, "running\n");
	break;
    case OMpaused:
        buffer_sprintf(&CCreply, true, "paused %s\n", ModeReason);
	break;
    case OMthrottled:
        buffer_sprintf(&CCreply, true, "throttled %s\n", ModeReason);
	break;
    }
    if (RejectReason)
        buffer_sprintf(&CCreply, true, "Rejecting %s\n", RejectReason);
    else
        buffer_sprintf(&CCreply, true, "Allowing remote connections\n");

    /* Server parameters. */
    for (count = 0, index = 0; CHANiter(&index, CTnntp) != NULL; )
	count++;
    buffer_sprintf(&CCreply, true, "Parameters c %ld i %ld (%d) l %ld o %d"
                   " t %ld H %d T %d X %ld %s %s\n",
                  innconf->artcutoff, innconf->maxconnections, count,
                  innconf->maxartsize, MaxOutgoing, (long) TimeOut.tv_sec,
                  RemoteLimit, RemoteTotal, (long) RemoteTimer,
                  innconf->xrefslave ? "slave" : "normal",
                  AnyIncoming ? "any" : "specified");

    /* Reservation. */
    if (Reservation)
        buffer_sprintf(&CCreply, true, "Reserved %s\n", Reservation);
    else
        buffer_sprintf(&CCreply, true, "Not reserved\n");

    /* Newsreaders. */
    buffer_sprintf(&CCreply, true, "Readers ");
    if (innconf->readerswhenstopped)
        buffer_sprintf(&CCreply, true, "independent ");
    else
        buffer_sprintf(&CCreply, true, "follow ");
    if (NNRPReason == NULL)
        buffer_sprintf(&CCreply, true, "enabled");
    else
        buffer_sprintf(&CCreply, true, "disabled %s", NNRPReason);

#ifdef DO_TCL
    buffer_sprintf(&CCreply, true, "\nTcl filtering ");
    if (TCLFilterActive)
        buffer_sprintf(&CCreply, true, "enabled");
    else
        buffer_sprintf(&CCreply, true, "disabled");
#endif

#ifdef DO_PERL
    buffer_sprintf(&CCreply, true, "\nPerl filtering ");
    if (PerlFilterActive)
        buffer_sprintf(&CCreply, true, "enabled");
    else
        buffer_sprintf(&CCreply, true, "disabled");
    stats = PLstats();
    if (stats != NULL) {
        buffer_sprintf(&CCreply, true, "\nPerl filter stats: %s", stats);
        free(stats);
    }    
#endif

#ifdef DO_PYTHON
    buffer_sprintf(&CCreply, true, "\nPython filtering ");
    if (PythonFilterActive)
        buffer_sprintf(&CCreply, true, "enabled");
    else
        buffer_sprintf(&CCreply, true, "disabled");
#endif

    buffer_append(&CCreply, "", 1);
    return CCreply.data;
}


/*
**  Log our operating mode (via syslog).
*/
static const char *
CClogmode(char *unused[])
{
    unused = unused;		/* ARGSUSED */
    syslog(L_NOTICE, "%s servermode %s", LogName, CCcurrmode());
    return NULL;
}


/*
**  Name the channels.  ("Name the bats -- simple names.")
*/
static const char *
CCname(char *av[])
{
    CHANNEL *cp;
    int i, count;
    const char *mode;

    if (av[0][0] != '\0') {
        cp = CHANfromdescriptor(atoi(av[0]));
        if (cp == NULL)
	    return xstrdup(CCnochannel);
        buffer_sprintf(&CCreply, false, "0 %s", CHANname(cp));
	return CCreply.data;
    }
    buffer_set(&CCreply, "0 ", 2);
    for (count = 0, i = 0; (cp = CHANiter(&i, CTany)) != NULL; ) {
	if (cp->Type == CTfree)
	    continue;
	if (++count > 1)
	    buffer_append(&CCreply, "\n", 1);
        buffer_sprintf(&CCreply, true, "%s", CHANname(cp));
	switch (cp->Type) {
	case CTremconn:
            buffer_sprintf(&CCreply, true, ":remconn::");
	    break;
	case CTreject:
            buffer_sprintf(&CCreply, true, ":reject::");
	    break;
	case CTnntp:
            mode = (cp->MaxCnx > 0 && cp->ActiveCnx == 0) ? "paused" : "";
            buffer_sprintf(&CCreply, true, ":%s:%ld:%s",
                           cp->State == CScancel ? "cancel" : "nntp",
                           (long) Now.time - cp->LastActive, mode);
	    break;
	case CTlocalconn:
            buffer_sprintf(&CCreply, true, ":localconn::");
	    break;
	case CTcontrol:
            buffer_sprintf(&CCreply, true, ":control::");
	    break;
	case CTfile:
            buffer_sprintf(&CCreply, true, "::");
	    break;
	case CTexploder:
            buffer_sprintf(&CCreply, true, ":exploder::");
	    break;
	case CTprocess:
            buffer_sprintf(&CCreply, true, ":");
	    break;
	default:
            buffer_sprintf(&CCreply, true, ":unknown::");
	    break;
	}
    }
    buffer_append(&CCreply, "", 1);
    return CCreply.data;
}


/*
**  Create a newsgroup.
*/
static const char *
CCnewgroup(char *av[])
{
    static char		*TIMES = NULL;
    static char		WHEN[] = "updating active.times";
    int			fd;
    char		*p;
    NEWSGROUP		*ngp;
    char		*Name;
    char		*Rest;
    const char *		who;
    char		*buff;
    int			oerrno;
    size_t              length;

    if (TIMES == NULL)
	TIMES = concatpath(innconf->pathdb, _PATH_ACTIVETIMES);

    Name = av[0];
    if (Name[0] == '.' || strspn(Name, "0123456789") == strlen(Name))
	return "1 Illegal newsgroup name";
    for (p = Name; *p; p++)
	if (*p == '.') {
	    if (p[1] == '.' || p[1] == '\0')
		return "1 Double or trailing period in newsgroup name";
	}
	else if (ISWHITE(*p) || *p == ':' || *p == '!' || *p == '/')
	    return "1 Illegal character in newsgroup name";

    Rest = av[1];
    if (Rest[0] != NF_FLAG_ALIAS) {
	Rest[1] = '\0';
	if (CTYPE(isupper, Rest[0]))
	    Rest[0] = tolower(Rest[0]);
    }
    if (strlen(Name) + strlen(Rest) > SMBUF - 24)
	return "1 Name too long";

    if ((ngp = NGfind(Name)) != NULL)
	return CCdochange(ngp, Rest);

    if (Mode == OMthrottled && ThrottledbyIOError)
	return "1 server throttled";

    /* Update the log of groups created.  Don't use stdio because SunOS
     * 4.1 has broken libc which can't handle fd's greater than 127. */
    if ((fd = open(TIMES, O_WRONLY | O_APPEND | O_CREAT, 0664)) < 0) {
	oerrno = errno;
	syslog(L_ERROR, "%s cant open %s %m", LogName, TIMES);
	IOError(WHEN, oerrno);
    }
    else {
	who = av[2];
	if (*who == '\0')
	    who = NEWSMASTER;

        length = snprintf(NULL, 0, "%s %ld %s\n", Name, (long) Now.time, who) + 1;
        buff = xmalloc(length);
        snprintf(buff, length, "%s %ld %s\n", Name, (long) Now.time, who);
	if (xwrite(fd, buff, strlen(buff)) < 0) {
	    oerrno = errno;
	    syslog(L_ERROR, "%s cant write %s %m", LogName, TIMES);
	    IOError(WHEN, oerrno);
	}

	free(buff);
	
	if (close(fd) < 0) {
	    oerrno = errno;
	    syslog(L_ERROR, "%s cant close %s %m", LogName, TIMES);
	    IOError(WHEN, oerrno);
	}
    }

    /* Update the in-core data. */
    if (!ICDnewgroup(Name, Rest))
	return "1 Failed";
    syslog(L_NOTICE, "%s newgroup %s as %s", LogName, Name, Rest);

    return NULL;
}


/*
**  Parse and set a boolean flag.
*/
static bool
CCparsebool(char name, bool *bp, char value)
{
    switch (value) {
    default:
	return false;
    case 'y':
	*bp = false;
	break;
    case 'n':
	*bp = true;
	break;
    }
    syslog(L_NOTICE, "%s changed -%c %c", LogName, name, value);
    return true;
}


/*
**  Change a running parameter.
*/
static const char *
CCparam(char *av[])
{
    static char	BADVAL[] = "1 Bad value";
    char	*p;
    int		temp;

    p = av[1];
    switch (av[0][0]) {
    default:
	return "1 Unknown parameter";
    case 'a':
	if (!CCparsebool('a', (bool *)&AnyIncoming, *p))
	    return BADVAL;
	break;
    case 'c':
	innconf->artcutoff = atoi(p);
	syslog(L_NOTICE, "%s changed -c %ld", LogName, innconf->artcutoff);
	break;
    case 'i':
	innconf->maxconnections = atoi(p);
	syslog(L_NOTICE, "%s changed -i %ld", LogName, innconf->maxconnections);
	break;
    case 'l':
	innconf->maxartsize = atol(p);
	syslog(L_NOTICE, "%s changed -l %ld", LogName, innconf->maxartsize);
	break;
    case 'n':
	if (!CCparsebool('n', (bool *)&innconf->readerswhenstopped, *p))
	    return BADVAL;
	break;
    case 'o':
	MaxOutgoing = atoi(p);
	syslog(L_NOTICE, "%s changed -o %d", LogName, MaxOutgoing);
	break;
    case 't':
	TimeOut.tv_sec = atol(p);
	syslog(L_NOTICE, "%s changed -t %ld", LogName, (long)TimeOut.tv_sec);
	break;
    case 'H':
	RemoteLimit = atoi(p);
	syslog(L_NOTICE, "%s changed -H %d", LogName, RemoteLimit);
	break;
    case 'T':
        temp = atoi(p);
	if (temp > REMOTETABLESIZE) {
	    syslog(L_NOTICE, "%s -T must be lower than %d",
		   LogName, REMOTETABLESIZE+1);
	    temp = REMOTETABLESIZE;
	}
	syslog(L_NOTICE, "%s changed -T from %d to %d",
	       LogName, RemoteTotal, temp);
	RemoteTotal = temp;
	break;
    case 'X':
	RemoteTimer = (time_t) atoi(p);
	syslog(L_NOTICE, "%s changed -X %d", LogName, (int) RemoteTimer);
	break;
    }
    return NULL;
}


/*
**  Common code to implement a pause or throttle.
*/
const char *
CCblock(OPERATINGMODE NewMode, char *reason)
{
    static char		NO[] = "n";
    char *		av[2];

    if (*reason == '\0')
	return CCnoreason;

    if (strlen(reason) > MAX_REASON_LEN) /* MAX_REASON_LEN is as big as is safe */
	return CCbigreason;

    if (Reservation) {
	if (strcmp(reason, Reservation) != 0) {
	    snprintf(CCreply.data, CCreply.size, "1 Reserved \"%s\"",
                     Reservation);
	    return CCreply.data;
	}
	free(Reservation);
	Reservation = NULL;
    }

#ifdef DO_PERL
    PLmode(Mode, NewMode, reason);
#endif
#ifdef DO_PYTHON
    PYmode(Mode, NewMode, reason);
#endif

    ICDwrite();
    InndHisClose();
    Mode = NewMode;
    if (ModeReason)
	free(ModeReason);
    ModeReason = xstrdup(reason);
    if (NNRPReason == NULL && !innconf->readerswhenstopped) {
	av[0] = NO;
	av[1] = ModeReason;
	CCreaders(av);
    }
    syslog(L_NOTICE, "%s %s %s",
	LogName, NewMode == OMpaused ? "paused" : "throttled", reason);
    return NULL;
}


/*
**  Enter paused mode.
*/
static const char *
CCpause(char *av[])
{
    switch (Mode) {
    case OMrunning:
	return CCblock(OMpaused, av[0]);
    case OMpaused:
	return "1 Already paused";
    case OMthrottled:
	return "1 Already throttled";
    }
    return "1 Unknown mode";
}


/*
**  Allow or disallow newsreaders.
*/
static const char *
CCreaders(char *av[])
{
    const char	*p;

    switch (av[0][0]) {
    default:
	return "1 Bad flag";
    case 'y':
	if (NNRPReason == NULL)
	    return "1 Already allowing readers";
	p = av[1];
	if (*p && strcmp(p, NNRPReason) != 0)
	    return "1 Wrong reason";
	free(NNRPReason);
	NNRPReason = NULL;
	break;
    case 'n':
	if (NNRPReason)
	    return "1 Already not allowing readers";
	p = av[1];
	if (*p == '\0')
	    return CCnoreason;
	if (strlen(p) > MAX_REASON_LEN) /* MAX_REASON_LEN is as big as is safe */
	    return CCbigreason;
	NNRPReason = xstrdup(p);
	break;
    }
    return NULL;
}


/*
**  Re-exec ourselves.
*/
static const char *
CCxexec(char *av[])
{
    char	*innd;
    char	*p;
    int		i;

    if (CCargv == NULL)
	return "1 no argv!";
    
    innd = concatpath(innconf->pathbin, "innd");
    /* Get the pathname. */
    p = av[0];
    if (*p == '\0' || strcmp(p, "innd") == 0)
	CCargv[0] = innd;
    else
	return "1 Bad value";

    JustCleanup();
    syslog(L_NOTICE, "%s execv %s", LogName, CCargv[0]);

    /* Close all fds to protect possible fd leaking accross successive innds. */
    for (i=3; i<30; i++)
        close(i);

    execv(CCargv[0], CCargv);
    syslog(L_FATAL, "%s cant execv %s %m", LogName, CCargv[0]);
    _exit(1);
    /* NOTREACHED */
    return "1 Exit failed";
}

/*
**  Reject remote readers.
*/
static const char *
CCreject(char *av[])
{
    if (RejectReason)
	return "1 Already rejecting";
    if (strlen(av[0]) > MAX_REASON_LEN)	/* MAX_REASON_LEN is as big as is safe */
	return CCbigreason;
    RejectReason = xstrdup(av[0]);
    return NULL;
}


/*
**  Re-read all in-core data.
*/
static const char *
CCreload(char *av[])
{
    static char	BADSCHEMA[] = "1 Can't read schema";
    const char *p;

#ifdef DO_PERL
    static char BADPERLRELOAD[] = "1 Failed to define filter_art" ;
    char *path;
#endif

#ifdef DO_PYTHON
    static char BADPYRELOAD[] = "1 Failed to reload filter_innd.py" ;
#endif

    p = av[0];
    if (*p == '\0' || strcmp(p, "all") == 0) {
	SITEflushall(false);
        if (Mode == OMrunning)
	    InndHisClose();
	RCreadlist();
	if (Mode == OMrunning)
	    InndHisOpen();
	ICDwrite();
	ICDsetup(true);
	if (!ARTreadschema())
	    return BADSCHEMA;
#ifdef DO_TCL
	TCLreadfilter();
#endif
#ifdef DO_PERL
        path = concatpath(innconf->pathfilter, _PATH_PERL_FILTER_INND);
        PERLreadfilter(path, "filter_art") ;
        free(path);
#endif
#ifdef DO_PYTHON
	syslog(L_NOTICE, "reloading pyfilter");
        PYreadfilter();
	syslog(L_NOTICE, "reloaded pyfilter OK");
#endif
	p = "all";
    }
    else if (strcmp(p, "active") == 0 || strcmp(p, "newsfeeds") == 0) {
	SITEflushall(false);
	ICDwrite();
	ICDsetup(true);
    }
    else if (strcmp(p, "history") == 0) {
        if (Mode != OMrunning)
            return CCnotrunning;
	InndHisClose();
	InndHisOpen();
    }
    else if (strcmp(p, "incoming.conf") == 0)
	RCreadlist();
    else if (strcmp(p, "overview.fmt") == 0) {
	if (!ARTreadschema())
	    return BADSCHEMA;
    }
#if 0 /* we should check almost all innconf parameter, but the code
         is still incomplete for innd, so just commented out */
    else if (strcmp(p, "inn.conf") == 0) {
        struct innconf *saved;

        saved = innconf;
        innconf = NULL;
        if (innconf_read(NULL))
            innconf_free(saved);
        else {
            innconf = saved;
            return "1 Reload of inn.conf failed";
        }
	if (innconf->pathhost == NULL) {
	    syslog(L_FATAL, "%s No pathhost set", LogName);
	    exit(1);
	}   
	free(Path.Data);
	Path.Used = strlen(innconf->pathhost) + 1;
	Path.Data = xmalloc(Path.Used + 1);
	sprintf(Path.Data, "%s!", innconf->pathhost);
	if (Pathalias.Used > 0)
	    free(Pathalias.Data);
	if (innconf->pathalias == NULL) {
	    Pathalias.Used = 0;
	    Pathalias.Data = NULL;
	} else {
	    Pathalias.Used = strlen(innconf->pathalias) + 1;
	    Pathalias.Data = xmalloc(Pathalias.Used + 1);
	    sprintf(Pathalias.Data, "%s!", innconf->pathalias);
	}
    }
#endif
#ifdef DO_TCL
    else if (strcmp(p, "filter.tcl") == 0) {
	TCLreadfilter();
    }
#endif
#ifdef DO_PERL
    else if (strcmp(p, "filter.perl") == 0) {
        path = concatpath(innconf->pathfilter, _PATH_PERL_FILTER_INND);
        if (!PERLreadfilter(path, "filter_art"))
            return BADPERLRELOAD;
    }
#endif
#ifdef DO_PYTHON
    else if (strcmp(p, "filter.python") == 0) {
	if (!PYreadfilter())
	    return BADPYRELOAD;
    }
#endif
    else
	return "1 Unknown reload type";

    syslog(L_NOTICE, "%s reload %s %s", LogName, p, av[1]);
    return NULL;
}


/*
**  Renumber the active file.
*/
static const char *
CCrenumber(char *av[])
{
    static char	CANTRENUMBER[] = "1 Failed (see syslog)";
    char	*p;
    NEWSGROUP	*ngp;

    if (Mode != OMrunning)
	return CCnotrunning;
    if (ICDneedsetup)
	return "1 Must first reload newsfeeds";
    p = av[0];
    if (*p) {
	if ((ngp = NGfind(p)) == NULL)
	    return CCnogroup;
	if (!NGrenumber(ngp))
	    return CANTRENUMBER;
    }
    else if (!ICDrenumberactive())
	return CANTRENUMBER;
    return NULL;
}


/*
**  Reserve a lock.
*/
static const char *
CCreserve(char *av[])
{
    char	*p;

    if (Mode != OMrunning)
	return CCnotrunning;
    p = av[0];
    if (*p) {
	/* Trying to make a reservation. */
	if (Reservation)
	    return "1 Already reserved";
	if (strlen(p) > MAX_REASON_LEN) /* MAX_REASON_LEN is as big as is safe */
	    return CCbigreason;
	Reservation = xstrdup(p);
    }
    else {
	/* Trying to remove a reservation. */
	if (Reservation == NULL)
	    return "1 Not reserved";
	free(Reservation);
	Reservation = NULL;
    }
    return NULL;
}


/*
**  Remove a newsgroup.
*/
static const char *
CCrmgroup(char *av[])
{
    NEWSGROUP	*ngp;

    if ((ngp = NGfind(av[0])) == NULL)
	return CCnogroup;

    if (Mode == OMthrottled && ThrottledbyIOError)
	return "1 server throttled";

    /* Update the in-core data. */
    if (!ICDrmgroup(ngp))
	return "1 Failed";
    syslog(L_NOTICE, "%s rmgroup %s", LogName, av[0]);
    return NULL;
}


/*
**  Send a command line to an exploder.
*/
static const char *
CCsend(char *av[])
{
    SITE		*sp;

    if ((sp = SITEfind(av[0])) == NULL)
	return CCnosite;
    if (sp->Type != FTexploder)
	return CCwrongtype;
    SITEwrite(sp, av[1]);
    return NULL;
}


/*
**  Shut down the system.
*/
static const char *
CCshutdown(char *av[])
{
    syslog(L_NOTICE, "%s shutdown %s", LogName, av[0]);
    CleanupAndExit(0, av[0]);
    /* NOTREACHED */
    return "1 Exit failed";
}


/*
**  Send a signal to a site's feed.
*/
static const char *
CCsignal(char *av[])
{
    SITE	*sp;
    char	*p;
    int		s;
    int		oerrno;

    /* Parse the signal. */
    p = av[0];
    if (*p == '-')
	p++;
    if (strcasecmp(p, "HUP") == 0)
	s = SIGHUP;
    else if (strcasecmp(p, "INT") == 0)
	s = SIGINT;
    else if (strcasecmp(p, "TERM") == 0)
	s = SIGTERM;
    else if ((s = atoi(p)) <= 0)
	return "1 Invalid signal";

    /* Parse the site. */
    p = av[1];
    if ((sp = SITEfind(p)) == NULL)
	return CCnosite;
    if (sp->Type != FTchannel && sp->Type != FTexploder)
	return CCwrongtype;
    if (sp->Process < 0)
	return "1 Site has no process";

    /* Do it. */
    if (kill(sp->pid, s) < 0) {
	oerrno = errno;
	syslog(L_ERROR, "%s cant kill %ld %d site %s, %m", LogName, 
		(long) sp->pid, s, p);
        buffer_sprintf(&CCreply, false, "1 Can't signal process %ld: %s",
                       (long) sp->pid, strerror(oerrno));
	return CCreply.data;
    }

    return NULL;
}


/*
**  Enter throttled mode.
*/
static const char *
CCthrottle(char *av[])
{
    char	*p;

    p = av[0];
    switch (Mode) {
    case OMpaused:
	if (*p && strcmp(p, ModeReason) != 0)
	    return "1 Already paused";
	/* FALLTHROUGH */
    case OMrunning:
	return CCblock(OMthrottled, p);
    case OMthrottled:
	return "1 Already throttled";
    }
    return "1 unknown mode";
}

/*
**  Turn on or off performance monitoring
*/
static const char *
CCtimer(char *av[])
{
    int                 value;
    char                *p;
    
    if (strcmp(av[0], "off") == 0)
	value = 0;
    else {
	for (p = av[0]; *p; p++) {
	    if (!CTYPE(isdigit, *p))
		return "1 parameter should be a number or 'off'";
	}
	value = atoi(av[0]);
    }
    innconf->timer = value;
    if (innconf->timer)
        TMRinit(TMR_MAX);
    else
	TMRinit(0);
    return NULL;
}

/*
**  Log into filename some history stats
*/
static const char *
CCstathist(char *av[])
{
    if (strcmp(av[0], "off") == 0)
        HISlogclose();
    else
        HISlogto(av[0]);
    return NULL;
}

/*
**  Turn innd status creation on or off
*/
static const char *
CCstatus(char *av[])
{
    int                 value;
    char                *p;
    
    if (strcmp(av[0], "off") == 0)
	value = 0;
    else {
	for (p = av[0]; *p; p++) {
	    if (!CTYPE(isdigit, *p))
		return "1 parameter should be a number or 'off'";
	}
	value = atoi(av[0]);
    }
    innconf->status = value;
    return NULL;
}

/*
**  Add or remove tracing.
*/
static const char *
CCtrace(char *av[])
{
    char	*p;
    bool	Flag;
    const char *	word;
    CHANNEL	*cp;

    /* Parse the flag. */
    p = av[1];
    switch (p[0]) {
    default:			return "1 Bad trace flag";
    case 'y': case 'Y':		Flag = true;	word = "on";	break;
    case 'n': case 'N':		Flag = false;	word = "off";	break;
    }

    /* Parse what's being traced. */
    p = av[0];
    switch (*p) {
    default:
	return "1 Bad trace item";
    case 'i': case 'I':
	Tracing = Flag;
	syslog(L_NOTICE, "%s trace innd %s", LogName, word);
	break;
    case 'n': case 'N':
	NNRPTracing = Flag;
	syslog(L_NOTICE, "%s trace nnrpd %s", LogName, word);
	break;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
	if ((cp = CHANfromdescriptor(atoi(p))) == NULL)
	    return CCnochannel;
	CHANtracing(cp, Flag);
	break;
    }
    return NULL;
}



/*
**  Split up the text into fields and stuff them in argv.  Return the
**  number of elements or -1 on error.
*/
static int
CCargsplit(char *p, char *end, char **argv, int size)
{
    char		**save;

    for (save = argv, *argv++ = p, size--; p < end; p++)
	if (*p == SC_SEP) {
	    if (--size <= 0)
		return -1;
	    *p = '\0';
	    *argv++ = p + 1;
	}
    *argv = NULL;
    return argv - save;
}


/*
**  Read function.  Read and process the message.
*/
static void
CCreader(CHANNEL *cp)
{
    static char		TOOLONG[] = "0 Reply too long for server to send";
    CCDISPATCH		*dp;
    const char *	p;
    ICC_MSGLENTYPE	bufflen;
    ICC_PROTOCOLTYPE	protocol ;
#if	defined(HAVE_UNIX_DOMAIN_SOCKETS)
    struct sockaddr_un	client;
#else
    int			written;
#endif	/* defined(HAVE_UNIX_DOMAIN_SOCKETS) */
    int			i;
    char                buff[BIG_BUFFER + 2];
    char		*argv[SC_MAXFIELDS + 2];
    int			argc;
    int			len;
    char		*tbuff ;
    const char *start;
    char *copy;
    size_t offset;

    if (cp != CCchan) {
	syslog(L_ERROR, "%s internal CCreader wrong channel 0x%p not 0x%p",
	    LogName, (void *)cp, (void *)CCchan);
	return;
    }

#if defined (HAVE_UNIX_DOMAIN_SOCKETS)
    
    i = RECVorREAD(CCchan->fd, buff, BIG_BUFFER) ;
    if (i < 0) {
	syslog(L_ERROR, "%s cant recv CCreader %m", LogName);
	return;
    } else if (i == 0) {
	syslog(L_ERROR, "%s cant recv CCreader empty", LogName);
	return;
    } else if (i < (int)HEADER_SIZE) {
	syslog(L_ERROR, "%s cant recv CCreader header-length %m", LogName);
	return;
    }

    memcpy (&protocol,buff,sizeof (protocol)) ;
    memcpy (&bufflen,buff + sizeof (protocol),sizeof (bufflen)) ;
    bufflen = ntohs (bufflen) ;
    
    if (i != bufflen) {
	syslog(L_ERROR, "%s cant recv CCreader short-read %m", LogName);
	return;
    }

    i -= HEADER_SIZE ;
    memmove (buff,buff + HEADER_SIZE,i) ;
    buff[i] = '\0';

    if (protocol != ICC_PROTOCOL_1) {
        syslog(L_ERROR, "%s CCreader protocol mismatch", LogName) ;
        return ;
    }

#else  /* defined (HAVE_UNIX_DOMAIN_SOCKETS) */

    i = RECVorREAD(CCchan->fd, buff, HEADER_SIZE) ;
    if (i < 0) {
	syslog(L_ERROR, "%s cant read CCreader header %m", LogName);
	return;
    } else if (i == 0) {
	syslog(L_ERROR, "%s cant read CCreader header empty", LogName);
	return;
    } else if (i != HEADER_SIZE) {
	syslog(L_ERROR, "%s cant read CCreader header-length %m", LogName);
	return;
    }

    memcpy (&protocol,buff,sizeof (protocol)) ;
    memcpy (&bufflen,buff + sizeof (protocol),sizeof (bufflen)) ;
    bufflen = ntohs (bufflen) - HEADER_SIZE ;
    
    i = RECVorREAD(CCchan->fd, buff, bufflen) ;

    if (i < 0) {
	syslog(L_ERROR, "%s cant read CCreader buffer %m", LogName);
	return;
    } else if (i == 0) {
	syslog(L_ERROR, "%s cant read CCreader buffer empty", LogName);
	return;
    } else if (i != bufflen) {
	syslog(L_ERROR, "%s cant read CCreader buffer-length %m", LogName);
	return;
    }

    buff[i] = '\0';

    if (protocol != ICC_PROTOCOL_1) {
        syslog(L_ERROR, "%s CCreader protocol mismatch", LogName) ;
        return ;
    }

#endif /* defined (HAVE_UNIX_DOMAIN_SOCKETS) */
    
    /* Copy to a printable buffer, and log.  We skip the first
       SC_SEP-delimited field.  Note that the protocol allows for nuls in the
       message, which we'll replace with ? for logging. */
    copy = xmalloc(bufflen + 1);
    memcpy(copy, buff, bufflen);
    copy[bufflen] = '\0';
    for (offset = 0, start = NULL; offset < (size_t) bufflen; offset++)
        if (copy[offset] == SC_SEP) {
            copy[offset] = ':';
            if (start == NULL)
                start = copy + offset + 1;
        } else if (copy[offset] == '\0') {
            copy[offset] = '?';
        }
    notice("ctlinnd command %s", start != NULL ? start : copy);
    free(copy);

    /* Split up the fields, get the command letter. */
    if ((argc = CCargsplit(buff, &buff[i], argv, ARRAY_SIZE(argv))) < 2
     || argc == ARRAY_SIZE(argv)) {
	syslog(L_ERROR, "%s bad_fields CCreader", LogName);
	return;
    }
    p = argv[1];
    i = *p;

    /* Dispatch to the command function. */
    for (argc -= 2, dp = CCcommands; dp < ARRAY_END(CCcommands); dp++)
	if (i == dp->Name) {
	    if (argc != dp->argc)
		p = "1 Wrong number of parameters";
	    else
		p = (*dp->Function)(&argv[2]);
	    break;
	}
    if (dp == ARRAY_END(CCcommands)) {
	syslog(L_NOTICE, "%s bad_message %c", LogName, i);
	p = "1 Bad command";
    }
    else if (p == NULL)
	p = "0 Ok";

    /* Build the reply address and send the reply. */
    len = strlen(p) + HEADER_SIZE ;
    tbuff = xmalloc(len + 1);
    
    protocol = ICC_PROTOCOL_1 ;
    memcpy (tbuff,&protocol,sizeof (protocol)) ;
    tbuff += sizeof (protocol) ;
    
    bufflen = htons (len) ;
    memcpy (tbuff,&bufflen,sizeof (bufflen)) ;
    tbuff += sizeof (bufflen) ;

    strlcpy(tbuff, p, len + 1 - sizeof(protocol) - sizeof(bufflen));
    tbuff -= HEADER_SIZE ;

#if	defined(HAVE_UNIX_DOMAIN_SOCKETS)
    memset(&client, 0, sizeof client);
    client.sun_family = AF_UNIX;
    strlcpy(client.sun_path, argv[0], sizeof(client.sun_path));
    if (sendto(CCwriter, tbuff, len, 0,
	    (struct sockaddr *) &client, SUN_LEN(&client)) < 0) {
	i = errno;
	syslog(i == ENOENT ? L_NOTICE : L_ERROR,
	    "%s cant sendto CCreader bytes %d %m", LogName, len);
	if (i == EMSGSIZE)
	    sendto(CCwriter, TOOLONG, strlen(TOOLONG), 0,
		(struct sockaddr *) &client, SUN_LEN(&client));
    }
#else
    if ((i = open(argv[0], O_WRONLY | O_NDELAY)) < 0)
	syslog(L_ERROR, "%s cant open %s %m", LogName, argv[0]);
    else {
	if ((written = write(i, tbuff, len)) != len)
	    if (written < 0)
		syslog(L_ERROR, "%s cant write %s %m", LogName, argv[0]);
	    else
		syslog(L_ERROR, "%s cant write %s", LogName, argv[0]);
	if (close(i) < 0)
	    syslog(L_ERROR, "%s cant close %s %m", LogName, argv[0]);
    }
#endif	/* defined(HAVE_UNIX_DOMAIN_SOCKETS) */
    free (tbuff) ;
}


/*
**  Called when a write-in-progress is done on the channel.  Shouldn't happen.
*/
static void
CCwritedone(CHANNEL *unused)
{
    unused = unused;		/* ARGSUSED */
    syslog(L_ERROR, "%s internal CCwritedone", LogName);
}


/*
**  Create the channel.
*/
void
CCsetup(void)
{
    int			i;
#if	defined(HAVE_UNIX_DOMAIN_SOCKETS)
    struct sockaddr_un	server;
#endif	/* defined(HAVE_UNIX_DOMAIN_SOCKETS) */

    if (CCpath == NULL)
	CCpath = concatpath(innconf->pathrun, _PATH_NEWSCONTROL);
    /* Remove old detritus. */
    if (unlink(CCpath) < 0 && errno != ENOENT) {
	syslog(L_FATAL, "%s cant unlink %s %m", LogName, CCpath);
	exit(1);
    }

#if	defined(HAVE_UNIX_DOMAIN_SOCKETS)
    /* Create a socket and name it. */
    if ((i = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
	syslog(L_FATAL, "%s cant socket %s %m", LogName, CCpath);
	exit(1);
    }
    memset(&server, 0, sizeof server);
    server.sun_family = AF_UNIX;
    strlcpy(server.sun_path, CCpath, sizeof(server.sun_path));
    if (bind(i, (struct sockaddr *) &server, SUN_LEN(&server)) < 0) {
	syslog(L_FATAL, "%s cant bind %s %m", LogName, CCpath);
	exit(1);
    }

    /* Create an unbound socket to reply on. */
    if ((CCwriter = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
	syslog(L_FATAL, "%s cant socket unbound %m", LogName);
	exit(1);
    }
#else
    /* Create a named pipe and open it. */
    if (mkfifo(CCpath, 0666) < 0) {
	syslog(L_FATAL, "%s cant mkfifo %s %m", LogName, CCpath);
	exit(1);
    }
    if ((i = open(CCpath, O_RDWR)) < 0) {
	syslog(L_FATAL, "%s cant open %s %m", LogName, CCpath);
	exit(1);
    }
#endif	/* defined(HAVE_UNIX_DOMAIN_SOCKETS) */

    CCchan = CHANcreate(i, CTcontrol, CSwaiting, CCreader, CCwritedone);
    syslog(L_NOTICE, "%s ccsetup %s", LogName, CHANname(CCchan));
    RCHANadd(CCchan);

    buffer_resize(&CCreply, SMBUF);

    /*
     *  Catch SIGUSR1 so that we can recreate the control channel when
     *  needed (i.e. something has deleted our named socket.
     */
#if     defined(SIGUSR1)
    xsignal(SIGUSR1, CCresetup);
#endif  /* defined(SIGUSR1) */
}


/*
**  Cleanly shut down the channel.
*/
void
CCclose(void)
{
    CHANclose(CCchan, CHANname(CCchan));
    CCchan = NULL;
    if (unlink(CCpath) < 0)
	syslog(L_ERROR, "%s cant unlink %s %m", LogName, CCpath);
    free(CCpath);
    CCpath = NULL;
    free(CCreply.data);
    CCreply.data = NULL;
    CCreply.size = 0;
    CCreply.used = 0;
    CCreply.left = 0;
#if	defined(HAVE_UNIX_DOMAIN_SOCKETS)
    if (close(CCwriter) < 0)
	syslog(L_ERROR, "%s cant close unbound %m", LogName);
#endif	/* defined(HAVE_UNIX_DOMAIN_SOCKETS) */
}


/*
**  Restablish the control channel.
*/
static RETSIGTYPE
CCresetup(int unused)
{
#ifndef HAVE_SIGACTION
    xsignal(s, CCresetup);
#else
    unused = unused;		/* ARGSUSED */
#endif
    CCclose();
    CCsetup();
}
