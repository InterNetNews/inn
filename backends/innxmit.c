/*  $Id$
**
**  Transmit articles to remote site.
**  Modified for NNTP streaming: 1996-01-03 Jerry Aguirre
*/

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include "portable/time.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/uio.h>

/* Needed on AIX 4.1 to get fd_set and friends. */
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#include "inn/history.h"
#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/qio.h"
#include "inn/wire.h"
#include "libinn.h"
#include "macros.h"
#include "nntp.h"
#include "paths.h"
#include "storage.h"

#define OUTPUT_BUFFER_SIZE	(16 * 1024)

/* Streaming extensions to NNTP.  This extension removes the lock-step
** limitation of conventional NNTP.  Article transfer is several times
** faster.  Negotiated and falls back to old mode if receiver refuses.
*/

/* max number of articles that can be streamed ahead */
#define STNBUF 32

/* Send "takethis" without "check" if this many articles were
** accepted in a row.
*/
#define STNC 16

/* typical number of articles to stream  */
/* must be able to fopen this many articles */
#define STNBUFL (STNBUF/2)

/* number of retries before requeueing to disk */
#define STNRETRY 5

struct stbufs {		/* for each article we are procesing */
	char *st_fname;		/* file name */
	char *st_id;		/* message ID */
	int   st_retry;		/* retry count */
	int   st_age;		/* age count */
	ARTHANDLE *art;		/* arthandle to read article contents */
	int   st_hash;		/* hash value to speed searches */
	long  st_size;		/* article size */
};
static struct stbufs stbuf[STNBUF]; /* we keep track of this many articles */
static int stnq;	/* current number of active entries in stbuf */
static long stnofail;	/* Count of consecutive successful sends */

static int TryStream = true;	/* Should attempt stream negotation? */
static int CanStream = false;	/* Result of stream negotation */
static int DoCheck   = true;	/* Should check before takethis? */
static char modestream[] = "mode stream";
static char modeheadfeed[] = "mode headfeed";
static long retries = 0;
static int logRejects = false ;  /* syslog the 437 responses. */



/*
** Syslog formats - collected together so they remain consistent
*/
static char	STAT1[] =
	"%s stats offered %lu accepted %lu refused %lu rejected %lu missing %lu accsize %.0f rejsize %.0f";
static char	STAT2[] = "%s times user %.3f system %.3f elapsed %.3f";
static char	GOT_BADCOMMAND[] = "%s rejected %s %s";
static char	REJECTED[] = "%s rejected %s (%s) %s";
static char	REJ_STREAM[] = "%s rejected (%s) %s";
static char	CANT_CONNECT[] = "%s connect failed %s";
static char	CANT_AUTHENTICATE[] = "%s authenticate failed %s";
static char	IHAVE_FAIL[] = "%s ihave failed %s";

static char	CANT_FINDIT[] = "%s can't find %s";
static char	CANT_PARSEIT[] = "%s can't parse ID %s";
static char	UNEXPECTED[] = "%s unexpected response code %s";

/*
**  Global variables.
*/
static bool		AlwaysRewrite;
static bool		Debug;
static bool		DoRequeue = true;
static bool		Purging;
static bool		STATprint;
static bool		HeadersFeed;
static char		*BATCHname;
static char		*BATCHtemp;
static char		*REMhost;
static double		STATbegin;
static double		STATend;
static FILE		*BATCHfp;
static int		FromServer;
static int		ToServer;
static struct history	*History;
static QIOSTATE		*BATCHqp;
static sig_atomic_t	GotAlarm;
static sig_atomic_t	GotInterrupt;
static sig_atomic_t	JMPyes;
static jmp_buf		JMPwhere;
static char		*REMbuffer;
static char		*REMbuffptr;
static char		*REMbuffend;
static unsigned long	STATaccepted;
static unsigned long	STAToffered;
static unsigned long	STATrefused;
static unsigned long	STATrejected;
static unsigned long	STATmissing;
static double		STATacceptedsize;
static double		STATrejectedsize;


/* Prototypes. */
static ARTHANDLE *article_open(const char *path, const char *id);
static void article_free(ARTHANDLE *);


/*
**  Return true if the history file has the article expired.
*/
static bool
Expired(char *MessageID) {
    return !HISlookup(History, MessageID, NULL, NULL, NULL, NULL);
}


/*
**  Flush and reset the site's output buffer.  Return false on error.
*/
static bool
REMflush(void)
{
    int		i;

    if (REMbuffptr == REMbuffer) return true; /* nothing buffered */
    i = xwrite(ToServer, REMbuffer, (int)(REMbuffptr - REMbuffer));
    REMbuffptr = REMbuffer;
    return i < 0 ? false : true;
}

/*
**  Return index to entry matching this message ID.  Else return -1.
**  The hash is to speed up the search.
**  the protocol.
*/
static int
stindex(char *MessageID, int hash) {
    int i;

    for (i = 0; i < STNBUF; i++) { /* linear search for ID */
	if ((stbuf[i].st_id) && (stbuf[i].st_id[0])
	 && (stbuf[i].st_hash == hash)) {
	    int n;

	    if (strcasecmp(MessageID, stbuf[i].st_id)) continue;

	    /* left of '@' is case sensitive */
	    for (n = 0; (MessageID[n] != '@') && (MessageID[n] != '\0'); n++) ;
	    if (strncmp(MessageID, stbuf[i].st_id, n)) continue;
	    else break;	/* found a match */
	}
    }
    if (i >= STNBUF) i = -1;  /* no match found ? */
    return (i);
}

