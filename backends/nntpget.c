/*  $Id$
**
**  Connect to a remote site, and get news from it to offer to our local
**  server.  Read list on stdin, or get it via NEWNEWS command.  Writes
**  list of articles still needed to stdout.
*/
#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include "portable/time.h"
#include <errno.h>
#include <syslog.h>  
#include <sys/stat.h>
#include <sys/uio.h>

/* Needed on AIX 4.1 to get fd_set and friends. */
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#include "libinn.h"
#include "macros.h"
#include "nntp.h"
#include "paths.h"
#include "inn/history.h"

/*
**  All information about a site we are connected to.
*/
typedef struct _SITE {
    char	*Name;
    int		Rfd;
    int		Wfd;
    char	Buffer[BUFSIZ];
    char	*bp;
    int		Count;
} SITE;


/*
**  Global variables.
*/
static struct iovec	SITEvec[2];
static char		SITEv1[] = "\r\n";
static char		READER[] = "mode reader";
static unsigned long	STATgot;
static unsigned long	STAToffered;
static unsigned long	STATsent;
static unsigned long	STATrejected;
static struct history	*History;



/*
**  Read a line of input, with timeout.
*/
static bool
SITEread(sp, start)
    SITE		*sp;
    char		*start;
{
    register char	*p;
    register char	*end;
    struct timeval	t;
    fd_set		rmask;
    int			i;
    char		c;

    for (p = start, end = &start[NNTP_STRLEN - 1]; ; ) {
	if (sp->Count == 0) {
	    /* Fill the buffer. */
    Again:
	    FD_ZERO(&rmask);
	    FD_SET(sp->Rfd, &rmask);
	    t.tv_sec = DEFAULT_TIMEOUT;
	    t.tv_usec = 0;
	    i = select(sp->Rfd + 1, &rmask, NULL, NULL, &t);
	    if (i < 0) {
		if (errno == EINTR)
		    goto Again;
		return FALSE;
	    }
	    if (i == 0
	     || !FD_ISSET(sp->Rfd, &rmask)
	     || (sp->Count = read(sp->Rfd, sp->Buffer, sizeof sp->Buffer)) < 0)
		return FALSE;
	    if (sp->Count == 0)
		return FALSE;
	    sp->bp = sp->Buffer;
	}

	/* Process next character. */
	sp->Count--;
	c = *sp->bp++;
	if (c == '\n')
	    break;
	if (p < end)
	    *p++ = c;
    }

    /* If last two characters are \r\n, kill the \r as well as the \n. */
    if (p > start && p < end && p[-1] == '\r')
	p--;
    *p = '\0';
    return TRUE;
}


/*
**  Send a line to the server, adding \r\n.  Don't need to do dot-escape
**  since it's only for sending DATA to local site, and the data we got from
**  the remote site already is escaped.
*/
static bool
SITEwrite(sp, p, i)
    SITE		*sp;
    char		*p;
    int			i;
{
    SITEvec[0].iov_base = p;
    SITEvec[0].iov_len = i;
    return xwritev(sp->Wfd, SITEvec, 2) >= 0;
}


static SITE *
SITEconnect(host)
    char	*host;
{
    FILE	*From;
    FILE	*To;
    SITE	*sp;
    int		i;

    /* Connect and identify ourselves. */
    if (host)
	i = NNTPconnect(host, NNTP_PORT, &From, &To, (char *)NULL);
    else {
	host = innconf->server;
        if (host == NULL) {
            fprintf(stderr,
                    "No server specified and server not set in inn.conf\n");
            exit(1);
        }
	i = NNTPlocalopen(&From, &To, (char *)NULL);
    }
    if (i < 0) {
	(void)fprintf(stderr, "Can't connect to \"%s\", %s\n",
		host, strerror(errno));
	exit(1);
    }

    if (NNTPsendpassword(host, From, To) < 0) {
	(void)fprintf(stderr, "Can't authenticate with %s, %s\n",
		host, strerror(errno));
	/* Don't send quit; we want the remote to print a message. */
	exit(1);
    }

    /* Build the structure. */
    sp = NEW(SITE, 1);
    sp->Name = host;
    sp->Rfd = fileno(From);
    sp->Wfd = fileno(To);
    sp->bp = sp->Buffer;
    sp->Count = 0;
    return sp;
}


/*
**  Send "quit" to a site, and get its reply.
*/
static void
SITEquit(sp)
    SITE	*sp;
{
    char	buff[NNTP_STRLEN];

    (void)SITEwrite(sp, "quit", 4);
    (void)SITEread(sp, buff);
}


static bool
HIShaveit(mesgid)
    char		*mesgid;
{
    return HIScheck(History, mesgid);
}


static void
Usage(char *p)
{
    (void)fprintf(stderr, "Usage error:  %s\n", p);
    (void)fprintf(stderr,
    "Usage:  nntpget [ -d dist -n grps [-f file | -t time -u file]] host\n");
    exit(1);
}


