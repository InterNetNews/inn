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

#include "inn/qio.h"
#include "libinn.h"
#include "macros.h"
#include "nntp.h"
#include "paths.h"
#include "storage.h"
#include "inn/history.h"

/* Needed on AIX 4.1 to get fd_set and friends. */
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

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

static int TryStream = TRUE;	/* Should attempt stream negotation? */
static int CanStream = FALSE;	/* Result of stream negotation */
static int DoCheck   = TRUE;	/* Should check before takethis? */
static char modestream[] = "mode stream";
static char modeheadfeed[] = "mode headfeed";
static long retries = 0;
static int logRejects = FALSE ;  /* syslog the 437 responses. */



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
static bool		DoRequeue = TRUE;
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
**  Return TRUE if the history file has the article expired.
*/
static bool
Expired(char *MessageID) {
    return !HISlookup(History, MessageID, NULL, NULL, NULL, NULL);
}


/*
**  Flush and reset the site's output buffer.  Return FALSE on error.
*/
static bool
REMflush(void)
{
    int		i;

    if (REMbuffptr == REMbuffer) return TRUE; /* nothing buffered */
    i = xwrite(ToServer, REMbuffer, (int)(REMbuffptr - REMbuffer));
    REMbuffptr = REMbuffer;
    return i < 0 ? FALSE : TRUE;
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
    if (!stbuf[i].st_fname) stbuf[i].st_fname = NEW(char, SPOOLNAMEBUFF);
    if (!stbuf[i].st_id) stbuf[i].st_id = NEW(char, NNTP_STRLEN);
    (void)strcpy(stbuf[i].st_fname, Article);
    (void)strcpy(stbuf[i].st_id, MessageID);
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
	    return FALSE;
	if (REMbuffend - REMbuffer < i + 3) {
	    /* Line too long -- grow buffer. */
	    size = i * 2;
	    RENEW(REMbuffer, char, size);
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

    return TRUE;
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
	(void)REMwrite(QUIT, STRLEN(QUIT), FALSE);
	(void)REMflush();
    }
    (void)GetTimeInfo(&Now);
    STATend = TIMEINFOasDOUBLE(Now);
    if (GetResourceUsage(&usertime, &systime) < 0) {
	usertime = 0;
	systime = 0;
    }

    if (STATprint) {
	(void)printf(STAT1, REMhost, STAToffered, STATaccepted, STATrefused,
		STATrejected, STATmissing, STATacceptedsize, STATrejectedsize);
	(void)printf("\n");
	(void)printf(STAT2, REMhost, usertime, systime, STATend - STATbegin);
	(void)printf("\n");
    }

    syslog(L_NOTICE, STAT1, REMhost, STAToffered, STATaccepted, STATrefused,
		STATrejected, STATmissing, STATacceptedsize, STATrejectedsize);
    syslog(L_NOTICE, STAT2, REMhost, usertime, systime, STATend - STATbegin);
    if (retries)
	syslog(L_NOTICE, "%s %lu Streaming retries", REMhost, retries);

    if (BATCHfp != NULL && unlink(BATCHtemp) < 0 && errno != ENOENT)
	(void)fprintf(stderr, "Can't remove \"%s\", %s\n",
		BATCHtemp, strerror(errno));
    (void)sleep(1);
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
	(void)unlink(BATCHtemp);
	(void)fprintf(stderr, "Can't close \"%s\", %s\n",
		BATCHtemp, strerror(errno));
	ExitWithStats(1);
    }
    if (rename(BATCHtemp, BATCHname) < 0) {
	(void)fprintf(stderr, "Can't rename \"%s\", %s\n",
		BATCHtemp, strerror(errno));
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
            fprintf(stderr, "Can't create a temporary file, %s\n",
                    strerror(errno));
            ExitWithStats(1);
        }
        BATCHfp = fdopen(fd, "w");
        if (BATCHfp == NULL) {
            fprintf(stderr, "Can't open \"%s\", %s\n", BATCHtemp,
                    strerror(errno));
            ExitWithStats(1);
        }
    }

    /* Called only to get the file open? */
    if (Article == NULL)
	return;

    if (MessageID != NULL)
	(void)fprintf(BATCHfp, "%s %s\n", Article, MessageID);
    else
	(void)fprintf(BATCHfp, "%s\n", Article);
    if (fflush(BATCHfp) == EOF || ferror(BATCHfp)) {
	(void)fprintf(stderr, "Can't requeue \"%s\", %s\n",
		Article, strerror(errno));
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
	(void)fprintf(stderr, "Nothing sent -- leaving batchfile alone.\n");
	ExitWithStats(1);
    }

    (void)fprintf(stderr, "Rewriting batch file and exiting.\n");
    if (CanStream) {	/* streaming mode has a buffer of articles */
	int i;

	for (i = 0; i < STNBUF; i++) {    /* requeue unacknowledged articles */
	    if ((stbuf[i].st_fname) && (stbuf[i].st_fname[0] != '\0')) {
		if (Debug)
		    (void)fprintf(stderr, "stbuf[%d]= %s, %s\n",
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
		(void)fprintf(stderr, "Skipping long line in \"%s\".\n",
			BATCHname);
		(void)QIOread(BATCHqp);
		continue;
	    }
	    if (QIOerror(BATCHqp)) {
		(void)fprintf(stderr, "Can't read \"%s\", %s\n",
			BATCHname, strerror(errno));
		ExitWithStats(1);
	    }

	    /* Normal EOF. */
	    break;
	}

	if (fprintf(BATCHfp, "%s\n", p) == EOF
	 || ferror(BATCHfp)) {
	    (void)fprintf(stderr, "Can't requeue \"%s\", %s\n",
		    p, strerror(errno));
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
**  and the dot escape.  Return TRUE if okay, *or we got interrupted.*
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
	return FALSE;

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
		return TRUE;
	    if (i < 0) {
		if (errno == EINTR)
		    goto Again;
		return FALSE;
	    }
	    if (i == 0 || !FD_ISSET(FromServer, &rmask))
		return FALSE;
	    count = read(FromServer, buffer, sizeof buffer);
	    if (GotInterrupt)
		return TRUE;
	    if (count <= 0)
		return FALSE;
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
	    return FALSE;
	for (q = &start[1]; (*p++ = *q++) != '\0'; )
	    continue;
    }
    return TRUE;
}


/*
**  Handle the interrupt.
*/
static void
Interrupted(char *Article, char *MessageID) {
    (void)fprintf(stderr, "Interrupted\n");
    RequeueRestAndExit(Article, MessageID);
}


/*
**  Returns the length of the headers.
*/
static int
HeadersLen(ARTHANDLE *art, int *iscmsg) {
    char	*p;
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
	return FALSE;
    if (HeadersFeed) {
	struct iovec vec[3];
	char buf[20];
	int iscmsg = 0;
	int len = HeadersLen(art, &iscmsg);

	vec[0].iov_base = art->data;
	vec[0].iov_len = len;
	/* Add 14 bytes, which maybe will be the length of the Bytes header */
	sprintf(buf, "Bytes: %d\r\n", art->len + 14);
	vec[1].iov_base = buf;
	vec[1].iov_len = strlen(buf);
	if (iscmsg) {
	    vec[2].iov_base = art->data + len;
	    vec[2].iov_len = art->len - len;
	} else {
	    vec[2].iov_base = (char *) ".\r\n";
	    vec[2].iov_len = 3;
	}
	if (xwritev(ToServer, vec, 3) < 0)
	    return FALSE;
    } else
	if (xwrite(ToServer, art->data, art->len) < 0)
	    return FALSE;
    if (GotInterrupt)
	Interrupted(Article, MessageID);
    if (Debug) {
	(void)fprintf(stderr, "> [ article %d ]\n", art->len);
	(void)fprintf(stderr, "> .\n");
    }

    if (CanStream) return TRUE;	/* streaming mode does not wait for ACK */

    /* What did the remote site say? */
    if (!REMread(buff, (int)sizeof buff)) {
	(void)fprintf(stderr, "No reply after sending \"%s\", %s\n",
		Article, strerror(errno));
	return FALSE;
    }
    if (GotInterrupt)
	Interrupted(Article, MessageID);
    if (Debug)
	(void)fprintf(stderr, "< %s", buff);

    /* Parse the reply. */
    switch (atoi(buff)) {
    default:
	(void)fprintf(stderr, "Unknown reply after \"%s\" -- %s",
		Article, buff);
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
    return TRUE;
}


/*
**  Get the Message-ID header from an open article.
*/
static char *
GetMessageID(ARTHANDLE *art) {
    static char	*buff;
    static int	buffsize = 0;
    const char	*p, *q;

    if ((p = HeaderFindMem(art->data, art->len, "Message-ID", 10)) == NULL)
	return NULL;
    for (q = p; q < art->data + art->len; q++) {
        if (*q == '\r' || *q == '\n')
            break;
    }
    if (q == art->data + art->len)
	return NULL;
    if (buffsize < q - p) {
	if (buffsize == 0)
	    buff = NEW(char, q - p + 1);
	else
	    RENEW(buff, char, q - p + 1);
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
    GotInterrupt = TRUE;

    /* Let two interrupts kill us. */
    xsignal(s, SIG_DFL);
}


/*
**  Mark that the alarm went off.
*/
static RETSIGTYPE
CATCHalarm(int s UNUSED)
{
    GotAlarm = TRUE;
    if (JMPyes)
	longjmp(JMPwhere, 1);
}

/* check articles in streaming NNTP mode
** return TRUE on failure.
*/
static bool
check(int i) {
    char	buff[NNTP_STRLEN];

    /* send "check <ID>" to the other system */
    (void)sprintf(buff, "check %s", stbuf[i].st_id);
    if (!REMwrite(buff, (int)strlen(buff), FALSE)) {
	(void)fprintf(stderr, "Can't check article, %s\n",
		strerror(errno));
	return TRUE;
    }
    STAToffered++;
    if (Debug) {
	if (stbuf[i].st_retry)
	    (void)fprintf(stderr, "> %s (retry %d)\n", buff, stbuf[i].st_retry);
	else
	    (void)fprintf(stderr, "> %s\n", buff);
    }
    if (GotInterrupt)
	Interrupted(stbuf[i].st_fname, stbuf[i].st_id);

    /* That all.  Response is checked later by strlisten() */
    return FALSE;
}

/* Send article in "takethis <id> streaming NNTP mode.
** return TRUE on failure.
*/
static bool
takethis(int i) {
    char	buff[NNTP_STRLEN];

    if (!stbuf[i].art) {
        fprintf(stderr, "Internal error: null article for %s in takethis\n",
                stbuf[i].st_fname);
        return TRUE;
    }
    /* send "takethis <ID>" to the other system */
    (void)sprintf(buff, "takethis %s", stbuf[i].st_id);
    if (!REMwrite(buff, (int)strlen(buff), FALSE)) {
        (void)fprintf(stderr, "Can't send takethis <id>, %s\n",
                      strerror(errno));
        return TRUE;
    }
    if (Debug)
        (void)fprintf(stderr, "> %s\n", buff);
    if (GotInterrupt)
        Interrupted((char *)0, (char *)0);
    if (!REMsendarticle(stbuf[i].st_fname, stbuf[i].st_id, stbuf[i].art))
        return TRUE;
    stbuf[i].st_size = stbuf[i].art->len;
    article_free(stbuf[i].art); /* should not need file again */
    stbuf[i].art = 0;		/* so close to free descriptor */
    stbuf[i].st_age = 0;
    /* That all.  Response is checked later by strlisten() */
    return FALSE;
}


/* listen for responses.  Process acknowledgments to remove items from
** the queue.  Also sends the articles on request.  Returns TRUE on error.
** return TRUE on failure.
*/
static bool
strlisten(void)
{
    int		resp;
    int		i;
    char	*id, *p;
    char	buff[NNTP_STRLEN];
    int		hash;

    while(TRUE) {
	if (!REMread(buff, (int)sizeof buff)) {
	    (void)fprintf(stderr, "No reply to check, %s\n", strerror(errno));
	    return TRUE;
	}
	if (GotInterrupt)
	    Interrupted((char *)0, (char *)0);
	if (Debug)
	    (void)fprintf(stderr, "< %s", buff);

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
		    return (TRUE); /* can't find it! */
		}
	    } else {
		syslog(L_NOTICE, CANT_PARSEIT, REMhost, REMclean(buff));
		return (TRUE);
	    }
	    break;
	case NNTP_GOODBYE_VAL:
	    /* Most likely out of space -- no point in continuing. */
	    syslog(L_NOTICE, IHAVE_FAIL, REMhost, REMclean(buff));
	    return TRUE;
	    /* NOTREACHED */
	default:
	    syslog(L_NOTICE, UNEXPECTED, REMhost, REMclean(buff));
	    if (Debug)
		(void)fprintf(stderr, "Unknown reply \"%s\"",
						    buff);
	    return (TRUE);
	}
	switch (resp) { /* now we take some action */
	case NNTP_RESENDID_VAL:	/* remote wants it later */
	    /* try again now because time has passed */
	    if (stbuf[i].st_retry < STNRETRY) {
		if (check(i)) return TRUE;
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
	    if (takethis(i)) return TRUE;
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
    return (FALSE);
}

/*
**  Print a usage message and exit.
*/
static void
Usage(void)
{
    (void)fprintf(stderr,
	"Usage: innxmit [-a] [-c] [-d] [-H] [-l] [-p] [-r] [-s] [-t#] [-T#] host file\n");
    exit(1);
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
                fprintf(stderr, "Requeue %s: %s\n", path, SMerrorstr);
                Requeue(path, id);
            }
        }
        return article;
    } else {
        fd = open(path, O_RDONLY);
        if (fd < 0)
            return NULL;
        if (fstat(fd, &st) < 0) {
            fprintf(stderr, "Requeue %s: %s\n", path, strerror(errno));
            Requeue(path, id);
            return NULL;
        }
        article = NEW(ARTHANDLE, 1);
        article->type = TOKEN_EMPTY;
        article->len = st.st_size;
        article->data = NEW(char, article->len);
        if (xread(fd, article->data, article->len) < 0) {
            fprintf(stderr, "Requeue %s: %s\n", path, strerror(errno));
            free(article->data);
            free(article);
            close(fd);
            Requeue(path, id);
            return NULL;
        }
        close(fd);
        p = memchr(article->data, '\n', article->len);
        if (p == NULL || p == article->data) {
            fprintf(stderr, "Requeue %s: can't find headers\n", path);
            free(article->data);
            free(article);
            Requeue(path, id);
            return NULL;
        }
        if (p[-1] != '\r') {
            p = ToWireFmt(article->data, article->len, &length);
            free(article->data);
            article->data = p;
            article->len = length;
        }
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
        free(article->data);
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

    (void)openlog("innxmit", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
    /* Set defaults. */
    if (ReadInnConf() < 0) exit(1);

    ConnectTimeout = 0;
    TotalTimeout = 0;
    
    (void)umask(NEWSUMASK);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "lacdHprst:T:vP:")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'P':
	    port = atoi(COPY(optarg));
	    break;
	case 'a':
	    AlwaysRewrite = TRUE;
	    break;
	case 'c':
	    DoCheck = FALSE;
	    break;
	case 'd':
	    Debug = TRUE;
	    break;
	case 'H':
	    HeadersFeed = TRUE;
	    break;
        case 'l':
            logRejects = TRUE ;
            break ;
	case 'p':
	    AlwaysRewrite = TRUE;
	    Purging = TRUE;
	    break;
	case 'r':
	    DoRequeue = FALSE;
	    break;
	case 's':
	    TryStream = FALSE;
	    break;
	case 't':
	    ConnectTimeout = atoi(optarg);
	    break;
	case 'T':
	    TotalTimeout = atoi(optarg);
	    break;
	case 'v':
	    STATprint = TRUE;
	    break;
	}
    ac -= optind;
    av += optind;

    /* Parse arguments; host and filename. */
    if (ac != 2)
	Usage();
    REMhost = av[0];
    BATCHname = av[1];

    if (chdir(innconf->patharticles) < 0) {
	(void)fprintf(stderr, "Can't cd to \"%s\", %s\n",
		innconf->patharticles, strerror(errno));
	exit(1);
    }

    val = TRUE;
    if (!SMsetup(SM_PREOPEN,(void *)&val)) {
	fprintf(stderr, "Can't setup the storage manager\n");
	exit(1);
    }
    if (!SMinit()) {
	fprintf(stderr, "Can't initialize the storage manager: %s\n", SMerrorstr);
	exit(1);
    }

    /* Open the batch file and lock others out. */
    if (BATCHname[0] != '/') {
	BATCHname = NEW(char, strlen(innconf->pathoutgoing) + 1 +
						strlen(av[1]) + 1);
	(void)sprintf(BATCHname, "%s/%s", innconf->pathoutgoing, av[1]);
    }
    if (((i = open(BATCHname, O_RDWR)) < 0) || ((BATCHqp = QIOfdopen(i)) == NULL)) {
	(void)fprintf(stderr, "Can't open \"%s\", %s\n",
		BATCHname, strerror(errno));
	SMshutdown();
	exit(1);
    }
    if (!inn_lock_file(QIOfileno(BATCHqp), INN_LOCK_WRITE, TRUE)) {
#if	defined(EWOULDBLOCK)
	if (errno == EWOULDBLOCK) {
	    SMshutdown();
	    exit(0);
	}
#endif	/* defined(EWOULDBLOCK) */
	(void)fprintf(stderr, "Can't lock \"%s\", %s\n",
		BATCHname, strerror(errno));
	SMshutdown();
	exit(1);
    }

    /* Get a temporary name in the same directory as the batch file. */
    p = strrchr(BATCHname, '/');
    BATCHtemp = NEW(char, strlen(BATCHname) + STRLEN("/bchXXXXXX") + 1);
    *p = '\0';
    (void)sprintf(BATCHtemp, "%s/bchXXXXXX", BATCHname);
    *p = '/';

    /* Set up buffer used by REMwrite. */
    REMbuffer = NEW(char, OUTPUT_BUFFER_SIZE);
    REMbuffend = &REMbuffer[OUTPUT_BUFFER_SIZE];
    REMbuffptr = REMbuffer;

    /* Start timing. */
    if (GetTimeInfo(&Now) < 0) {
	(void)fprintf(stderr, "Can't get time, %s\n", strerror(errno));
	SMshutdown();
	exit(1);
    }
    STATbegin = TIMEINFOasDOUBLE(Now);

    if (!Purging) {
	/* Open a connection to the remote server. */
	if (ConnectTimeout) {
	    GotAlarm = FALSE;
	    old = xsignal(SIGALRM, CATCHalarm);
            if (setjmp(JMPwhere)) {
                fprintf(stderr, "Can't connect to %s, timed out\n", REMhost);
                SMshutdown();
                exit(1);
            }
	    JMPyes = TRUE;
	    (void)alarm(ConnectTimeout);
	}
	if (NNTPconnect(REMhost, port, &From, &To, buff) < 0 || GotAlarm) {
	    i = errno;
	    (void)fprintf(stderr, "Can't connect to %s, %s\n",
		    REMhost, buff[0] ? REMclean(buff) : strerror(errno));
	    if (GotAlarm)
		syslog(L_NOTICE, CANT_CONNECT, REMhost, "timeout");
	    else 
		syslog(L_NOTICE, CANT_CONNECT, REMhost,
		    buff[0] ? REMclean(buff) : strerror(i));
	    SMshutdown();
	    exit(1);
	}
	if (Debug)
	    (void)fprintf(stderr, "< %s\n", REMclean(buff));
	if (NNTPsendpassword(REMhost, From, To) < 0 || GotAlarm) {
	    i = errno;
	    (void)fprintf(stderr, "Can't authenticate with %s, %s\n",
		    REMhost, strerror(errno));
	    syslog(L_ERROR, CANT_AUTHENTICATE,
		REMhost, GotAlarm ? "timeout" : strerror(i));
	    /* Don't send quit; we want the remote to print a message. */
	    SMshutdown();
	    exit(1);
	}
	if (ConnectTimeout) {
	    (void)alarm(0);
	    (void)xsignal(SIGALRM, old);
	    JMPyes = FALSE;
	}

	/* We no longer need standard I/O. */
	FromServer = fileno(From);
	ToServer = fileno(To);

	if (TryStream) {
	    if (!REMwrite(modestream, (int)strlen(modestream), FALSE)) {
		(void)fprintf(stderr, "Can't negotiate %s, %s\n",
			modestream, strerror(errno));
	    }
	    if (Debug)
		(void)fprintf(stderr, ">%s\n", modestream);
	    /* Does he understand mode stream? */
	    if (!REMread(buff, (int)sizeof buff)) {
		(void)fprintf(stderr, "No reply to %s, %s\n",
				modestream, strerror(errno));
	    } else {
		if (Debug)
		    (void)fprintf(stderr, "< %s", buff);

		/* Parse the reply. */
		switch (atoi(buff)) {
		default:
		    (void)fprintf(stderr, "Unknown reply to \"%s\" -- %s",
			    modestream, buff);
		    CanStream = FALSE;
		    break;
		case NNTP_OK_STREAM_VAL:	/* YES! */
		    CanStream = TRUE;
		    break;
                case NNTP_AUTH_NEEDED_VAL: /* authentication refusal */
		case NNTP_BAD_COMMAND_VAL: /* normal refusal */
		    CanStream = FALSE;
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
	    if (!REMwrite(modeheadfeed, strlen(modeheadfeed), FALSE)) {
		fprintf(stderr, "Can't negotiate %s, %s\n",
			modeheadfeed, strerror(errno));
	    }
	    if (Debug)
		fprintf(stderr, ">%s\n", modeheadfeed);
	    if (!REMread(buff, sizeof buff)) {
		fprintf(stderr, "No reply to %s, %s\n",
				modeheadfeed, strerror(errno));
	    } else {
		if (Debug)
		    fprintf(stderr, "< %s", buff);

		/* Parse the reply. */
		switch (atoi(buff)) {
		case 250:		/* YES! */
		    break;
		case NNTP_BAD_COMMAND_VAL: /* normal refusal */
		    fprintf(stderr, "\"%s\" not allowed -- %s\n",
			    modeheadfeed, buff);
		    exit(1);
		default:
		    fprintf(stderr, "Unknown reply to \"%s\" -- %s",
			    modeheadfeed, buff);
		    exit(1);
		}
	    }
	}
    }

    /* Set up signal handlers. */
    (void)xsignal(SIGHUP, CATCHinterrupt);
    (void)xsignal(SIGINT, CATCHinterrupt);
    (void)xsignal(SIGTERM, CATCHinterrupt);
    (void)xsignal(SIGPIPE, SIG_IGN);
    if (TotalTimeout) {
	(void)xsignal(SIGALRM, CATCHalarm);
	(void)alarm(TotalTimeout);
    }

    path = concatpath(innconf->pathdb, _PATH_HISTORY);
    History = HISopen(path, innconf->hismethod, HIS_RDONLY);
    free(path);

    /* Main processing loop. */
    GotInterrupt = FALSE;
    GotAlarm = FALSE;
    for (Article = NULL, MessageID = NULL; ; ) {
	if (GotAlarm) {
	    (void)fprintf(stderr, "Timed out\n");
	    /* Don't resend the current article. */
	    RequeueRestAndExit((char *)NULL, (char *)NULL);
	}
	if (GotInterrupt)
	    Interrupted(Article, MessageID);

	if ((Article = QIOread(BATCHqp)) == NULL) {
	    if (QIOtoolong(BATCHqp)) {
		(void)fprintf(stderr, "Skipping long line in \"%s\"\n",
			BATCHname);
		(void)QIOread(BATCHqp);
		continue;
	    }
	    if (QIOerror(BATCHqp)) {
		(void)fprintf(stderr, "Can't read \"%s\", %s\n",
			BATCHname, strerror(errno));
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
		(void)fprintf(stderr, "Ignoring line \"%s %s...\"\n",
			      Article, MessageID);
		continue;
	    }
	}

	if (*Article == '\0') {
	    if (MessageID)
		(void)fprintf(stderr, "Empty filename for \"%s\" in \"%s\"\n",
		    MessageID, BATCHname);
	    else (void)fprintf(stderr,
		    "Empty filename, no Message-ID in \"%s\"\n", BATCHname);
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
            (void)fprintf(stderr, "Dropping article in \"%s\" - long message id \"%s\"\n",
                          BATCHname, MessageID);
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
		(void)fprintf(stderr, SKIPPING, Article, "no Message-ID");
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
		    (void)fprintf(stderr, "Skipping duplicate ID %s\n",
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
	(void)sprintf(buff, "ihave %s", MessageID);
	if (!REMwrite(buff, (int)strlen(buff), FALSE)) {
	    (void)fprintf(stderr, "Can't offer article, %s\n",
		    strerror(errno));
            article_free(art);
	    RequeueRestAndExit(Article, MessageID);
	}
	STAToffered++;
	if (Debug)
	    (void)fprintf(stderr, "> %s\n", buff);
	if (GotInterrupt)
	    Interrupted(Article, MessageID);

	/* Does he want it? */
	if (!REMread(buff, (int)sizeof buff)) {
	    (void)fprintf(stderr, "No reply to ihave, %s\n", strerror(errno));
            article_free(art);
	    RequeueRestAndExit(Article, MessageID);
	}
	if (GotInterrupt)
	    Interrupted(Article, MessageID);
	if (Debug)
	    (void)fprintf(stderr, "< %s", buff);

	/* Parse the reply. */
	switch (atoi(buff)) {
	default:
	    (void)fprintf(stderr, "Unknown reply to \"%s\" -- %s",
		    Article, buff);
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
	(void)fprintf(stderr, "Can't remove \"%s\", %s\n",
		BATCHtemp, strerror(errno));
    ExitWithStats(0);
    /* NOTREACHED */
    return 0;
}