/* stidhash(): calculate a hash value for message IDs to speed comparisons */
static int
stidhash(char *MessageID) {
    char	*p;
    int		hash;

    hash = 0;
    for (p = MessageID + 1; *p && (*p != '>'); p++) {
	hash <<= 1;
	if (isascii((int)*p) && isupper((int)*p)) {
	    hash += tolower(*p);
	} else {
	    hash += *p;
	}
    }
    return hash;
}

/* stalloc(): save path, ID, and qp into one of the streaming mode entries */
static int
stalloc(char *Article, char *MessageID, ARTHANDLE *art, int hash) {
    int i;

    for (i = 0; i < STNBUF; i++) {
	if ((!stbuf[i].st_fname) || (stbuf[i].st_fname[0] == '\0')) break;
    }
    if (i >= STNBUF) { /* stnq says not full but can not find unused */
	syslog(L_ERROR, "stalloc: Internal error");
	return (-1);
    }
    if ((int)strlen(Article) >= SPOOLNAMEBUFF) {
	syslog(L_ERROR, "stalloc: filename longer than %d", SPOOLNAMEBUFF);
	return (-1);
    }
    /* allocate buffers on first use.
    ** If filename ever is longer than SPOOLNAMEBUFF then code will abort.
    ** If ID is ever longer than NNTP_STRLEN then other code would break.
    */
    if (!stbuf[i].st_fname) stbuf[i].st_fname = xmalloc(SPOOLNAMEBUFF);
    if (!stbuf[i].st_id) stbuf[i].st_id = xmalloc(NNTP_STRLEN);
    strcpy(stbuf[i].st_fname, Article);
    strcpy(stbuf[i].st_id, MessageID);
    stbuf[i].art = art;
    stbuf[i].st_hash = hash;
    stbuf[i].st_retry = 0;
    stbuf[i].st_age = 0;
    stnq++;
    return i;
}

/* strel(): release for reuse one of the streaming mode entries */
static void
strel(int i) {
	if (stbuf[i].art) {
            article_free(stbuf[i].art);
	    stbuf[i].art = NULL;
	}
	if (stbuf[i].st_id) stbuf[i].st_id[0] = '\0';
	if (stbuf[i].st_fname) stbuf[i].st_fname[0] = '\0';
	stnq--;
}

/*
**  Send a line to the server, adding the dot escape and \r\n.
*/
static bool
REMwrite(char *p, int i, bool escdot) {
    int	size;

    /* Buffer too full? */
    if (REMbuffend - REMbuffptr < i + 3) {
	if (!REMflush())
	    return false;
	if (REMbuffend - REMbuffer < i + 3) {
	    /* Line too long -- grow buffer. */
	    size = i * 2;
            REMbuffer = xrealloc(REMbuffer, size);
	    REMbuffend = &REMbuffer[size];
	}
    }

    /* Dot escape, text of the line, line terminator. */
    if (escdot && (*p == '.'))
	*REMbuffptr++ = '.';
    memcpy(REMbuffptr, p, i);
    REMbuffptr += i;
    *REMbuffptr++ = '\r';
    *REMbuffptr++ = '\n';

    return true;
}


/*
**  Print transfer statistics, clean up, and exit.
*/
static void
ExitWithStats(int x)
{
    static char		QUIT[] = "quit";
    TIMEINFO		Now;
    double		usertime;
    double		systime;

    if (!Purging) {
	REMwrite(QUIT, STRLEN(QUIT), false);
	REMflush();
    }
    GetTimeInfo(&Now);
    STATend = TIMEINFOasDOUBLE(Now);
    if (GetResourceUsage(&usertime, &systime) < 0) {
	usertime = 0;
	systime = 0;
    }

    if (STATprint) {
	printf(STAT1, REMhost, STAToffered, STATaccepted, STATrefused,
		STATrejected, STATmissing, STATacceptedsize, STATrejectedsize);
	printf("\n");
	printf(STAT2, REMhost, usertime, systime, STATend - STATbegin);
	printf("\n");
    }

    syslog(L_NOTICE, STAT1, REMhost, STAToffered, STATaccepted, STATrefused,
		STATrejected, STATmissing, STATacceptedsize, STATrejectedsize);
    syslog(L_NOTICE, STAT2, REMhost, usertime, systime, STATend - STATbegin);
    if (retries)
	syslog(L_NOTICE, "%s %lu Streaming retries", REMhost, retries);

    if (BATCHfp != NULL && unlink(BATCHtemp) < 0 && errno != ENOENT)
        syswarn("cannot remove %s", BATCHtemp);
    sleep(1);
    SMshutdown();
    HISclose(History);
    exit(x);
    /* NOTREACHED */
}


/*
**  Close the batchfile and the temporary file, and rename the temporary
**  to be the batchfile.
*/
static void
CloseAndRename(void)
{
    /* Close the files, rename the temporary. */
    if (BATCHqp) {
	QIOclose(BATCHqp);
	BATCHqp = NULL;
    }
    if (ferror(BATCHfp)
     || fflush(BATCHfp) == EOF
     || fclose(BATCHfp) == EOF) {
	unlink(BATCHtemp);
        syswarn("cannot close %s", BATCHtemp);
	ExitWithStats(1);
    }
    if (rename(BATCHtemp, BATCHname) < 0) {
        syswarn("cannot rename %s", BATCHtemp);
	ExitWithStats(1);
    }
}


