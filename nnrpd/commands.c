/*  $Id$
**
**  Miscellaneous commands.
*/
#include "config.h"
#include "clibrary.h"
#include "portable/wait.h"

#include "nnrpd.h"
#include "ov.h"
#include "inn/innconf.h"
#include "inn/messages.h"
#include "tls.h"

typedef struct {
    char                *name;
    int                 high;
    int                 low;
    int                 count;
} GROUPDATA;


extern const char *NNRPinstance;

/* returns:
	-1 for problem (such as no such authenticator etc.)
	0 for authentication succeeded
	1 for authentication failed
 */

static char *PERMauthstring;

static int
PERMgeneric(char *av[], char *accesslist, size_t size)
{
    char path[BIG_BUFFER], *fields[6], *p;
    int i, pan[2], status;
    pid_t pid;
    struct stat stb;

    av += 2;

    PERMcanread = false;
    PERMcanpost = false;
    PERMaccessconf->locpost = false;
    PERMaccessconf->allowapproved = false;

    if (!*av) {
	Reply("%d no authenticator\r\n", NNTP_SYNTAX_VAL);
	return(-1);
    }

    /* check for ../.  I'd use strstr, but there doesn't appear to
       be any other references for it, and I don't want to break
       portability */
    for (p = av[0]; *p; p++)
	if (strncmp(p, "../", 3) == 0) {
	    Reply("%d ../ in authenticator %s\r\n", NNTP_SYNTAX_VAL, av[0]);
	    return(-1);
	}

    if (strchr(_PATH_AUTHDIR,'/') == NULL)
	snprintf(path, sizeof(path), "%s/%s/%s/%s", innconf->pathbin,
                 _PATH_AUTHDIR, _PATH_AUTHDIR_GENERIC, av[0]);
    else
	snprintf(path, sizeof(path), "%s/%s/%s", _PATH_AUTHDIR,
                 _PATH_AUTHDIR_GENERIC, av[0]);

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

    for (i = 0; (pid = fork()) < 0; i++) {
	if (i == innconf->maxforks) {
	    Reply("%d Can't fork %s\r\n", NNTP_TEMPERR_VAL,
		strerror(errno));
	    syslog(L_FATAL, "cant fork %s %m", av[0]);
	    return -1;
	}
	syslog(L_NOTICE, "cant fork %s -- waiting", av[0]);
	sleep(5);
    }

    /* Run the child, with redirection. */
    if (pid == 0) {
	close(STDERR_FILENO);	/* Close existing stderr */
	close(pan[PIPE_READ]);

	/* stderr goes down the pipe. */
	if (pan[PIPE_WRITE] != STDERR_FILENO) {
	    if ((i = dup2(pan[PIPE_WRITE], STDERR_FILENO)) != STDERR_FILENO) {
		syslog(L_FATAL, "cant dup2 %d to %d got %d %m",
		    pan[PIPE_WRITE], STDERR_FILENO, i);
		_exit(1);
	    }
	    close(pan[PIPE_WRITE]);
	}

	close_on_exec(STDIN_FILENO, false);
	close_on_exec(STDOUT_FILENO, false);
	close_on_exec(STDERR_FILENO, false);

	execv(path, av);
	Reply("%s\r\n", NNTP_BAD_COMMAND);

	syslog(L_FATAL, "cant execv %s %m", path);
	_exit(1);
    }

    close(pan[PIPE_WRITE]);
    i = read(pan[PIPE_READ], path, sizeof(path));

    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return 1;

    if ((p = strchr(path, '\n')) != NULL)
	*p = '\0';

    if (PERMauthstring)
	free(PERMauthstring);

    PERMauthstring = xstrdup(path);

    /*syslog(L_NOTICE, "%s (%ld) returned: %d %s %d\n", av[0], (long) pid, i, path, status);*/
    /* Split "host:permissions:user:pass:groups" into fields. */
    for (fields[0] = path, i = 0, p = path; *p; p++)
	if (*p == ':') {
	    *p = '\0';
	    fields[++i] = p + 1;
	}

    PERMcanread = strchr(fields[1], 'R') != NULL;
    PERMcanpost = strchr(fields[1], 'P') != NULL;
    PERMaccessconf->allowapproved = strchr(fields[1], 'A') != NULL;
    PERMaccessconf->locpost = strchr(fields[1], 'L') != NULL;
    PERMaccessconf->allowihave = strchr(fields[1], 'I') != NULL;
    if (strchr(fields[1], 'N') != NULL) PERMaccessconf->allownewnews = true;
    snprintf(PERMuser, sizeof(PERMuser), "%s@%s", fields[2], fields[0]);
    strlcpy(PERMpass, fields[3], sizeof(PERMpass));
    strlcpy(accesslist, fields[4], size);
    /*strcpy(writeaccess, fields[5]); future work? */

    /*for (i = 0; fields[i] && i < 6; i++)
	printf("fields[%d] = %s\n", i, fields[i]);*/

    return 0;
}