int
main(ac, av)
    int		ac;
    char	*av[];
{
    char	buff[NNTP_STRLEN];
    char	mesgid[NNTP_STRLEN];
    char	tbuff[SMBUF];
    char	temp[BUFSIZ];
    const char	*Groups;
    char	*distributions;
    char	*Since;
    char        *path;
    int		i;
    struct tm	*gt;
    struct stat	Sb;
    SITE	*Remote;
    SITE	*Local;
    FILE	*F;
    bool	Offer;
    bool	Error;
    bool	Verbose = FALSE;
    char	*Update;
    char	*p;

    /* First thing, set up logging and our identity. */
    openlog("nntpget", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);           

    /* Set defaults. */
    distributions = NULL;
    Groups = NULL;
    Since = NULL;
    Offer = FALSE;
    Update = NULL;

    if (ReadInnConf() < 0) exit(1);

    (void)umask(NEWSUMASK);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "d:f:n:t:ovu:")) != EOF)
	switch (i) {
	default:
	    Usage("Bad flag");
	    /* NOTREACHED */
	case 'd':
	    distributions = optarg;
	    break;
	case 'u':
	    Update = optarg;
	    /* FALLTHROUGH */
	case 'f':
	    if (Since)
		Usage("Only one -f -t or -u flag");
	    if (stat(optarg, &Sb) < 0) {
		(void)fprintf(stderr, "Can't stat \"%s\", %s\n",
			optarg, strerror(errno));
		exit(1);
	    }
	    gt = gmtime(&Sb.st_mtime);
	    /* Y2K: NNTP Spec currently allows only two digit years. */
	    (void)sprintf(tbuff, "%02d%02d%02d %02d%02d%02d GMT",
		    gt->tm_year % 100, gt->tm_mon + 1, gt->tm_mday,
		    gt->tm_hour, gt->tm_min, gt->tm_sec);
	    Since = tbuff;
	    break;
	case 'n':
	    Groups = optarg;
	    break;
	case 'o':
	    /* Open the history file. */
            path = concatpath(innconf->pathdb, _PATH_HISTORY);
	    History = HISopen(path, innconf->hismethod, HIS_RDONLY);
	    if (!History) {
		(void)fprintf(stderr, "Can't open history, %s\n",
		    strerror(errno));
		exit(1);
	    }
            free(path);
	    Offer = TRUE;
	    break;
	case 't':
	    if (Since)
		Usage("Only one -t or -f flag");
	    Since = optarg;
	    break;
	case 'v':
	    Verbose = TRUE;
	    break;
	}
    ac -= optind;
    av += optind;
    if (ac != 1)
	Usage("No host given");

    /* Set up the scatter/gather vectors used by SITEwrite. */
    SITEvec[1].iov_base = SITEv1;
    SITEvec[1].iov_len = STRLEN(SITEv1);

    /* Connect to the remote server. */
    if ((Remote = SITEconnect(av[0])) == NULL) {
	(void)fprintf(stderr, "Can't connect to \"%s\", %s\n",
		av[0], strerror(errno));
	exit(1);
    }
    if (!SITEwrite(Remote, READER, (int)STRLEN(READER))
     || !SITEread(Remote, buff)) {
	(void)fprintf(stderr, "Can't start reading, %s\n", strerror(errno));
	exit(1);
    }

    if (Since == NULL) {
	F = stdin;
	temp[0] = '\0';
	if (distributions || Groups)
	    Usage("No -d or -n when reading stdin");
    }
    else {
	/* Ask the server for a list of what's new. */
	if (Groups == NULL)
	    Groups = "*";
	if (distributions)
	    (void)sprintf(buff, "NEWNEWS %s %s <%s>",
		    Groups, Since, distributions);
	else
	    (void)sprintf(buff, "NEWNEWS %s %s", Groups, Since);
	if (!SITEwrite(Remote, buff, (int)strlen(buff))
	 || !SITEread(Remote, buff)) {
	    (void)fprintf(stderr, "Can't start list, %s\n", strerror(errno));
	    exit(1);
	}
	if (buff[0] != NNTP_CLASS_OK) {
	    (void)fprintf(stderr, "Protocol error from \"%s\", got \"%s\"\n",
		    Remote->Name, buff);
	    SITEquit(Remote);
	    exit(1);
	}

	/* Create a temporary file. */
	(void)sprintf(temp, "%s/nntpgetXXXXXX", innconf->pathtmp);
	(void)mktemp(temp);
	if ((F = fopen(temp, "w+")) == NULL) {
	    (void)fprintf(stderr, "Can't open \"%s\", %s\n",
		    temp, strerror(errno));
	    exit(1);
	}

	/* Read and store the Message-ID list. */
	for ( ; ; ) {
	    if (!SITEread(Remote, buff)) {
		(void)fprintf(stderr, "Can't read from \"%s\", %s\n",
			Remote->Name, strerror(errno));
		(void)fclose(F);
		SITEquit(Remote);
		exit(1);
	    }
	    if (EQ(buff, "."))
		break;
	    if (Offer && HIShaveit(buff))
		continue;
	    if (fprintf(F, "%s\n", buff) == EOF || ferror(F)) {
		(void)fprintf(stderr, "Can't write \"%s\", %s\n",
			temp, strerror(errno));
		(void)fclose(F);
		SITEquit(Remote);
		exit(1);
	    }
	}
	if (fflush(F) == EOF) {
	    (void)fprintf(stderr, "Can't flush \"%s\", %s\n",
		    temp, strerror(errno));
	    (void)fclose(F);
	    SITEquit(Remote);
	    exit(1);
	}
	fseeko(F, 0, SEEK_SET);
    }

    if (Offer) {
	/* Connect to the local server. */
	if ((Local = SITEconnect((char *)NULL)) == NULL) {
	    (void)fprintf(stderr, "Can't connect to local server, %s\n", 
		    strerror(errno));
	    (void)fclose(F);
	    exit(1);
	}
    }

    /* Loop through the list of Message-ID's. */
    while (fgets(mesgid, sizeof mesgid, F) != NULL) {
	STATgot++;
	if ((p = strchr(mesgid, '\n')) != NULL)
	    *p = '\0';

	if (Offer) {
	    /* See if the local server wants it. */
	    STAToffered++;
	    (void)sprintf(buff, "ihave %s", mesgid);
	    if (!SITEwrite(Local, buff, (int)strlen(buff))
	     || !SITEread(Local, buff)) {
		(void)fprintf(stderr, "Can't offer \"%s\", %s\n.",
			mesgid, strerror(errno));
		break;
	    }
	    if (atoi(buff) != NNTP_SENDIT_VAL)
		continue;
	}

	/* Try to get the article. */
	(void)sprintf(buff, "article %s", mesgid);
	if (!SITEwrite(Remote, buff, (int)strlen(buff))
	 || !SITEread(Remote, buff)) {
	    (void)fprintf(stderr, "Can't get \"%s\", %s\n",
		    mesgid, strerror(errno));
	    (void)printf("%s\n", mesgid);
	    break;
	}
	if (atoi(buff) != NNTP_ARTICLE_FOLLOWS_VAL) {
          if (Offer) {
              (void)SITEwrite(Local, ".", 1);
              if (!SITEread(Local, buff)) {
                  (void)fprintf(stderr, "No reply after \"%s\", %s\n",
                                mesgid, strerror(errno));
                  break;
              }
          }
          continue;
	}

	if (Verbose)
	    (void)fprintf(stderr, "%s...\n", mesgid);

	/* Read each line in the article and write it. */
	for (Error = FALSE; ; ) {
	    if (!SITEread(Remote, buff)) {
		(void)fprintf(stderr, "Can't read \"%s\" from \"%s\", %s\n",
			mesgid, Remote->Name, strerror(errno));
		Error = TRUE;
		break;
	    }
	    if (Offer) {
		if (!SITEwrite(Local, buff, (int)strlen(buff))) {
		    (void)fprintf(stderr, "Can't send \"%s\", %s\n",
			    mesgid, strerror(errno));
		    Error = TRUE;
		    break;
		}
	    }
	    else
		(void)printf("%s\n", buff);
	    if (EQ(buff, "."))
		break;
	}
	if (Error) {
	    (void)printf("%s\n", mesgid);
	    break;
	}
	STATsent++;

	/* How did the local server respond? */
	if (Offer) {
	    if (!SITEread(Local, buff)) {
		(void)fprintf(stderr, "No reply after \"%s\", %s\n",
			mesgid, strerror(errno));
		(void)printf("%s\n", mesgid);
		break;
	    }
	    i = atoi(buff);
	    if (i == NNTP_TOOKIT_VAL)
		continue;
	    if (i == NNTP_RESENDIT_VAL) {
		(void)printf("%s\n", mesgid);
		break;
	    }
	    (void)fprintf(stderr, "%s to \"%s\"\n", buff, mesgid);
	    STATrejected++;
	}
    }

    /* Write rest of the list, close the input. */
    if (!feof(F))
	while (fgets(mesgid, sizeof mesgid, F) != NULL) {
	    if ((p = strchr(mesgid, '\n')) != NULL)
		*p = '\0';
	    (void)printf("%s\n", mesgid);
	    STATgot++;
	}
    (void)fclose(F);

    /* Remove our temp file. */
    if (temp[0] && unlink(temp) < 0)
	(void)fprintf(stderr, "Can't remove \"%s\", %s\n",
		temp, strerror(errno));

    /* All done. */
    SITEquit(Remote);
    if (Offer)
	SITEquit(Local);

    /* Update timestamp file? */
    if (Update) {
	if ((F = fopen(Update, "w")) == NULL) {
	    (void)fprintf(stderr, "Can't update %s, %s\n",
		    Update, strerror(errno));
	    exit(1);
	}
	(void)fprintf(F, "got %ld offered %ld sent %ld rejected %ld\n",
		STATgot, STAToffered, STATsent, STATrejected); 
	if (ferror(F) || fclose(F) == EOF) {
	    (void)fprintf(stderr, "Can't update %s, %s\n",
		    Update, strerror(errno));
	    exit(1);
	}
    }

    exit(0);
    /* NOTREACHED */
}