/*
**  Requeue an article, opening the temp file if we have to.  If we get
**  a file write error, exit so that the original input is left alone.
*/
static void
Requeue(const char *Article, const char *MessageID)
{
    int fd;

    /* Temp file already open? */
    if (BATCHfp == NULL) {
        fd = mkstemp(BATCHtemp);
        if (fd < 0) {
            syswarn("cannot create a temporary file");
            ExitWithStats(1);
        }
        BATCHfp = fdopen(fd, "w");
        if (BATCHfp == NULL) {
            syswarn("cannot open %s", BATCHtemp);
            ExitWithStats(1);
        }
    }

    /* Called only to get the file open? */
    if (Article == NULL)
	return;

    if (MessageID != NULL)
	fprintf(BATCHfp, "%s %s\n", Article, MessageID);
    else
	fprintf(BATCHfp, "%s\n", Article);
    if (fflush(BATCHfp) == EOF || ferror(BATCHfp)) {
        syswarn("cannot requeue %s", Article);
	ExitWithStats(1);
    }
}


/*
**  Requeue an article then copy the rest of the batch file out.
*/
static void
RequeueRestAndExit(char *Article, char *MessageID) {
    char	*p;

    if (!AlwaysRewrite
     && STATaccepted == 0 && STATrejected == 0 && STATrefused == 0
     && STATmissing == 0) {
        warn("nothing sent -- leaving batchfile alone");
	ExitWithStats(1);
    }

    warn("rewriting batch file and exiting");
    if (CanStream) {	/* streaming mode has a buffer of articles */
	int i;

	for (i = 0; i < STNBUF; i++) {    /* requeue unacknowledged articles */
	    if ((stbuf[i].st_fname) && (stbuf[i].st_fname[0] != '\0')) {
		if (Debug)
		    fprintf(stderr, "stbuf[%d]= %s, %s\n",
			    i, stbuf[i].st_fname, stbuf[i].st_id);
		Requeue(stbuf[i].st_fname, stbuf[i].st_id);
		if (Article == stbuf[i].st_fname) Article = NULL;
		strel(i); /* release entry */
	    }
	}
    }
    Requeue(Article, MessageID);

    for ( ; BATCHqp; ) {
	if ((p = QIOread(BATCHqp)) == NULL) {
	    if (QIOtoolong(BATCHqp)) {
                warn("skipping long line in %s", BATCHname);
		QIOread(BATCHqp);
		continue;
	    }
	    if (QIOerror(BATCHqp)) {
                syswarn("cannot read %s", BATCHname);
		ExitWithStats(1);
	    }

	    /* Normal EOF. */
	    break;
	}

	if (fprintf(BATCHfp, "%s\n", p) == EOF
	 || ferror(BATCHfp)) {
            syswarn("cannot requeue %s", p);
	    ExitWithStats(1);
	}
    }

    CloseAndRename();
    ExitWithStats(1);
}


/*
**  Clean up the NNTP escapes from a line.
*/
static char *
REMclean(char *buff) {
    char	*p;

    if ((p = strchr(buff, '\r')) != NULL)
	*p = '\0';
    if ((p = strchr(buff, '\n')) != NULL)
	*p = '\0';

    /* The dot-escape is only in text, not command responses. */
    return buff;
}


/*
**  Read a line of input, with timeout.  Also handle \r\n-->\n mapping
**  and the dot escape.  Return true if okay, *or we got interrupted.*
*/
static bool
REMread(char *start, int size) {
    static int		count;
    static char		buffer[BUFSIZ];
    static char		*bp;
    char		*p;
    char		*q;
    char		*end;
    struct timeval	t;
    fd_set		rmask;
    int			i;
    char		c;

    if (!REMflush())
	return false;

    for (p = start, end = &start[size - 1]; ; ) {
	if (count == 0) {
	    /* Fill the buffer. */
    Again:
	    FD_ZERO(&rmask);
	    FD_SET(FromServer, &rmask);
	    t.tv_sec = 10 * 60;
	    t.tv_usec = 0;
	    i = select(FromServer + 1, &rmask, NULL, NULL, &t);
	    if (GotInterrupt)
		return true;
	    if (i < 0) {
		if (errno == EINTR)
		    goto Again;
		return false;
	    }
	    if (i == 0 || !FD_ISSET(FromServer, &rmask))
		return false;
	    count = read(FromServer, buffer, sizeof buffer);
	    if (GotInterrupt)
		return true;
	    if (count <= 0)
		return false;
	    bp = buffer;
	}

	/* Process next character. */
	count--;
	c = *bp++;
	if (c == '\n')
	    break;
	if (p < end)
	    *p++ = c;
    }

    /* We know we got \n; if previous char was \r, turn it into \n. */
    if (p > start && p < end && p[-1] == '\r')
	p[-1] = '\n';
    *p = '\0';

    /* Handle the dot escape. */
    if (*p == '.') {
	if (p[1] == '\n' && p[2] == '\0')
	    /* EOF. */
	    return false;
	for (q = &start[1]; (*p++ = *q++) != '\0'; )
	    continue;
    }
    return true;
}


/*
**  Handle the interrupt.
*/
static void
Interrupted(char *Article, char *MessageID) {
    warn("interrupted");
    RequeueRestAndExit(Article, MessageID);
}


/*
**  Returns the length of the headers.
*/
static int
HeadersLen(ARTHANDLE *art, int *iscmsg) {
    const char	*p;
    char	lastchar = -1;

    /* from nnrpd/article.c ARTsendmmap() */
    for (p = art->data; p < (art->data + art->len); p++) {
	if (*p == '\r')
	    continue;
	if (*p == '\n') {
	    if (lastchar == '\n') {
		if (*(p-1) == '\r')
		    p--;
		break;
	    }
	    if (*(p + 1) == 'C' && caseEQn(p + 1, "Control: ", 9))
		*iscmsg = 1;
	}
	lastchar = *p;
    }
    return (p - art->data);
}


