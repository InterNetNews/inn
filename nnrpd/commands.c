/*  $Revision$
**
**  Miscellaneous commands.
*/
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "configdata.h"
#include "clibrary.h"
#include "nnrpd.h"
#include <time.h>


typedef struct _LISTINFO {
    STRING	Path;
    STRING	File;
    BOOL	Required;
    STRING	Items;
    STRING	Format;
} LISTINFO;


STATIC LISTINFO		INFOactive = {
    NULL, _PATH_ACTIVE, TRUE, "active newsgroups",
    "Newsgroups in form \"group high low flags\""
};
STATIC LISTINFO		INFOactivetimes = {
    NULL, _PATH_ACTIVETIMES, FALSE, "creation times",
    "Group creations in form \"name time who\""
};
STATIC LISTINFO		INFOdistribs = {
    NULL, _PATH_NNRPDIST, FALSE, "newsgroup distributions",
    "Distributions in form \"area description\""
};
STATIC LISTINFO               INFOsubs = {
    NULL, _PATH_NNRPSUBS, FALSE, "automatic group subscriptions",
    "Subscriptions in form \"group\""
};
STATIC LISTINFO		INFOdistribpats = {
    NULL, _PATH_DISTPATS, FALSE, "distribution patterns",
    "Default distributions in form \"weight:pattern:value\""
};
STATIC LISTINFO		INFOgroups = {
    NULL, _PATH_NEWSGROUPS, FALSE, "newsgroup descriptions",
    "Descriptions in form \"group description\""
};
STATIC LISTINFO		INFOmoderators = {
    NULL, _PATH_MODERATORS, FALSE, "moderator patterns",
    "Newsgroup moderators in form \"group-pattern:mail-address-pattern\""
};
STATIC LISTINFO		INFOschema = {
    NULL, _PATH_SCHEMA, TRUE, "overview format",
    "Order of fields in overview database"
};
STATIC LISTINFO		INFOmotd = {
    NULL, _PATH_MOTD, FALSE, "motd",
    "Message of the day text."
};



/* returns:
	-1 for problem (such as no such authenticator etc.)
	0 for authentication succeeded
	1 for authentication failed
 */

static char *PERMauthstring;