/* ARGSUSED */
void
CMDauthinfo(ac, av)
    int		ac;
    char	*av[];
{
    static char	User[SMBUF];
    static char	Password[SMBUF];
    char	accesslist[BIG_BUFFER];
    char        errorstr[BIG_BUFFER];

    if (strcasecmp(av[1], "generic") == 0) {
	char *logrec = Glom(av);

	strlcpy(PERMuser, "<none>", sizeof(PERMuser));

	switch (PERMgeneric(av, accesslist, sizeof(accesslist))) {
	    case 1:
		PERMspecified = NGgetlist(&PERMreadlist, accesslist);
		PERMpostlist = PERMreadlist;
		syslog(L_NOTICE, "%s auth %s (%s -> %s)", ClientHost, PERMuser,
			logrec, PERMauthstring? PERMauthstring: "" );
		Reply("%d Authentication succeeded\r\n", NNTP_AUTH_OK_VAL);
		PERMneedauth = false;
		PERMauthorized = true;
		free(logrec);
		return;
	    case 0:
		syslog(L_NOTICE, "%s bad_auth %s (%s)", ClientHost, PERMuser,
			logrec);
		Reply("%d Authentication failed\r\n", NNTP_ACCESS_VAL);
		free(logrec);
		ExitWithStats(1, false);
	    default:
		/* lower level has issued Reply */
		return;
	}

#ifdef HAVE_SASL
    } else if (strcasecmp(av[1], "sasl") == 0) {
	SASLauth(ac, av);
#endif /* HAVE_SASL */

    } else {

	if (strcasecmp(av[1], "simple") == 0) {
	    if (ac != 4) {
		Reply("%d AUTHINFO SIMPLE <USER> <PASS>\r\n", NNTP_BAD_COMMAND_VAL);
		return;
	    }
	    strlcpy(User, av[2], sizeof(User));
	    strlcpy(Password, av[3], sizeof(Password));
	} else {
	    if (strcasecmp(av[1], "user") == 0) {
		strlcpy(User, av[2], sizeof(User));
		Reply("%d PASS required\r\n", NNTP_AUTH_NEXT_VAL);
		return;
	    }

	    if (strcasecmp(av[1], "pass") != 0) {
		Reply("%d bad authinfo param\r\n", NNTP_BAD_COMMAND_VAL);
		return;
	    }
	    if (User[0] == '\0') {
		Reply("%d USER required\r\n", NNTP_AUTH_REJECT_VAL);
		return;
	    }

	    strlcpy(Password, av[2], sizeof(Password));
	}

        if (strcmp(User, PERMuser) == 0 && strcmp(Password, PERMpass) == 0) {
            syslog(L_NOTICE, "%s user %s", ClientHost, PERMuser);
            if (LLOGenable) {
                fprintf(locallog, "%s user (%s):%s\n", ClientHost, Username, PERMuser);
                fflush(locallog);
            }
            Reply("%d Ok\r\n", NNTP_AUTH_OK_VAL);
            PERMneedauth = false;
            PERMauthorized = true;
            return;
        }
        
        errorstr[0] = '\0';
        
        PERMlogin(User, Password, errorstr);
        PERMgetpermissions();
        if (!PERMneedauth) {
            syslog(L_NOTICE, "%s user %s", ClientHost, PERMuser);
            if (LLOGenable) {
                fprintf(locallog, "%s user (%s):%s\n", ClientHost, Username, PERMuser);
                fflush(locallog);
            }
            Reply("%d Ok\r\n", NNTP_AUTH_OK_VAL);
            PERMneedauth = false;
            PERMauthorized = true;
            return;
        }

	syslog(L_NOTICE, "%s bad_auth", ClientHost);
        if (errorstr[0] != '\0') {
            syslog(L_NOTICE, "%s script error str: %s", ClientHost, errorstr);
            Reply("%d %s\r\n", NNTP_ACCESS_VAL, errorstr);
        } else {
            Reply("%d Authentication error\r\n", NNTP_ACCESS_VAL);
        }
	ExitWithStats(1, false);
    }

}