/*
**  Send a whole article to the server.
*/
static bool
REMsendarticle(char *Article, char *MessageID, ARTHANDLE *art) {
    char	buff[NNTP_STRLEN];

    if (!REMflush())
	return false;
    if (HeadersFeed) {
	struct iovec vec[3];
	char buf[20];
	int iscmsg = 0;
	int len = HeadersLen(art, &iscmsg);

	vec[0].iov_base = (char *) art->data;
	vec[0].iov_len = len;
	/* Add 14 bytes, which maybe will be the length of the Bytes header */
	snprintf(buf, sizeof(buf), "Bytes: %d\r\n", art->len + 14);
	vec[1].iov_base = buf;
	vec[1].iov_len = strlen(buf);
	if (iscmsg) {
	    vec[2].iov_base = (char *) art->data + len;
	    vec[2].iov_len = art->len - len;
	} else {
	    vec[2].iov_base = (char *) ".\r\n";
	    vec[2].iov_len = 3;
	}
	if (xwritev(ToServer, vec, 3) < 0)
	    return false;
    } else
	if (xwrite(ToServer, art->data, art->len) < 0)
	    return false;
    if (GotInterrupt)
	Interrupted(Article, MessageID);
    if (Debug) {
	fprintf(stderr, "> [ article %d ]\n", art->len);
	fprintf(stderr, "> .\n");
    }

    if (CanStream) return true;	/* streaming mode does not wait for ACK */

    /* What did the remote site say? */
    if (!REMread(buff, (int)sizeof buff)) {
        syswarn("no reply after sending %s", Article);
	return false;
    }
    if (GotInterrupt)
	Interrupted(Article, MessageID);
    if (Debug)
	fprintf(stderr, "< %s", buff);

    /* Parse the reply. */
    switch (atoi(buff)) {
    default:
        warn("unknown reply after %s -- %s", Article, buff);
	if (DoRequeue)
	    Requeue(Article, MessageID);
	break;
    case NNTP_BAD_COMMAND_VAL:
    case NNTP_SYNTAX_VAL:
    case NNTP_ACCESS_VAL:
	/* The receiving server is likely confused...no point in continuing */
        syslog(L_FATAL, GOT_BADCOMMAND, REMhost, MessageID, REMclean(buff));
        RequeueRestAndExit(Article, MessageID);
        /* NOTREACHED */
    case NNTP_RESENDIT_VAL:
    case NNTP_GOODBYE_VAL:
	Requeue(Article, MessageID);
	break;
    case NNTP_TOOKIT_VAL:
	STATaccepted++;
	STATacceptedsize += (double)art->len;
	break;
    case NNTP_REJECTIT_VAL:
        if (logRejects)
            syslog(L_NOTICE, REJECTED, REMhost,
                   MessageID, Article, REMclean(buff));
	STATrejected++;
	STATrejectedsize += (double)art->len;
	break;
    }

    /* Article sent, or we requeued it. */
    return true;
}


/*
**  Get the Message-ID header from an open article.
*/
static char *
GetMessageID(ARTHANDLE *art) {
    static char	*buff;
    static int	buffsize = 0;
    const char	*p, *q;

    p = wire_findheader(art->data, art->len, "Message-ID");
    if (p == NULL)
	return NULL;
    for (q = p; q < art->data + art->len; q++) {
        if (*q == '\r' || *q == '\n')
            break;
    }
    if (q == art->data + art->len)
	return NULL;
    if (buffsize < q - p) {
	if (buffsize == 0)
	    buff = xmalloc(q - p + 1);
	else
            buff = xrealloc(buff, q - p + 1);
	buffsize = q - p;
    }
    memcpy(buff, p, q - p);
    buff[q - p] = '\0';
    return buff;
}


/*
**  Mark that we got interrupted.
*/
static RETSIGTYPE
CATCHinterrupt(int s) {
    GotInterrupt = true;

    /* Let two interrupts kill us. */
    xsignal(s, SIG_DFL);
}


/*
**  Mark that the alarm went off.
*/
static RETSIGTYPE
CATCHalarm(int s UNUSED)
{
    GotAlarm = true;
    if (JMPyes)
	longjmp(JMPwhere, 1);
}

/* check articles in streaming NNTP mode
** return true on failure.
*/
static bool
check(int i) {
    char	buff[NNTP_STRLEN];

    /* send "check <ID>" to the other system */
    snprintf(buff, sizeof(buff), "check %s", stbuf[i].st_id);
    if (!REMwrite(buff, (int)strlen(buff), false)) {
        syswarn("cannot check article");
	return true;
    }
    STAToffered++;
    if (Debug) {
	if (stbuf[i].st_retry)
	    fprintf(stderr, "> %s (retry %d)\n", buff, stbuf[i].st_retry);
	else
	    fprintf(stderr, "> %s\n", buff);
    }
    if (GotInterrupt)
	Interrupted(stbuf[i].st_fname, stbuf[i].st_id);

    /* That all.  Response is checked later by strlisten() */
    return false;
}

/* Send article in "takethis <id> streaming NNTP mode.
** return true on failure.
*/
static bool
takethis(int i) {
    char	buff[NNTP_STRLEN];

    if (!stbuf[i].art) {
        warn("internal error: null article for %s in takethis",
             stbuf[i].st_fname);
        return true;
    }
    /* send "takethis <ID>" to the other system */
    snprintf(buff, sizeof(buff), "takethis %s", stbuf[i].st_id);
    if (!REMwrite(buff, (int)strlen(buff), false)) {
        syswarn("cannot send takethis");
        return true;
    }
    if (Debug)
        fprintf(stderr, "> %s\n", buff);
    if (GotInterrupt)
        Interrupted((char *)0, (char *)0);
    if (!REMsendarticle(stbuf[i].st_fname, stbuf[i].st_id, stbuf[i].art))
        return true;
    stbuf[i].st_size = stbuf[i].art->len;
    article_free(stbuf[i].art); /* should not need file again */
    stbuf[i].art = 0;		/* so close to free descriptor */
    stbuf[i].st_age = 0;
    /* That all.  Response is checked later by strlisten() */
    return false;
}