int
PERMgeneric(av, accesslist)
    char	*av[];
    char	*accesslist;
{
    char path[BIG_BUFFER], *fields[6], *p;
    int i, pan[2], status;
    PID_T pid;
    struct stat stb;

    av += 2;

    PERMcanread = FALSE;
    PERMcanpost = FALSE;
    PERMlocpost = FALSE;

    if (!*av) {
	Reply("%d no authenticator\r\n", NNTP_SYNTAX_VAL);
	return(-1);
    }

    /* check for ../.  I'd use strstr, but there doesn't appear to
       be any other references for it, and I don't want to break
       portability */
    for (p = av[0]; *p; p++)
	if (EQn(p, "../", 3)) {
	    Reply("%d ../ in authenticator %s\r\n", NNTP_SYNTAX_VAL, av[0]);
	    return(-1);
	}

    if (strchr(_PATH_AUTHDIR,'/') == NULL)
	(void)sprintf(path, "%s/%s/%s", innconf->pathbin, _PATH_AUTHDIR, av[0]);
    else
	(void)sprintf(path, "%s/%s", _PATH_AUTHDIR, av[0]);

#if !defined(S_IXUSR) && defined(_S_IXUSR)
#define S_IXUSR _S_IXUSR
#endif /* !defined(S_IXUSR) && defined(_S_IXUSR) */
    
#if !defined(S_IXUSR) && defined(S_IEXEC)
#define S_IXUSR S_IEXEC
#endif  /* !defined(S_IXUSR) && defined(S_IEXEC) */

    if (stat(path, &stb) || !(stb.st_mode&S_IXUSR)) {
	Reply("%d No such authenticator %s\r\n", NNTP_TEMPERR_VAL, av[0]);
	return -1;
    }
	

    /* Create a pipe. */
    if (pipe(pan) < 0) {
	syslog(L_FATAL, "cant pipe for %s %m", av[0]);
	return -1;
    }

    for (i = 0; (pid = FORK()) < 0; i++) {
	if (i == innconf->maxforks) {
	    Reply("%d Can't fork %s\r\n", NNTP_TEMPERR_VAL,
		strerror(errno));
	    syslog(L_FATAL, "cant fork %s %m", av[0]);
	    return -1;
	}
	syslog(L_NOTICE, "cant fork %s -- waiting", av[0]);
	(void)sleep(5);
    }

    /* Run the child, with redirection. */
    if (pid == 0) {
	(void)close(STDERR);		/* close existing stderr */
	(void)close(pan[PIPE_READ]);

	/* stderr goes down the pipe. */
	if (pan[PIPE_WRITE] != STDERR) {
	    if ((i = dup2(pan[PIPE_WRITE], STDERR)) != STDERR) {
		syslog(L_FATAL, "cant dup2 %d to %d got %d %m",
		    pan[PIPE_WRITE], STDERR, i);
		_exit(1);
	    }
	    (void)close(pan[PIPE_WRITE]);
	}

	CloseOnExec(STDIN, FALSE);
	CloseOnExec(STDOUT, FALSE);
	CloseOnExec(STDERR, FALSE);

	(void)execv(path, av);
	Reply("%s\r\n", NNTP_BAD_COMMAND);

	syslog(L_FATAL, "cant execv %s %m", path);
	_exit(1);
    }

    (void)close(pan[PIPE_WRITE]);
    i = read(pan[PIPE_READ], path, sizeof(path));

    if ((p = strchr(path, '\n')) != NULL)
	*p = '\0';

    if (PERMauthstring)
	free(PERMauthstring);

    PERMauthstring = malloc(strlen(path) + 1);
    if (PERMauthstring)
	strcpy(PERMauthstring, path);

    while( pid != waitnb(&status) );

    /*syslog(L_NOTICE, "%s (%ld) returned: %d %s %d\n", av[0], (long) pid, i, path, status);*/
    /* Split "host:permissions:user:pass:groups" into fields. */
    for (fields[0] = path, i = 0, p = path; *p; p++)
	if (*p == ':') {
	    *p = '\0';
	    fields[++i] = p + 1;
	}

    PERMcanread = strchr(fields[1], 'R') != NULL;
    PERMcanpost = strchr(fields[1], 'P') != NULL;
    PERMlocpost = strchr(fields[1], 'L') != NULL;
    if (strchr(fields[1], 'N') != NULL) PERMnewnews = TRUE;
    sprintf(PERMuser, "%s@%s", fields[2], fields[0]);
    (void)strcpy(PERMpass, fields[3]);
    (void)strcpy(accesslist, fields[4]);
    /*(void)strcpy(writeaccess, fields[5]); future work? */

    /*for (i = 0; fields[i] && i < 6; i++)
	printf("fields[%d] = %s\n", i, fields[i]);*/

    return !status;
}