/*
**  The "DATE" command.  Part of NNTPv2.
*/
/* ARGSUSED0 */
void
CMDdate(ac, av)
    int		ac UNUSED;
    char	*av[] UNUSED;
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
**  Handle the "mode" command.
*/
/* ARGSUSED */
void
CMDmode(ac, av)
    int		ac UNUSED;
    char	*av[];
{
    if (strcasecmp(av[1], "reader") == 0)
	Reply("%d %s InterNetNews NNRP server %s ready (%s).\r\n",
	       PERMcanpost ? NNTP_POSTOK_VAL : NNTP_NOPOSTOK_VAL,
               PERMaccessconf->pathhost, inn_version_string,
	       PERMcanpost ? "posting ok" : "no posting");
    else
	Reply("%d What?\r\n", NNTP_SYNTAX_VAL);
}

static int GroupCompare(const void *a1, const void* b1) {
    const GROUPDATA     *a = a1;
    const GROUPDATA     *b = b1;

    return strcmp(a->name, b->name);
}

/*
**  Display new newsgroups since a given date and time for specified
**  <distributions>.
*/
void CMDnewgroups(int ac, char *av[])
{
    char	        *p;
    char	        *q;
    QIOSTATE	        *qp;
    time_t		date;
    char		*grplist[2];
    int                 hi, lo, count, flag;
    GROUPDATA           *grouplist = NULL;
    GROUPDATA           key;
    GROUPDATA           *gd;
    int                 listsize = 0;
    int                 numgroups = 0;
    int                 numfound = 0;
    int                 i;
    bool                local;

    /* Parse the date. */
    local = !(ac > 3 && strcasecmp(av[3], "GMT") == 0);
    date = parsedate_nntp(av[1], av[2], local);
    if (date == (time_t) -1) {
        Reply("%d Bad date\r\n", NNTP_SYNTAX_VAL);
        return;
    }

    /* Log an error if active.times doesn't exist, but don't return an error
       to the client.  The most likely cause of this is a new server
       installation that's yet to have any new groups created, and returning
       an error was causing needless confusion.  Just return the empty list
       of groups. */
    if ((qp = QIOopen(ACTIVETIMES)) == NULL) {
	syslog(L_ERROR, "%s cant fopen %s %m", ClientHost, ACTIVETIMES);
	Reply("%d New newsgroups follow.\r\n", NNTP_NEWGROUPS_FOLLOWS_VAL);
        Printf(".\r\n");
	return;
    }

    /* Read the file, ignoring long lines. */
    while ((p = QIOread(qp)) != NULL) {
	if ((q = strchr(p, ' ')) == NULL)
	    continue;
	*q++ = '\0';
        if ((time_t) atol(q) < date)
            continue;
	if (!OVgroupstats(p, &lo, &hi, &count, &flag))
	    continue;

	if (PERMspecified) {
	    grplist[0] = p;
	    grplist[1] = NULL;
	    if (!PERMmatch(PERMreadlist, grplist))
		continue;
	}
	else 
	    continue;

	if (grouplist == NULL) {
	    grouplist = xmalloc(1000 * sizeof(GROUPDATA));
	    listsize = 1000;
	}
	if (listsize <= numgroups) {
	    listsize += 1000;
            grouplist = xrealloc(grouplist, listsize * sizeof(GROUPDATA));
	}

	grouplist[numgroups].high = hi;
	grouplist[numgroups].low = lo;
	grouplist[numgroups].count = count;
	grouplist[numgroups].name = xstrdup(p);
	numgroups++;
    }
    QIOclose(qp);

    if ((qp = QIOopen(ACTIVE)) == NULL) {
	syslog(L_ERROR, "%s cant fopen %s %m", ClientHost, ACTIVE);
	Reply("%d Cannot open active file.\r\n", NNTP_TEMPERR_VAL);
	return;
    }
    qsort(grouplist, numgroups, sizeof(GROUPDATA), GroupCompare);
    Reply("%d New newsgroups follow.\r\n", NNTP_NEWGROUPS_FOLLOWS_VAL);
    for (numfound = numgroups; (p = QIOread(qp)) && numfound;) {
	if ((q = strchr(p, ' ')) == NULL)
	    continue;
	*q++ = '\0';
	if ((q = strchr(q, ' ')) == NULL)
	    continue;
	q++;
	if ((q = strchr(q, ' ')) == NULL)
	    continue;
	q++;
	key.name = p;
	if ((gd = bsearch(&key, grouplist, numgroups, sizeof(GROUPDATA), GroupCompare)) == NULL)
	    continue;
	Printf("%s %d %d %s\r\n", p, gd->high, gd->low, q);
	numfound--;
    }
    for (i = 0; i < numgroups; i++) {
	free(grouplist[i].name);
    }
    free(grouplist);
    QIOclose(qp);
    Printf(".\r\n");
}