/* listen for responses.  Process acknowledgments to remove items from
** the queue.  Also sends the articles on request.  Returns true on error.
** return true on failure.
*/
static bool
strlisten(void)
{
    int		resp;
    int		i;
    char	*id, *p;
    char	buff[NNTP_STRLEN];
    int		hash;

    while(true) {
	if (!REMread(buff, (int)sizeof buff)) {
            syswarn("no reply to check");
	    return true;
	}
	if (GotInterrupt)
	    Interrupted((char *)0, (char *)0);
	if (Debug)
	    fprintf(stderr, "< %s", buff);

	/* Parse the reply. */
	resp =  atoi(buff);
	/* Skip the 1XX informational messages */
	if ((resp >= 100) && (resp < 200)) continue;
	switch (resp) { /* first time is to verify it */
	case NNTP_ERR_GOTID_VAL:
	case NNTP_OK_SENDID_VAL:
	case NNTP_OK_RECID_VAL:
	case NNTP_ERR_FAILID_VAL:
	case NNTP_RESENDID_VAL:
	    if ((id = strchr(buff, '<')) != NULL) {
		p = strchr(id, '>');
		if (p) *(p+1) = '\0';
		hash = stidhash(id);
		i = stindex(id, hash);	/* find table entry */
		if (i < 0) { /* should not happen */
		    syslog(L_NOTICE, CANT_FINDIT, REMhost, REMclean(buff));
		    return (true); /* can't find it! */
		}
	    } else {
		syslog(L_NOTICE, CANT_PARSEIT, REMhost, REMclean(buff));
		return (true);
	    }
	    break;
	case NNTP_GOODBYE_VAL:
	    /* Most likely out of space -- no point in continuing. */
	    syslog(L_NOTICE, IHAVE_FAIL, REMhost, REMclean(buff));
	    return true;
	    /* NOTREACHED */
	default:
	    syslog(L_NOTICE, UNEXPECTED, REMhost, REMclean(buff));
	    if (Debug)
		fprintf(stderr, "Unknown reply \"%s\"",
						    buff);
	    return (true);
	}
	switch (resp) { /* now we take some action */
	case NNTP_RESENDID_VAL:	/* remote wants it later */
	    /* try again now because time has passed */
	    if (stbuf[i].st_retry < STNRETRY) {
		if (check(i)) return true;
		stbuf[i].st_retry++;
		stbuf[i].st_age = 0;
	    } else { /* requeue to disk for later */
		Requeue(stbuf[i].st_fname, stbuf[i].st_id);
		strel(i); /* release entry */
	    }
	    break;
	case NNTP_ERR_GOTID_VAL:	/* remote doesn't want it */
	    strel(i); /* release entry */
	    STATrefused++;
	    stnofail = 0;
	    break;
		
	case NNTP_OK_SENDID_VAL:	/* remote wants article */
	    if (takethis(i)) return true;
	    stnofail++;
	    break;

	case NNTP_OK_RECID_VAL:	/* remote received it OK */
	    STATacceptedsize += (double) stbuf[i].st_size;
	    strel(i); /* release entry */
	    STATaccepted++;
	    break;
		
	case NNTP_ERR_FAILID_VAL:
	    STATrejectedsize += (double) stbuf[i].st_size;
	    if (logRejects)
		syslog(L_NOTICE, REJ_STREAM, REMhost,
		    stbuf[i].st_fname, REMclean(buff));
/* XXXXX Caution THERE BE DRAGONS, I don't think this logs properly
   The message ID is returned in the peer response... so this is redundant
		    stbuf[i].st_id, stbuf[i].st_fname, REMclean(buff)); */
	    strel(i); /* release entry */
	    STATrejected++;
	    stnofail = 0;
	    break;
	}
	break;
    }
    return (false);
}

/*
**  Print a usage message and exit.
*/
static void
Usage(void)
{
    die("Usage: innxmit [-acdHlprs] [-t#] [-T#] host file");
}


/*
**  Open an article.  If the argument is a token, retrieve the article via
**  the storage API.  Otherwise, open the file and fake up an ARTHANDLE for
**  it.  Only fill in those fields that we'll need.  Articles not retrieved
**  via the storage API will have a type of TOKEN_EMPTY.
*/
static ARTHANDLE *
article_open(const char *path, const char *id)
{
    TOKEN token;
    ARTHANDLE *article;
    int fd, length;
    struct stat st;
    char *p;

    if (IsToken(path)) {
        token = TextToToken(path);
        article = SMretrieve(token, RETR_ALL);
        if (article == NULL) {
            if (SMerrno == SMERR_NOENT || SMerrno == SMERR_UNINIT)
                STATmissing++;
            else {
                warn("requeue %s: %s", path, SMerrorstr);
                Requeue(path, id);
            }
        }
        return article;
    } else {
        char *data;
        fd = open(path, O_RDONLY);
        if (fd < 0)
            return NULL;
        if (fstat(fd, &st) < 0) {
            syswarn("requeue %s", path);
            Requeue(path, id);
            return NULL;
        }
        article = xmalloc(sizeof(ARTHANDLE));
        article->type = TOKEN_EMPTY;
        article->len = st.st_size;
        data = xmalloc(article->len);
        if (xread(fd, data, article->len) < 0) {
            syswarn("requeue %s", path);
            free(data);
            free(article);
            close(fd);
            Requeue(path, id);
            return NULL;
        }
        close(fd);
        p = memchr(data, '\n', article->len);
        if (p == NULL || p == data) {
            warn("requeue %s: cannot find headers", path);
            free(data);
            free(article);
            Requeue(path, id);
            return NULL;
        }
        if (p[-1] != '\r') {
            p = ToWireFmt(data, article->len, (size_t *)&length);
            free(data);
            data = p;
            article->len = length;
        }
        article->data = data;
        return article;
    }
}


