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
#include <sys/stat.h>
#include <sys/uio.h>

/* Needed on AIX 4.1 to get fd_set and friends. */
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#include "inn/history.h"
#include "inn/innconf.h"
#include "inn/messages.h"
#include "libinn.h"
#include "nntp.h"
#include "paths.h"

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
SITEread(SITE *sp, char *start)
{
    char	*p;
    char	*end;
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
		return false;
	    }
	    if (i == 0
	     || !FD_ISSET(sp->Rfd, &rmask)
	     || (sp->Count = read(sp->Rfd, sp->Buffer, sizeof sp->Buffer)) < 0)
		return false;
	    if (sp->Count == 0)
		return false;
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
    return true;
}


/*
**  Send a line to the server, adding \r\n.  Don't need to do dot-escape
**  since it's only for sending DATA to local site, and the data we got from
**  the remote site already is escaped.
*/
static bool
SITEwrite(SITE *sp, const char *p, int i)
{
    SITEvec[0].iov_base = (char *) p;
    SITEvec[0].iov_len = i;
    return xwritev(sp->Wfd, SITEvec, 2) >= 0;
}


static SITE *
SITEconnect(char *host)
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
        if (host == NULL)
            die("no server specified and server not set in inn.conf");
	i = NNTPlocalopen(&From, &To, (char *)NULL);
    }
    if (i < 0)
        sysdie("cannot connect to %s", host);

    if (NNTPsendpassword(host, From, To) < 0)
        sysdie("cannot authenticate to %s", host);

    /* Build the structure. */
    sp = xmalloc(sizeof(SITE));
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
SITEquit(SITE *sp)
{
    char	buff[NNTP_STRLEN];

    SITEwrite(sp, "quit", 4);
    SITEread(sp, buff);
}


static bool
HIShaveit(char *mesgid)
{
    return HIScheck(History, mesgid);
}


static void
Usage(const char *p)
{
    warn("%s", p);
    fprintf(stderr, "Usage: nntpget"
            " [ -d dist -n grps [-f file | -t time -u file]] host\n");
    exit(1);
}