/*
**  Post an article.
*/
/* ARGSUSED */
void
CMDpost(int ac UNUSED, char *av[] UNUSED)
{
    static char	*article;
    static int	size;
    char	*p, *q;
    char	*end;
    int		longline;
    READTYPE	r;
    int		i;
    long	l;
    long	sleeptime;
    char	*path;
    const char *response;
    char	idbuff[SMBUF];
    static int	backoff_inited = false;
    bool	ihave, permanent;

    ihave = (strcasecmp(av[0], "ihave") == 0);
    if (ihave && (!PERMaccessconf->allowihave || !PERMcanpost)) {
	syslog(L_NOTICE, "%s noperm ihave without permission", ClientHost);
	Reply("%s\r\n", NNTP_ACCESS);
	return;
    }
    if (!ihave && !PERMcanpost) {
	syslog(L_NOTICE, "%s noperm post without permission", ClientHost);
	Reply("%s\r\n", NNTP_CANTPOST);
	return;
    }

    if (!backoff_inited) {
	/* Exponential posting backoff */
	InitBackoffConstants();
	backoff_inited = true;
    }

    /* Dave's posting limiter - Limit postings to a certain rate
     * And now we support multiprocess rate limits. Questions?
     * Email dave@jetcafe.org.
     */
    if (BACKOFFenabled) {

      /* Acquire lock (this could be in RateLimit but that would
       * invoke the spaghetti factor). 
       */
      if ((path = (char *) PostRecFilename(ClientIpString,PERMuser)) == NULL) {
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
	article = xmalloc(size);
    }
    idbuff[0] = 0;
    if (ihave) {
	Reply(NNTP_SENDIT "\r\n");
    } else {
	if ((p = GenerateMessageID(PERMaccessconf->domain)) != NULL) {
	    if (VirtualPathlen > 0) {
		q = p;
		if ((p = strchr(p, '@')) != NULL) {
		    *p = '\0';
		    snprintf(idbuff, sizeof(idbuff), "%s%s@%s>", q,
                             NNRPinstance, PERMaccessconf->domain);
		}
	    } else {
		strlcpy(idbuff, p, sizeof(idbuff));
	    }
	}
	Reply("%d Ok, recommended ID %s\r\n", NNTP_START_POST_VAL, idbuff);
    }
    fflush(stdout);

    p = article;
    end = &article[size];

    longline = 0;
    for (l = 1; ; l++) {
	size_t len;
	const char *line;

	r = line_read(&NNTPline, PERMaccessconf->clienttimeout, &line, &len);
	switch (r) {
	default:
	    warn("%s internal %d in post", ClientHost, r);
	    /* FALLTHROUGH */
	case RTtimeout:
	    warn("%s timeout in post", ClientHost);
	    ExitWithStats(1, false);
	    /* NOTREACHED */
	case RTeof:
	    warn("%s eof in post", ClientHost);
	    ExitWithStats(1, false);
	    /* NOTREACHED */
	case RTlong:
	    if (longline == 0)
		longline = l;
	    continue;
	case RTok:
	    break;
	}

	/* if its the terminator, break out */
	if (strcmp(line, ".") == 0) {
	    break;
	}

	/* if they broke our line length limit, there's little point
	 * in processing any more of their input */
	if (longline != 0) {
	    continue;
	}

	/* +2 because of the \n\0 we append; note we don't add the 2
	 * when increasing the size of the buffer as ART_LINE_MALLOC
	 * will always be larger than 2 bytes */
	if ((len + 2) > (size_t)(end - p)) {
	    i = p - article;
	    size += len + ART_LINE_MALLOC;
            article = xrealloc(article, size);
	    end = &article[size];
	    p = i + article;
	}

	/* reverse any byte-stuffing */
	if (*line == '.') {
	    ++line;
	    --len;
	}
	memcpy(p, line, len);
	p += len;
	*p++ = '\n';
	*p = '\0';
    }

    if (longline) {
	warn("%s toolong in post", ClientHost);
	Printf("%d Line %d too long\r\n", 
	       ihave ? NNTP_REJECTIT_VAL : NNTP_POSTFAIL_VAL, longline);
	POSTrejected++;
	return;
    }

    /* Send the article to the server. */
    response = ARTpost(article, idbuff, ihave, &permanent);
    if (response == NULL) {
	notice("%s post ok %s", ClientHost, idbuff);
	Reply("%s %s\r\n", ihave ? NNTP_TOOKIT : NNTP_POSTEDOK, idbuff);
	POSTreceived++;
    }
    else {
	if ((p = strchr(response, '\r')) != NULL)
	    *p = '\0';
	if ((p = strchr(response, '\n')) != NULL)
	    *p = '\0';
	notice("%s post failed %s", ClientHost, response);
	if (!ihave || permanent) {
	    /* for permanent errors reject the message */
	    Reply("%d %s\r\n", ihave ? NNTP_REJECTIT_VAL : NNTP_POSTFAIL_VAL,
		  response);
	} else {
	    /* non-permanent errors only have relevance to ihave, for
	     * these we have the error status from the upstream
	     * server to report */
	    Reply("%s\r\n", response);
	}
	POSTrejected++;
    }
}

/*
**  The "xpath" command.  An uncommon extension.
*/
/* ARGSUSED */
void
CMDxpath(ac, av)
    int		ac UNUSED;
    char	*av[] UNUSED;
{
    Reply("%d Syntax error or bad command\r\n", NNTP_BAD_COMMAND_VAL);
}