/*
**  Free an article, using the type field to determine whether to free it
**  via the storage API.
*/
static void
article_free(ARTHANDLE *article)
{
    if (article->type == TOKEN_EMPTY) {
        free((char *)article->data);
        free(article);
    } else
        SMfreearticle(article);
}


int main(int ac, char *av[]) {
    static char		SKIPPING[] = "Skipping \"%s\" --%s?\n";
    int	                i;
    char	        *p;
    ARTHANDLE		*art;
    TIMEINFO		Now;
    FILE		*From;
    FILE		*To;
    char		buff[8192+128];
    char		*Article;
    char		*MessageID;
    RETSIGTYPE		(*old)(int) = NULL;
    unsigned int	ConnectTimeout;
    unsigned int	TotalTimeout;
    int                 port = NNTP_PORT;
    bool		val;
    char                *path;

    openlog("innxmit", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
    message_program_name = "innxmit";

    /* Set defaults. */
    if (!innconf_read(NULL))
        exit(1);

    ConnectTimeout = 0;
    TotalTimeout = 0;
    
    umask(NEWSUMASK);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "lacdHprst:T:vP:")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'P':
	    port = atoi(optarg);
	    break;
	case 'a':
	    AlwaysRewrite = true;
	    break;
	case 'c':
	    DoCheck = false;
	    break;
	case 'd':
	    Debug = true;
	    break;
	case 'H':
	    HeadersFeed = true;
	    break;
        case 'l':
            logRejects = true ;
            break ;
	case 'p':
	    AlwaysRewrite = true;
	    Purging = true;
	    break;
	case 'r':
	    DoRequeue = false;
	    break;
	case 's':
	    TryStream = false;
	    break;
	case 't':
	    ConnectTimeout = atoi(optarg);
	    break;
	case 'T':
	    TotalTimeout = atoi(optarg);
	    break;
	case 'v':
	    STATprint = true;
	    break;
	}
    ac -= optind;
    av += optind;

    /* Parse arguments; host and filename. */
    if (ac != 2)
	Usage();
    REMhost = av[0];
    BATCHname = av[1];

    if (chdir(innconf->patharticles) < 0)
        sysdie("cannot cd to %s", innconf->patharticles);

    val = true;
    if (!SMsetup(SM_PREOPEN,(void *)&val))
        die("cannot set up the storage manager");
    if (!SMinit())
        die("cannot initialize the storage manager: %s", SMerrorstr);

    /* Open the batch file and lock others out. */
    if (BATCHname[0] != '/') {
        BATCHname = concatpath(innconf->pathoutgoing, av[1]);
    }
    if (((i = open(BATCHname, O_RDWR)) < 0) || ((BATCHqp = QIOfdopen(i)) == NULL)) {
        syswarn("cannot open %s", BATCHname);
	SMshutdown();
	exit(1);
    }
    if (!inn_lock_file(QIOfileno(BATCHqp), INN_LOCK_WRITE, true)) {
#if	defined(EWOULDBLOCK)
	if (errno == EWOULDBLOCK) {
	    SMshutdown();
	    exit(0);
	}
#endif	/* defined(EWOULDBLOCK) */
        syswarn("cannot lock %s", BATCHname);
	SMshutdown();
	exit(1);
    }

    /* Get a temporary name in the same directory as the batch file. */
    p = strrchr(BATCHname, '/');
    *p = '\0';
    BATCHtemp = concatpath(BATCHname, "bchXXXXXX");
    *p = '/';

    /* Set up buffer used by REMwrite. */
    REMbuffer = xmalloc(OUTPUT_BUFFER_SIZE);
    REMbuffend = &REMbuffer[OUTPUT_BUFFER_SIZE];
    REMbuffptr = REMbuffer;

    /* Start timing. */
    if (GetTimeInfo(&Now) < 0) {
        syswarn("cannot get time");
	SMshutdown();
	exit(1);
    }
    STATbegin = TIMEINFOasDOUBLE(Now);

    if (!Purging) {
	/* Open a connection to the remote server. */
	if (ConnectTimeout) {
	    GotAlarm = false;
	    old = xsignal(SIGALRM, CATCHalarm);
            if (setjmp(JMPwhere)) {
                warn("cannot connect to %s: timed out", REMhost);
                SMshutdown();
                exit(1);
            }
	    JMPyes = true;
	    alarm(ConnectTimeout);
	}
	if (NNTPconnect(REMhost, port, &From, &To, buff) < 0 || GotAlarm) {
	    i = errno;
            warn("cannot connect to %s: %s", REMhost,
                 buff[0] ? REMclean(buff) : strerror(errno));
	    if (GotAlarm)
		syslog(L_NOTICE, CANT_CONNECT, REMhost, "timeout");
	    else 
		syslog(L_NOTICE, CANT_CONNECT, REMhost,
		    buff[0] ? REMclean(buff) : strerror(i));
	    SMshutdown();
	    exit(1);
	}
	if (Debug)
	    fprintf(stderr, "< %s\n", REMclean(buff));
	if (NNTPsendpassword(REMhost, From, To) < 0 || GotAlarm) {
	    i = errno;
            syswarn("cannot authenticate with %s", REMhost);
	    syslog(L_ERROR, CANT_AUTHENTICATE,
		REMhost, GotAlarm ? "timeout" : strerror(i));
	    /* Don't send quit; we want the remote to print a message. */
	    SMshutdown();
	    exit(1);
	}
	if (ConnectTimeout) {
	    alarm(0);
	    xsignal(SIGALRM, old);
	    JMPyes = false;
	}

	/* We no longer need standard I/O. */
	FromServer = fileno(From);
	ToServer = fileno(To);

	if (TryStream) {
	    if (!REMwrite(modestream, (int)strlen(modestream), false)) {
                syswarn("cannot negotiate %s", modestream);
	    }
	    if (Debug)
		fprintf(stderr, ">%s\n", modestream);
	    /* Does he understand mode stream? */
	    if (!REMread(buff, (int)sizeof buff)) {
                syswarn("no reply to %s", modestream);
	    } else {
		if (Debug)
		    fprintf(stderr, "< %s", buff);

		/* Parse the reply. */
		switch (atoi(buff)) {
		default:
                    warn("unknown reply to %s -- %s", modestream, buff);
		    CanStream = false;
		    break;
		case NNTP_OK_STREAM_VAL:	/* YES! */
		    CanStream = true;
		    break;
                case NNTP_AUTH_NEEDED_VAL: /* authentication refusal */
		case NNTP_BAD_COMMAND_VAL: /* normal refusal */
		    CanStream = false;
		    break;
		}
	    }
	    if (CanStream) {
		for (i = 0; i < STNBUF; i++) { /* reset buffers */
		    stbuf[i].st_fname = 0;
		    stbuf[i].st_id = 0;
		    stbuf[i].art = 0;
		}
		stnq = 0;
	    }
	}
	if (HeadersFeed) {
	    if (!REMwrite(modeheadfeed, strlen(modeheadfeed), false))
                syswarn("cannot negotiate %s", modeheadfeed);
	    if (Debug)
		fprintf(stderr, ">%s\n", modeheadfeed);
	    if (!REMread(buff, sizeof buff)) {
                syswarn("no reply to %s", modeheadfeed);
	    } else {
		if (Debug)
		    fprintf(stderr, "< %s", buff);

		/* Parse the reply. */
		switch (atoi(buff)) {
		case 250:		/* YES! */
		    break;
		case NNTP_BAD_COMMAND_VAL: /* normal refusal */
                    die("%s not allowed -- %s", modeheadfeed, buff);
		default:
                    die("unknown reply to %s -- %s", modeheadfeed, buff);
		}
	    }
	}
    }

    /* Set up signal handlers. */
    xsignal(SIGHUP, CATCHinterrupt);
    xsignal(SIGINT, CATCHinterrupt);
    xsignal(SIGTERM, CATCHinterrupt);
    xsignal(SIGPIPE, SIG_IGN);
    if (TotalTimeout) {
	xsignal(SIGALRM, CATCHalarm);
	alarm(TotalTimeout);
    }

    path = concatpath(innconf->pathdb, _PATH_HISTORY);
    History = HISopen(path, innconf->hismethod, HIS_RDONLY);
    free(path);

    /* Main processing loop. */
    GotInterrupt = false;
    GotAlarm = false;
    for (Article = NULL, MessageID = NULL; ; ) {
	if (GotAlarm) {
            warn("timed out");
	    /* Don't resend the current article. */
	    RequeueRestAndExit((char *)NULL, (char *)NULL);
	}
	if (GotInterrupt)
	    Interrupted(Article, MessageID);

	if ((Article = QIOread(BATCHqp)) == NULL) {
	    if (QIOtoolong(BATCHqp)) {
                warn("skipping long line in %s", BATCHname);
		QIOread(BATCHqp);
		continue;
	    }
	    if (QIOerror(BATCHqp)) {
                syswarn("cannot read %s", BATCHname);
		ExitWithStats(1);
	    }

	    /* Normal EOF -- we're done. */
	    QIOclose(BATCHqp);
	    BATCHqp = NULL;
	    break;
	}

	/* Ignore blank lines. */
	if (*Article == '\0')
	    continue;

	/* Split the line into possibly two fields. */
	if (Article[0] == '/'
	 && Article[strlen(innconf->patharticles)] == '/'
	 && EQn(Article, innconf->patharticles, strlen(innconf->patharticles)))
	    Article += strlen(innconf->patharticles) + 1;
	if ((MessageID = strchr(Article, ' ')) != NULL) {
	    *MessageID++ = '\0';
	    if (*MessageID != '<'
		|| (p = strrchr(MessageID, '>')) == NULL
		|| *++p != '\0') {
                warn("ignoring line %s %s...", Article, MessageID);
		continue;
	    }
	}

	if (*Article == '\0') {
	    if (MessageID)
                warn("empty file name for %s in %s", MessageID, BATCHname);
	    else
                warn("empty file name, no message ID in %s", BATCHname);
	    /* We could do a history lookup. */
	    continue;
	}

	if (Purging && MessageID != NULL && !Expired(MessageID)) {
	    Requeue(Article, MessageID);
	    continue;
	}

        /* Drop articles with a message ID longer than NNTP_MSGID_MAXLEN to
           avoid overrunning buffers and throwing the server on the
           receiving end a blow from behind. */
        if (MessageID != NULL && strlen(MessageID) > NNTP_MSGID_MAXLEN) {
            warn("dropping article in %s: long message ID %s", BATCHname,
                 MessageID);
            continue;
        }

        art = article_open(Article, MessageID);
        if (art == NULL)
	    continue;

	if (Purging) {
            article_free(art);
	    Requeue(Article, MessageID);
	    continue;
	}

	/* Get the Message-ID from the article if we need to. */
	if (MessageID == NULL) {
	    if ((MessageID = GetMessageID(art)) == NULL) {
                warn(SKIPPING, Article, "no message ID");
                article_free(art);
		continue;
	    }
	}
	if (GotInterrupt)
	    Interrupted(Article, MessageID);

	/* Offer the article. */
	if (CanStream) {
	    int lim;
	    int hash;

	    hash = stidhash(MessageID);
	    if (stindex(MessageID, hash) >= 0) { /* skip duplicates in queue */
		if (Debug)
		    fprintf(stderr, "Skipping duplicate ID %s\n",
							    MessageID);
                article_free(art);
		continue;
	    }
	    /* This code tries to optimize by sending a burst of "check"
	     * commands before flushing the buffer.  This should result
	     * in several being sent in one packet reducing the network
	     * overhead.
	     */
	    if (DoCheck && (stnofail < STNC)) lim = STNBUF;
	    else                              lim = STNBUFL;
	    if (stnq >= lim) { /* need to empty a buffer */
		while (stnq >= STNBUFL) { /* or several */
		    if (strlisten()) {
			RequeueRestAndExit(Article, MessageID);
		    }
		}
	    }
	    /* save new article in the buffer */
	    i = stalloc(Article, MessageID, art, hash);
	    if (i < 0) {
                article_free(art);
		RequeueRestAndExit(Article, MessageID);
	    }
	    if (DoCheck && (stnofail < STNC)) {
		if (check(i)) {
		    RequeueRestAndExit((char *)NULL, (char *)NULL);
		}
	    } else {
                STAToffered++ ;
		if (takethis(i)) {
		    RequeueRestAndExit((char *)NULL, (char *)NULL);
		}
	    }
	    /* check for need to resend any IDs */
	    for (i = 0; i < STNBUF; i++) {
		if ((stbuf[i].st_fname) && (stbuf[i].st_fname[0] != '\0')) {
		    if (stbuf[i].st_age++ > stnq) {
			/* This should not happen but just in case ... */
			if (stbuf[i].st_retry < STNRETRY) {
			    if (check(i)) /* resend check */
				RequeueRestAndExit((char *)NULL, (char *)NULL);
			    retries++;
			    stbuf[i].st_retry++;
			    stbuf[i].st_age = 0;
			} else { /* requeue to disk for later */
			    Requeue(stbuf[i].st_fname, stbuf[i].st_id);
			    strel(i); /* release entry */
			}
		    }
		}
	    }
	    continue; /* next article */
	}
	snprintf(buff, sizeof(buff), "ihave %s", MessageID);
	if (!REMwrite(buff, (int)strlen(buff), false)) {
            syswarn("cannot offer article");
            article_free(art);
	    RequeueRestAndExit(Article, MessageID);
	}
	STAToffered++;
	if (Debug)
	    fprintf(stderr, "> %s\n", buff);
	if (GotInterrupt)
	    Interrupted(Article, MessageID);

	/* Does he want it? */
	if (!REMread(buff, (int)sizeof buff)) {
            syswarn("no reply to ihave");
            article_free(art);
	    RequeueRestAndExit(Article, MessageID);
	}
	if (GotInterrupt)
	    Interrupted(Article, MessageID);
	if (Debug)
	    fprintf(stderr, "< %s", buff);

	/* Parse the reply. */
	switch (atoi(buff)) {
	default:
            warn("unknown reply to %s -- %s", Article, buff);
	    if (DoRequeue)
		Requeue(Article, MessageID);
	    break;
        case NNTP_BAD_COMMAND_VAL:
        case NNTP_SYNTAX_VAL:
        case NNTP_ACCESS_VAL:
            /* The receiving server is likely confused...no point in continuing */
            syslog(L_FATAL, GOT_BADCOMMAND, REMhost, MessageID, REMclean(buff));
	    RequeueRestAndExit(Article, MessageID);
	    /* NOTREACHED */
        case NNTP_AUTH_NEEDED_VAL:
	case NNTP_RESENDIT_VAL:
	case NNTP_GOODBYE_VAL:
	    /* Most likely out of space -- no point in continuing. */
	    syslog(L_NOTICE, IHAVE_FAIL, REMhost, REMclean(buff));
	    RequeueRestAndExit(Article, MessageID);
	    /* NOTREACHED */
	case NNTP_SENDIT_VAL:
	    if (!REMsendarticle(Article, MessageID, art))
		RequeueRestAndExit(Article, MessageID);
	    break;
	case NNTP_HAVEIT_VAL:
	    STATrefused++;
	    break;
#if	defined(NNTP_SENDIT_LATER)
	case NNTP_SENDIT_LATER_VAL:
	    Requeue(Article, MessageID);
	    break;
#endif	/* defined(NNTP_SENDIT_LATER) */
	}

        article_free(art);
    }
    if (CanStream) { /* need to wait for rest of ACKs */
	while (stnq > 0) {
	    if (strlisten()) {
		RequeueRestAndExit((char *)NULL, (char *)NULL);
	    }
	}
    }

    if (BATCHfp != NULL)
	/* We requeued something, so close the temp file. */
	CloseAndRename();
    else if (unlink(BATCHname) < 0 && errno != ENOENT)
        syswarn("cannot remove %s", BATCHtemp);
    ExitWithStats(0);
    /* NOTREACHED */
    return 0;
}