/* ARGSUSED */
FUNCTYPE
CMDauthinfo(ac, av)
    int		ac;
    char	*av[];
{
    static char	User[30];
    static char	Password[30];
    char	accesslist[BIG_BUFFER];

    if (caseEQ(av[1], "generic")) {
	char *logrec = Glom(av);

	strcpy(PERMuser, "<none>");

	switch (PERMgeneric(av, accesslist)) {
	    case 1:
		PERMspecified = NGgetlist(&PERMlist, accesslist);
		syslog(L_NOTICE, "%s auth %s (%s -> %s)", ClientHost, PERMuser,
			logrec, PERMauthstring? PERMauthstring: "" );
		Reply("%d Authentication succeeded\r\n", NNTP_AUTH_OK_VAL);
		PERMneedauth = FALSE;
		PERMauthorized = TRUE;
		free(logrec);
		return;
	    case 0:
		syslog(L_NOTICE, "%s bad_auth %s (%s)", ClientHost, PERMuser,
			logrec);
		Reply("%d Authentication failed\r\n", NNTP_ACCESS_VAL);
		free(logrec);
		ExitWithStats(1);
	    default:
		/* lower level has issued Reply */
		return;
	}

    } else {

	if (caseEQ(av[1], "user")) {
	    (void)strncpy(User, av[2], sizeof User - 1);
	    User[sizeof User - 1] = 0;
	    Reply("%d PASS required\r\n", NNTP_AUTH_NEXT_VAL);
	    return;
	}

	if (!caseEQ(av[1], "pass")) {
	    Reply("%d bad authinfo param\r\n", NNTP_BAD_COMMAND_VAL);
	    return;
	}
	if (User[0] == '\0') {
	    Reply("%d USER required\r\n", NNTP_AUTH_REJECT_VAL);
	    return;
	}

	(void)strncpy(Password, av[2], sizeof Password - 1);
	Password[sizeof Password - 1] = 0;

	if (EQ(User, PERMuser) && EQ(Password, PERMpass)) {
	    syslog(L_NOTICE, "%s user %s", ClientHost, User);
	    Reply("%d Ok\r\n", NNTP_AUTH_OK_VAL);
	    PERMneedauth = FALSE;
	    PERMauthorized = TRUE;
	    return;
	}
	if (PERMinfile((char *)NULL, (char *)NULL, User, Password,
		accesslist, NNRPACCESS)) {
	    PERMspecified = NGgetlist(&PERMlist, accesslist);
	    syslog(L_NOTICE, "%s user %s", ClientHost, User);
	    Reply("%d Ok\r\n", NNTP_AUTH_OK_VAL);
	    PERMneedauth = FALSE;
	    PERMauthorized = TRUE;
	    return;
	}

	syslog(L_NOTICE, "%s bad_auth", ClientHost);
	Reply("%d Authentication error\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1);

    }

}


/*
**  The "DATE" command.  Part of NNTPv2.
*/
/* ARGSUSED0 */
FUNCTYPE
CMDdate(ac, av)
    int		ac;
    char	*av[];
{
    TIMEINFO	t;
    struct tm	*gmt;

    if (GetTimeInfo(&t) < 0 || (gmt = gmtime(&t.time)) == NULL) {
	Reply("%d Can't get time, %s\r\n", NNTP_TEMPERR_VAL, strerror(errno));
	return;
    }
    Reply("%d %04.4d%02.2d%02.2d%02.2d%02.2d%02.2d\r\n",
	NNTP_DATE_FOLLOWS_VAL,
	gmt->tm_year + 1900, gmt->tm_mon + 1, gmt->tm_mday,
	gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
}


/*
**  List active newsgroups, newsgroup descriptions, and distributions.
*/
/* ARGSUSED0 */
FUNCTYPE
CMDlist(ac, av)
    int			ac;
    char		*av[];
{
    register QIOSTATE	*qp;
    register char	*p;
    register char	*save;
    char		*q;
    char		*grplist[2];
    LISTINFO		*lp;
    char		*wildarg = NULL;
    char		savec;

    p = av[1];
    if (p == NULL || caseEQ(p, "active")) {
	static time_t	last_time;
	time_t		now;
	(void)time(&now);
	lp = &INFOactive;
	if (ac < 3 || now > last_time + NNRP_RESCAN_DELAY * 3) {
	    if (last_time && !GetGroupList()) {
		syslog(L_NOTICE, "%s cant getgroupslist for list %m", ClientHost);
		Reply("%d Group update failed. Try later.\r\n", NNTP_TEMPERR_VAL);
		ExitWithStats(1);
	    }
	    last_time = now;
	}
	if (ac == 3) {
	    GROUPENTRY *gp = GRPfind(av[2]);
	    if (gp) {
		grplist[0] = av[2];
		grplist[1] = NULL;
		Reply("%d list:\r\n", NNTP_LIST_FOLLOWS_VAL);
		if (PERMmatch(PERMdefault, PERMlist, grplist))
			Printf("%s %ld %ld %c%s\r\n.\r\n",
		      		GPNAME(gp), (long)GPHIGH(gp), (long)GPLOW(gp),
		      		GPFLAG(gp), GPALIAS(gp) ? GPALIAS(gp) : "");
		else
			Printf(".\r\n");
		return;
	    }
	    wildarg = av[2];
	}
	lp = &INFOactive;
    }
    else if (caseEQ(p, "active.times"))
	lp = &INFOactivetimes;
    else if (caseEQ(p, "distributions"))
	lp = &INFOdistribs;
    else if (caseEQ(p, "subscriptions"))
	lp = &INFOsubs;
    else if (caseEQ(p, "distrib.pats"))
	lp = &INFOdistribpats;
    else if (caseEQ(p, "moderators"))
	lp = &INFOmoderators;
    else if (caseEQ(p,"motd"))
	lp = &INFOmotd;
    else if (caseEQ(p, "newsgroups")) {
	if (ac == 3)
	    wildarg = av[2];
  	lp = &INFOgroups;
    }
    else if (caseEQ(p, "overview.fmt"))
	lp = &INFOschema;
    else {
	Reply("%s\r\n", NNTP_SYNTAX_USE);
	return;
    }
    if (ac > 2 && !wildarg) {
	Reply("%s\r\n", NNTP_SYNTAX_USE);
	return;
    }
    lp->Path = innconf->pathetc;
    if ((strstr(lp->File, "active") != NULL) ||
	(strstr(lp->File, "newsgroups") != NULL))
		lp->Path = innconf->pathdb;
    if (strchr(lp->File, '/') != NULL)
	lp->Path = "";
    if ((qp = QIOopen(cpcatpath((char *)lp->Path, (char *)lp->File))) == NULL) {
	if (!lp->Required && errno == ENOENT) {
	    Reply("%d %s.\r\n", NNTP_LIST_FOLLOWS_VAL, lp->Format);
	    Printf(".\r\n");
	}
	else {
	    syslog(L_ERROR, "%s cant fopen %s %m", ClientHost, lp->File);
	    Reply("%d No list of %s available.\r\n",
		NNTP_TEMPERR_VAL, lp->Items);
	}
	return;
    }

    Reply("%d %s.\r\n", NNTP_LIST_FOLLOWS_VAL, lp->Format);
    if (!PERMspecified && !PERMdefault) {
	/* Optmize for unlikely case of no permissions and FALSE default. */
	(void)QIOclose(qp);
	Printf(".\r\n");
    }

    /* Set up group list terminator. */
    grplist[1] = NULL;

    /* Read lines, ignore long ones. */
    while ((p = QIOread(qp)) != NULL) {
	if (lp == &INFOmotd) {
	    Printf("%s\r\n", p);
	    continue;
	}
	/* matching patterns against patterns is not that
	   good but it's better than nothing ... */
	if (lp == &INFOdistribpats) {
	    if (*p == '\0' || *p == '#' || *p == ';' || *p == ' ')
		continue;
	    if (PERMspecified) {
		if ((q = strchr(p, ':')) == NULL)
	    	    continue;
		q++;
		if ((save = strchr(q, ':')) == NULL)
		    continue;
		*save = '\0';
		grplist[0] = q;
		if (!PERMmatch(PERMdefault, PERMlist, grplist))
		    continue;
		*save = ':';
	    }
	    Printf("%s\r\n", p);
	    continue;
	}
	if (lp == &INFOdistribs || lp == &INFOmoderators ||
	    lp == &INFOschema) {
	    if (*p != '\0' && *p != '#' && *p != ';' && *p != ' ')
		Printf("%s\r\n", p);
	    continue;
	}
	savec = '\0';
	for (save = p; *save != '\0'; save++) {
	    if (*save == ' ' || *save == '\t') {
		savec = *save;
		*save = '\0';
		break;
	    }
	}
	      
	if (PERMspecified) {
	    grplist[0] = p;
	    if (!PERMmatch(PERMdefault, PERMlist, grplist))
		continue;
	}
	if (wildarg && !wildmat(p, wildarg))
	    continue;
	if (savec != '\0')
	    *save = savec;
	Printf("%s\r\n", p);
    }
    QIOclose(qp);

    Printf(".\r\n");
}


/*
**  Handle the "mode" command.
*/
/* ARGSUSED */
FUNCTYPE
CMDmode(ac, av)
    int		ac;
    char	*av[];
{
    if (caseEQ(av[1], "reader"))
	Reply("%d %s InterNetNews NNRP server %s ready (%s).\r\n",
	       PERMcanpost ? NNTP_POSTOK_VAL : NNTP_NOPOSTOK_VAL,
	       innconf->pathhost, INNVersion(),
	       PERMcanpost ? "posting ok" : "no posting");
    else
	Reply("%d What?\r\n", NNTP_BAD_COMMAND_VAL);
}


/*
**  Display new newsgroups since a given date and time for specified
**  <distributions>.
*/
FUNCTYPE
CMDnewgroups(ac, av)
    int			ac;
    char		*av[];
{
    static char		USAGE[] =
	"NEWGROUPS yymmdd hhmmss [\"GMT\"] [<distributions>]";
    static char		**distlist;
    register char	*p;
    register char	*q;
    register char	**dp;
    register QIOSTATE	*qp;
    register GROUPENTRY	*gp;
    BOOL		All;
    long		date;
    char		*grplist[2];

    /* Parse the date. */
    date = NNTPtoGMT(av[1], av[2]);
    if (date < 0) {
	Reply("%d Usage: %s\r\n", NNTP_SYNTAX_VAL, USAGE);
	return;
    }
    ac -= 3;
    av += 3;
    if (ac > 0 && caseEQ(*av, "GMT")) {
	av++;
	ac--;
    }
    else
	date = LOCALtoGMT(date);

    if (ac == 0)
	All = TRUE;
    else {
	if (!ParseDistlist(&distlist, *av)) {
	    Reply("%d Bad distribution list %s:\r\n", NNTP_SYNTAX_VAL, *av);
	    return;
	}
	All = FALSE;
    }

    if (!GetGroupList()) {
	syslog(L_NOTICE, "%s cant getgroupslist for list %m", ClientHost);
	Reply("%d Group update failed. Try later.\r\n", NNTP_TEMPERR_VAL);
	ExitWithStats(1);
    }

    if ((qp = QIOopen(ACTIVETIMES)) == NULL) {
	syslog(L_ERROR, "%s cant fopen %s %m",
	    ClientHost, ACTIVETIMES);
	Reply("%d Cannot open newsgroup date file.\r\n", NNTP_TEMPERR_VAL);
	return;
    }
    Reply("%d New newsgroups follow.\r\n", NNTP_NEWGROUPS_FOLLOWS_VAL);

    /* Set up group list terminator. */
    grplist[1] = NULL;

    /* Read the file, ignoring long lines. */
    while ((p = QIOread(qp)) != NULL) {
	if ((q = strchr(p, ' ')) == NULL)
	    continue;
	*q++ = '\0';
	if (atol(q) < date || (gp = GRPfind(p)) == NULL)
	    continue;

	if (PERMspecified) {
	    grplist[0] = p;
	    grplist[1] = NULL;
	    if (!PERMmatch(PERMdefault, PERMlist, grplist))
		continue;
	}
	else if (!PERMdefault)
	    continue;

	if (!All) {
	    if ((q = strchr(p, '.')) == NULL)
		continue;
	    for (*q = '\0', dp = distlist; *dp; dp++)
		if (EQ(p, *dp)) {
		    *q = '.';
		    break;
		}
	    if (*dp == NULL)
		continue;
	}
	Printf("%s %ld %ld %c%s\r\n",
	    p, (long)GPHIGH(gp), (long)GPLOW(gp),
	    GPFLAG(gp), GPALIAS(gp) ? GPALIAS(gp) : "");
    }
    QIOclose(qp);
    Printf(".\r\n");
}


/*
**  Post an article.
*/
/* ARGSUSED */
FUNCTYPE
CMDpost(ac, av)
    int		ac;
    char	*av[];
{
    static char		*article;
    static int		size;
    register char	*p;
    register char	*end;
    register int	longline;
    register READTYPE	r;
    int			i;
    long		l;
    long                sleeptime;
    char               *path;
    STRING		response;
    char		idbuff[SMBUF];

    if (!PERMcanpost) {
	syslog(L_NOTICE, "%s noperm post without permission", ClientHost);
	Reply("%s\r\n", NNTP_CANTPOST);
	return;
    }

    /* Dave's posting limiter - Limit postings to a certain rate
     * And now we support multiprocess rate limits. Questions?
     * Email dave@jetcafe.org.
     */
    if (BACKOFFenabled) {

      /* Acquire lock (this could be in RateLimit but that would
       * invoke the spaghetti factor). 
       */
      if ((path = (char *) PostRecFilename(ClientIP,PERMuser)) == NULL) {
        syslog(L_ERROR,"%s Unable to create postrec directories: %s",
               ClientHost,strerror(errno));
        Reply("%s\r\n", NNTP_CANTPOST);
        return;
      }
      
      if (LockPostRec(path) == 0) {
        syslog(L_ERROR, "%s Error write locking '%s'",
               ClientHost, path);
        Reply("%s\r\n", NNTP_CANTPOST);
        return;
      }
      
      if (!RateLimit(&sleeptime,path)) {
	syslog(L_ERROR, "%s can't check rate limit info", ClientHost);
	Reply("%s\r\n", NNTP_CANTPOST);
        UnlockPostRec(path);
	return;
      } else if (sleeptime != 0L) {
        syslog(L_NOTICE,"%s post sleep time is now %ld", ClientHost, sleeptime);
        sleep(sleeptime);
      }
      
      /* Remove the lock here so that only one nnrpd process does the
       * backoff sleep at once. Other procs are sleeping for the lock.
       */
      UnlockPostRec(path);

    } /* end backoff code */

    /* Start at beginning of buffer. */
    if (article == NULL) {
	size = 4096;
	article = NEW(char, size);
    }
    p = article;
    end = &article[size];

    Reply("%d Ok\r\n", NNTP_START_POST_VAL);
    (void)fflush(stdout);

    for (l = 0, longline = 0; ; l++) {
	/* Need more room? */
	if (end - p < ART_LINE_MALLOC) {
	    i = p - article;
	    size += ART_LINE_MALLOC;
	    RENEW(article, char, size);
	    end = &article[size];
	    p = i + article;
	}

	/* Read line, process bad cases. */
	switch (r = READline(p, ART_LINE_LENGTH, DEFAULT_TIMEOUT)) {
	default:
	    syslog(L_ERROR, "%s internal %d in post", ClientHost, r);
	    /* FALLTHROUGH */
	case RTtimeout:
	    syslog(L_ERROR, "%s timeout in post", ClientHost);
	    Printf("%d timeout after %d seconds, closing connection\r\n",
		   NNTP_TEMPERR_VAL, DEFAULT_TIMEOUT);
	    ExitWithStats(1);
	    /* NOTREACHED */
	case RTeof:
	    syslog(L_ERROR, "%s eof in post", ClientHost);
	    ExitWithStats(1);
	    /* NOTREACHED */
	case RTlong:
	    if (longline == 0)
		longline = l + 1;
	    continue;
	case RTok:
	    break;
	}

	/* Process normal text. */
	if (*p != '.') {
	    p += strlen(p);
	    *p++ = '\n';
	    *p = '\0';
	    continue;
	}

	/* Got a leading period; see if it's the terminator. */
	if (p[1] == '\0') {
	    *p = '\0';
	    break;
	}

	/* "Arnold, please copy down over the period for me." */
	while ((p[0] = p[1]) != '\0')
	    p++;
	*p++ = '\n';
	*p = '\0';
    }

    if (longline) {
	syslog(L_NOTICE, "%s toolong in post", ClientHost);
	Printf("%d Line %d too long\r\n", NNTP_POSTFAIL_VAL, longline);
	POSTrejected++;
	return;
    }

    /* Send the article to the server. */
    response = ARTpost(article, idbuff);
    if (response == NULL) {
	syslog(L_NOTICE, "%s post ok %s", ClientHost, idbuff);
	Reply("%s\r\n", NNTP_POSTEDOK);
	POSTreceived++;
    }
    else {
	if ((p = strchr(response, '\r')) != NULL)
	    *p = '\0';
	if ((p = strchr(response, '\n')) != NULL)
	    *p = '\0';
	syslog(L_NOTICE, "%s post failed %s", ClientHost, response);
	Reply("%d %s\r\n", NNTP_POSTFAIL_VAL, response);
	POSTrejected++;
    }
}


/*
**  The "xpath" command.  An uncommon extension.
*/
/* ARGSUSED */
FUNCTYPE
CMDxpath(ac, av)
    int		ac;
    char	*av[];
{
    char	*p;
    HASH	hash = HashMessageID(av[1]);

    if ((p = HISgetent(&hash, TRUE, NULL)) == NULL)
	Reply("%d Don't have it\r\n", NNTP_DONTHAVEIT_VAL);
    else
	Reply("%d %s\r\n", NNTP_NOTHING_FOLLOWS_VAL, p);
}