int
main(int ac, char *av[])
{
    char	buff[NNTP_STRLEN];
    char	mesgid[NNTP_STRLEN];
    char	tbuff[SMBUF];
    char	*msgidfile = NULL;
    int         msgidfd;
    const char	*Groups;
    char	*distributions;
    char	*Since;
    char        *path;
    int		i;
    struct tm	*gt;
    struct stat	Sb;
    SITE	*Remote;
    SITE	*Local = NULL;
    FILE	*F;
    bool	Offer;
    bool	Error;
    bool	Verbose = false;
    char	*Update;
    char	*p;

    /* First thing, set up our identity. */
    message_program_name = "nntpget";

    /* Set defaults. */
    distributions = NULL;
    Groups = NULL;
    Since = NULL;
    Offer = false;
    Update = NULL;
    if (!innconf_read(NULL))
        exit(1);

    umask(NEWSUMASK);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "d:f:n:t:ovu:")) != EOF)
	switch (i) {
	default:
	    Usage("bad flag");
	    /* NOTREACHED */
	case 'd':
	    distributions = optarg;
	    break;
	case 'u':
	    Update = optarg;
	    /* FALLTHROUGH */
	case 'f':
	    if (Since)
		Usage("only one of -f, -t, or -u may be given");
	    if (stat(optarg, &Sb) < 0)
                sysdie("cannot stat %s", optarg);
	    gt = gmtime(&Sb.st_mtime);
	    /* Y2K: NNTP Spec currently allows only two digit years. */
	    snprintf(tbuff, sizeof(tbuff), "%02d%02d%02d %02d%02d%02d GMT",
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
	    if (!History)
                sysdie("cannot open history");
            free(path);
	    Offer = true;
	    break;
	case 't':
	    if (Since)
		Usage("only one of -t or -f may be given");
	    Since = optarg;
	    break;
	case 'v':
	    Verbose = true;
	    break;
	}
    ac -= optind;
    av += optind;
    if (ac != 1)
	Usage("no host given");

    /* Set up the scatter/gather vectors used by SITEwrite. */
    SITEvec[1].iov_base = SITEv1;
    SITEvec[1].iov_len = strlen(SITEv1);

    /* Connect to the remote server. */
    if ((Remote = SITEconnect(av[0])) == NULL)
        sysdie("cannot connect to %s", av[0]);
    if (!SITEwrite(Remote, READER, (int)strlen(READER))
     || !SITEread(Remote, buff))
        sysdie("cannot start reading");

    if (Since == NULL) {
	F = stdin;
	if (distributions || Groups)
	    Usage("no -d or -n flags allowed when reading stdin");
    }
    else {
	/* Ask the server for a list of what's new. */
	if (Groups == NULL)
	    Groups = "*";
	if (distributions)
	    snprintf(buff, sizeof(buff), "NEWNEWS %s %s <%s>",
                     Groups, Since, distributions);
	else
	    snprintf(buff, sizeof(buff), "NEWNEWS %s %s", Groups, Since);
	if (!SITEwrite(Remote, buff, (int)strlen(buff))
	 || !SITEread(Remote, buff))
            sysdie("cannot start list");
	if (buff[0] != NNTP_CLASS_OK) {
	    SITEquit(Remote);
            die("protocol error from %s, got %s", Remote->Name, buff);
	}

        /* Create a temporary file. */
        msgidfile = concatpath(innconf->pathtmp, "nntpgetXXXXXX");
        msgidfd = mkstemp(msgidfile);
        if (msgidfd < 0)
            sysdie("cannot create a temporary file");
        F = fopen(msgidfile, "w+");
        if (F == NULL)
            sysdie("cannot open %s", msgidfile);

	/* Read and store the Message-ID list. */
	for ( ; ; ) {
	    if (!SITEread(Remote, buff)) {
                syswarn("cannot read from %s", Remote->Name);
		fclose(F);
		SITEquit(Remote);
		exit(1);
	    }
	    if (strcmp(buff, ".") == 0)
		break;
	    if (Offer && HIShaveit(buff))
		continue;
	    if (fprintf(F, "%s\n", buff) == EOF || ferror(F)) {
                syswarn("cannot write %s", msgidfile);
		fclose(F);
		SITEquit(Remote);
		exit(1);
	    }
	}
	if (fflush(F) == EOF) {
            syswarn("cannot flush %s", msgidfile);
	    fclose(F);
	    SITEquit(Remote);
	    exit(1);
	}
	fseeko(F, 0, SEEK_SET);
    }

    if (Offer) {
	/* Connect to the local server. */
	if ((Local = SITEconnect((char *)NULL)) == NULL) {
            syswarn("cannot connect to local server");
	    fclose(F);
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
	    snprintf(buff, sizeof(buff), "ihave %s", mesgid);
	    if (!SITEwrite(Local, buff, (int)strlen(buff))
	     || !SITEread(Local, buff)) {
                syswarn("cannot offer %s", mesgid);
		break;
	    }
	    if (atoi(buff) != NNTP_SENDIT_VAL)
		continue;
	}

	/* Try to get the article. */
	snprintf(buff, sizeof(buff), "article %s", mesgid);
	if (!SITEwrite(Remote, buff, (int)strlen(buff))
	 || !SITEread(Remote, buff)) {
            syswarn("cannot get %s", mesgid);
	    printf("%s\n", mesgid);
	    break;
	}
	if (atoi(buff) != NNTP_ARTICLE_FOLLOWS_VAL) {
          if (Offer) {
              SITEwrite(Local, ".", 1);
              if (!SITEread(Local, buff)) {
                  syswarn("no reply after %s", mesgid);
                  break;
              }
          }
          continue;
	}

	if (Verbose)
            notice("%s...", mesgid);

	/* Read each line in the article and write it. */
	for (Error = false; ; ) {
	    if (!SITEread(Remote, buff)) {
                syswarn("cannot read %s from %s", mesgid, Remote->Name);
		Error = true;
		break;
	    }
	    if (Offer) {
		if (!SITEwrite(Local, buff, (int)strlen(buff))) {
                    syswarn("cannot send %s", mesgid);
		    Error = true;
		    break;
		}
	    }
	    else
		printf("%s\n", buff);
	    if (strcmp(buff, ".") == 0)
		break;
	}
	if (Error) {
	    printf("%s\n", mesgid);
	    break;
	}
	STATsent++;

	/* How did the local server respond? */
	if (Offer) {
	    if (!SITEread(Local, buff)) {
                syswarn("no reply after %s", mesgid);
		printf("%s\n", mesgid);
		break;
	    }
	    i = atoi(buff);
	    if (i == NNTP_TOOKIT_VAL)
		continue;
	    if (i == NNTP_RESENDIT_VAL) {
		printf("%s\n", mesgid);
		break;
	    }
            syswarn("%s to %s", buff, mesgid);
	    STATrejected++;
	}
    }

    /* Write rest of the list, close the input. */
    if (!feof(F))
	while (fgets(mesgid, sizeof mesgid, F) != NULL) {
	    if ((p = strchr(mesgid, '\n')) != NULL)
		*p = '\0';
	    printf("%s\n", mesgid);
	    STATgot++;
	}
    fclose(F);

    /* Remove our temp file. */
    if (msgidfile && unlink(msgidfile) < 0)
        syswarn("cannot remove %s", msgidfile);

    /* All done. */
    SITEquit(Remote);
    if (Offer)
	SITEquit(Local);

    /* Update timestamp file? */
    if (Update) {
	if ((F = fopen(Update, "w")) == NULL)
            sysdie("cannot update %s", Update);
	fprintf(F, "got %ld offered %ld sent %ld rejected %ld\n",
		STATgot, STAToffered, STATsent, STATrejected); 
	if (ferror(F) || fclose(F) == EOF)
            sysdie("cannot update %s", Update);
    }

    exit(0);
    /* NOTREACHED */
}
