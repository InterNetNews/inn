/*
**  Transmit articles to remote site.
**  Modified for NNTP streaming: 1996-01-03 Jerry Aguirre.
*/

#include "portable/system.h"

#include "portable/socket.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <syslog.h>

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#include <time.h>

/*
**  Needed on AIX 4.1 to get fd_set and friends.
*/
#ifdef HAVE_SYS_SELECT_H
#    include <sys/select.h>
#endif

#include "inn/history.h"
#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/nntp.h"
#include "inn/paths.h"
#include "inn/qio.h"
#include "inn/storage.h"
#include "inn/timer.h"
#include "inn/wire.h"

#define OUTPUT_BUFFER_SIZE (16 * 1024)

/*
**  Streaming extensions to NNTP.  This extension removes the lock-step
**  limitation of conventional NNTP.  Article transfer is several times
**  faster.  Negotiated and falls back to old mode if receiver refuses.
*/

/*
**  Max number of articles that can be streamed ahead.
*/
#define STNBUF             64

/*
**  Send TAKETHIS without CHECK if this many articles were accepted in a row.
*/
#define STNC               16

/*
**  Typical number of articles to stream.
**  Must be able to fopen this many articles.
*/
#define STNBUFL            (STNBUF / 2)

/*
**  Number of retries before requeueing to disk.
*/
#define STNRETRY           5

/*
**  Tracking information for an article in flight.
**  Only used in streaming mode.
**
**  stbuf functions as a ring buffer, each entry
**  representing an article we've sent a request
**  for but not yet received the response for.
**
**  stnq is the number of articles "in flight" and
**  stoldest is the oldest one in flight, i.e. the
**  next response received corresponds to stdoldest.
**
**  We always require:
**     0 <= stnq <= STNBUF
**     0 <= stoldest < STNBUF
*/
struct stbufs {                      /* for each article we are procesing */
    char st_fname[SPOOLNAMEBUFF];    /* file name */
    char st_id[NNTP_MAXLEN_COMMAND]; /* Message-ID */
    int st_retry;                    /* retry count (0 for the first try) */
    ARTHANDLE *st_art;               /* arthandle to read article contents */
    int st_hash;                     /* hash value to speed searches */
    long st_size;                    /* article size */
};
static struct stbufs stbuf[STNBUF]; /* we keep track of this many articles */
static int stnq;      /* current number of active entries in stbuf */
static long stnofail; /* Count of consecutive successful sends */
static int stoldest;  /* Oldest allocated entry to stbuf (if stnq!=0) */

static int TryStream = true;  /* Should attempt stream negotation? */
static int CanStream = false; /* Result of stream negotation */
static int DoCheck = true;    /* Should check before takethis? */
static char modestream[] = "MODE STREAM";
static char modeheadfeed[] = "MODE HEADFEED";
static long retries = 0;
static int logRejects = false; /* syslog the 437 responses. */


/*
**  Syslog formats - collected together so they remain consistent.
*/
#define STAT1                                                                 \
    "%s stats offered %lu accepted %lu refused %lu rejected %lu missing %lu " \
    "accsize %.0f rejsize %.0f"
#define STAT2             "%s times user %.3f system %.3f elapsed %.3f"
#define GOT_BADCOMMAND    "%s rejected %s %s"
#define REJECTED          "%s rejected %s (%s) %s"
#define REJ_STREAM        "%s rejected (%s) %s"
#define CANT_CONNECT      "%s connect failed %s"
#define CANT_AUTHENTICATE "%s authenticate failed %s"
#define IHAVE_FAIL        "%s ihave failed %s"

#define CANT_FINDIT       "%s can't find %s"
#define CANT_PARSEIT      "%s can't parse ID %s"
#define UNEXPECTED        "%s unexpected response code %s"

/*
**  Global variables.
*/
static bool AlwaysRewrite;
static bool Debug;
static bool DoRequeue = true;
static bool Purging;
static bool STATprint;
static bool HeadersFeed;
static char *BATCHname;
static char *BATCHtemp;
static char *REMhost;
static double STATbegin;
static double STATend;
static FILE *BATCHfp;
static int FromServer;
static int ToServer;
static struct history *History;
static QIOSTATE *BATCHqp;
static sig_atomic_t GotAlarm;
static sig_atomic_t GotInterrupt;
static sig_atomic_t JMPyes;
static jmp_buf JMPwhere;
static char *REMbuffer;
static char *REMbuffptr;
static char *REMbuffend;
static unsigned long STATaccepted;
static unsigned long STAToffered;
static unsigned long STATrefused;
static unsigned long STATrejected;
static unsigned long STATmissing;
static double STATacceptedsize;
static double STATrejectedsize;


/*
**  Prototypes.
*/
static ARTHANDLE *article_open(const char *path, const char *id);
static void article_free(ARTHANDLE *);
static void Usage(void) __attribute__((__noreturn__));
static void RequeueRestAndExit(char *, char *) __attribute__((__noreturn__));
static void ExitWithStats(int) __attribute__((__noreturn__));


/*
**  Return true if the history file has the article expired.
*/
static bool
Expired(char *MessageID)
{
    return !HISlookup(History, MessageID, NULL, NULL, NULL, NULL);
}


/*
**  Flush and reset the site's output buffer.  Return false on error.
*/
static bool
REMflush(void)
{
    int i;

    if (REMbuffptr == REMbuffer)
        return true; /* nothing buffered */
    i = xwrite(ToServer, REMbuffer, (int) (REMbuffptr - REMbuffer));
    REMbuffptr = REMbuffer;
    return i < 0 ? false : true;
}

/*
**  Return index to entry matching this Message-ID.  Else return -1.
**  The hash is to speed up the search.
*/
static int
stindex(char *MessageID, int hash)
{
    int i;

    for (i = 0; i < STNBUF; i++) { /* linear search for ID */
        if (stbuf[i].st_id[0] && (stbuf[i].st_hash == hash)) {
            int n;

            if (strcmp(MessageID, stbuf[i].st_id))
                continue;

            for (n = 0; (MessageID[n] != '@') && (MessageID[n] != '\0'); n++)
                ;
            if (strncmp(MessageID, stbuf[i].st_id, n))
                continue;
            else
                break; /* found a match */
        }
    }
    if (i >= STNBUF)
        i = -1; /* no match found ? */
    return (i);
}

/*  stidhash(): calculate a hash value for Message-IDs to speed comparisons */
static int
stidhash(char *MessageID)
{
    char *p;
    int hash;

    hash = 0;
    for (p = MessageID + 1; *p && (*p != '>'); p++) {
        hash <<= 1;
        if (isupper((unsigned char) *p)) {
            hash += tolower((unsigned char) *p);
        } else {
            hash += *p;
        }
    }
    return hash;
}

/*
**  stalloc(): save path, ID, and qp into the ring buffer.
**
**  Called just before issuing a CHECK (or TAKETHIS, if we're not doing CHECK)
**  i.e. in streaming mode only.
**  Should only be called if there are free slots, i.e. if sntq<STNBUF.
**/
static int
stalloc(const char *Article, const char *MessageID, ARTHANDLE *art, int hash,
        int retry)
{
    int i;

    if (stnq >= STNBUF) { /* shouldn't be called if stnq>=STNBUF */
        syslog(L_ERROR, "stalloc: Internal error");
        return (-1);
    }
    /* Find the next free slot */
    i = (stoldest + stnq) % STNBUF;
    if ((int) strlen(Article) >= SPOOLNAMEBUFF) {
        syslog(L_ERROR, "stalloc: filename longer than %d", SPOOLNAMEBUFF);
        return (-1);
    }
    strlcpy(stbuf[i].st_fname, Article, SPOOLNAMEBUFF);
    strlcpy(stbuf[i].st_id, MessageID, NNTP_MAXLEN_COMMAND);
    stbuf[i].st_art = art;
    stbuf[i].st_hash = hash;
    stbuf[i].st_retry = retry;
    stnq++;
    return i;
}

/*
** strel(): release for reuse one of the streaming mode entries.
**
** Returns 0 on success an non-0 on error (which probably means
** the peer got the reply order wrong).
**/
static int
strel(int i)
{
    /* Consistency check */
    if (i != stoldest) {
        syslog(L_ERROR, "strel: Ordering error");
        return (-1);
    }
    article_free(stbuf[i].st_art);
    stbuf[i].st_art = NULL;
    stbuf[i].st_id[0] = '\0';
    stbuf[i].st_fname[0] = '\0';
    stoldest = (stoldest + 1) % STNBUF;
    stnq--;
    return 0;
}

/*
**  Send a line to the server, adding the dot escape and \r\n.
*/
static bool
REMwrite(char *p, int i, bool escdot)
{
    int size;

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
    static char QUIT[] = "QUIT";
    double usertime;
    double systime;

    if (!Purging) {
        REMwrite(QUIT, strlen(QUIT), false);
        REMflush();
    }
    STATend = TMRnow_double();
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
    if (ferror(BATCHfp) || fflush(BATCHfp) == EOF || fclose(BATCHfp) == EOF) {
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
RequeueRestAndExit(char *Article, char *MessageID)
{
    char *p;

    if (!AlwaysRewrite && STATaccepted == 0 && STATrejected == 0
        && STATrefused == 0 && STATmissing == 0) {
        warn("nothing sent -- leaving batchfile alone");
        ExitWithStats(1);
    }

    warn("rewriting batch file and exiting");
    if (CanStream) { /* streaming mode has a buffer of articles */
        int i;

        while (stnq > 0) { /* requeue unacknowledged articles */
            i = stoldest;  /* strel is picky about order */
            if (stbuf[i].st_fname[0] != '\0') {
                if (Debug)
                    fprintf(stderr, "stbuf[%d]= %s, %s\n", i,
                            stbuf[i].st_fname, stbuf[i].st_id);
                Requeue(stbuf[i].st_fname, stbuf[i].st_id);
                if (Article == stbuf[i].st_fname)
                    Article = NULL;
                strel(i); /* release entry */
                /* We ignore errors from strel here; the only
                 * possible error is an ordering violation and
                 * we take care to avoid that anyway. */
            }
        }
    }
    Requeue(Article, MessageID);

    for (; BATCHqp;) {
        if ((p = QIOread(BATCHqp)) == NULL) {
            if (QIOtoolong(BATCHqp)) {
                warn("skipping long line in %s", BATCHname);
                continue;
            }
            if (QIOerror(BATCHqp)) {
                syswarn("cannot read %s", BATCHname);
                ExitWithStats(1);
            }

            /* Normal EOF. */
            break;
        }

        if (fprintf(BATCHfp, "%s\n", p) == EOF || ferror(BATCHfp)) {
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
REMclean(char *buff)
{
    char *p;

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
REMread(char *start, int size)
{
    static int count;
    static char buffer[BUFSIZ];
    static char *bp;
    char *p;
    char *q;
    char *end;
    struct timeval t;
    fd_set rmask;
    int i;
    char c;

    if (!REMflush())
        return false;

    for (p = start, end = &start[size - 1];;) {
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
        for (q = &start[1]; (*p++ = *q++) != '\0';)
            continue;
    }
    return true;
}


/*
**  Handle the interrupt.
*/
__attribute__((__noreturn__)) static void
Interrupted(char *Article, char *MessageID)
{
    warn("interrupted");
    RequeueRestAndExit(Article, MessageID);
}


/*
**  Returns the length of the headers.
*/
static int
HeadersLen(ARTHANDLE *art, int *iscmsg)
{
    const char *p;
    char lastchar = -1;

    /* from nnrpd/article.c ARTsendmmap() */
    for (p = art->data; p < (art->data + art->len); p++) {
        if (*p == '\r')
            continue;
        if (*p == '\n') {
            if (lastchar == '\n') {
                if (*(p - 1) == '\r')
                    p--;
                break;
            }
            if ((*(p + 1) == 'C' || *(p + 1) == 'c')
                && strncasecmp(p + 1, "Control: ", 9) == 0)
                *iscmsg = 1;
        }
        lastchar = *p;
    }
    return (p - art->data);
}


/*
**  Send a whole article to the server.
**  Called from the takethis(), i.e. when sending a TAKETHIS
**  and when we get a 335 response to IHAVE.
*/
static bool
REMsendarticle(char *Article, char *MessageID, ARTHANDLE *art)
{
    char buff[NNTP_MAXLEN_COMMAND];

    if (!REMflush())
        return false;
    if (HeadersFeed) {
        struct iovec vec[3];
        char buf[32];
        int iscmsg = 0;
        int len = HeadersLen(art, &iscmsg);

        vec[0].iov_base = (char *) art->data;
        vec[0].iov_len = len;
        /* Add 14 bytes, which maybe will be the length of the Bytes header
         * field */
        snprintf(buf, sizeof(buf), "Bytes: %lu\r\n",
                 (unsigned long) art->len + 14);
        vec[1].iov_base = buf;
        vec[1].iov_len = strlen(buf);
        if (iscmsg) {
            vec[2].iov_base = (char *) art->data + len;
            vec[2].iov_len = art->len - len;
        } else {
            vec[2].iov_base = (char *) "\r\n.\r\n";
            vec[2].iov_len = 5;
        }
        if (xwritev(ToServer, vec, 3) < 0)
            return false;
    } else if (xwrite(ToServer, art->data, art->len) < 0)
        return false;
    if (GotInterrupt)
        Interrupted(Article, MessageID);
    if (Debug) {
        fprintf(stderr, "> [ article %lu ]\n", (unsigned long) art->len);
        fprintf(stderr, "> .\n");
    }

    if (CanStream)
        return true; /* streaming mode does not wait for ACK */

    /* What did the remote site say? (IHAVE only) */
    if (!REMread(buff, (int) sizeof buff)) {
        syswarn("no reply after sending %s (socket timeout?)", Article);
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
    case NNTP_ERR_COMMAND: /* 500 */
    case NNTP_ERR_ACCESS:
        /* The receiving server is likely confused... no point in continuing!
         */
        syslog(L_FATAL, GOT_BADCOMMAND, REMhost, MessageID, REMclean(buff));
        RequeueRestAndExit(Article, MessageID);
        /* NOTREACHED */
    case NNTP_FAIL_IHAVE_DEFER:
    case NNTP_FAIL_TERMINATING:
    case NNTP_FAIL_ACTION:
        Requeue(Article, MessageID);
        break;
    case NNTP_OK_IHAVE:
        STATaccepted++;
        STATacceptedsize += (double) art->len;
        break;
    case NNTP_FAIL_IHAVE_REJECT:
    case NNTP_ERR_SYNTAX: /* 501 */
        if (logRejects)
            syslog(L_NOTICE, REJECTED, REMhost, MessageID, Article,
                   REMclean(buff));
        STATrejected++;
        STATrejectedsize += (double) art->len;
        break;
    }

    /* Article sent, or we requeued it. */
    return true;
}


/*
**  Get the Message-ID header field from an open article.
*/
static char *
GetMessageID(ARTHANDLE *art)
{
    static char *buff = NULL;
    static size_t buffsize = 0; /* total size of buff */
    size_t buffneed;
    const char *p, *q;

    p = wire_findheader(art->data, art->len, "Message-ID", true);
    if (p == NULL)
        return NULL;
    for (q = p; q < art->data + art->len; q++) {
        if (*q == '\r' || *q == '\n')
            break;
    }
    if (q == art->data + art->len)
        return NULL;

    buffneed = q - p + 1; /* bytes needed, including '\0' terminator */
    if (buffsize < buffneed) {
        buff = xrealloc(buff, buffneed);
        buffsize = buffneed;
    }
    memcpy(buff, p, q - p);
    buff[q - p] = '\0';
    return buff;
}


/*
**  Mark that we got interrupted.
*/
static void
CATCHinterrupt(int s)
{
    GotInterrupt = true;

    /* Let two interrupts kill us. */
    xsignal(s, SIG_DFL);
}


/*
**  Mark that the alarm went off.
*/
static void
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
check(int i)
{
    char buff[NNTP_MAXLEN_COMMAND];

    /* Send "CHECK <mid>" to the other system. */
    snprintf(buff, sizeof(buff), "CHECK %s", stbuf[i].st_id);
    if (!REMwrite(buff, (int) strlen(buff), false)) {
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
**
** Called when we get a 238 response to CHECK, or from the
** main processing loop when we are streaming and are not
** using CHECK (i.e. when DoCheck=false).
*/
static bool
takethis(int i)
{
    char buff[NNTP_MAXLEN_COMMAND];

    if (!stbuf[i].st_art) {
        warn("internal error: null article for %s in takethis",
             stbuf[i].st_fname);
        return true;
    }
    /* Send "TAKETHIS <mid>" to the other system. */
    snprintf(buff, sizeof(buff), "TAKETHIS %s", stbuf[i].st_id);
    if (!REMwrite(buff, (int) strlen(buff), false)) {
        syswarn("cannot send TAKETHIS");
        return true;
    }
    if (Debug)
        fprintf(stderr, "> %s\n", buff);
    if (GotInterrupt)
        Interrupted((char *) 0, (char *) 0);
    if (!REMsendarticle(stbuf[i].st_fname, stbuf[i].st_id, stbuf[i].st_art))
        return true;
    stbuf[i].st_size = stbuf[i].st_art->len;
    /* We won't need st_art again, since we never do a fast retry of TAKETHIS,
     * we so close it straight away.  It would get free (from strel) anyway,
     * but we can keep memory usage down by freeing it straight away. */
    article_free(stbuf[i].st_art);
    stbuf[i].st_art = NULL;
    /* That all.  Response is checked later by strlisten() */
    return false;
}


/* listen for responses.  Process acknowledgments to remove items from
** the queue.  Also sends the articles on request.
**
** Called from the main processing loop when there are enough commands
** in flight to be worth waiting for responses, and after the main processing
** loop to reap all remaining responses.
**
** Returns false on success and true on error.  The callers respond to
** errors by rewriting the batch file and exiting.
*/
static bool
strlisten(void)
{
    int resp;
    int i;
    char *id, *p;
    char buff[NNTP_MAXLEN_COMMAND];
    int hash;
    struct stbufs st;
    bool (*submit)(int) = NULL;

    while (true) {
        if (!REMread(buff, (int) sizeof buff)) {
            syswarn("no reply to CHECK or TAKETHIS (socket timeout?)");
            return true;
        }
        if (GotInterrupt)
            Interrupted((char *) 0, (char *) 0);
        if (Debug)
            fprintf(stderr, "< %s", buff);

        /* Parse the reply. */
        resp = atoi(buff);

        /* Skip the 1XX informational messages. */
        if ((resp >= 100) && (resp < 200))
            continue;

        switch (resp) {       /* first time is to verify it */
        case NNTP_ERR_SYNTAX: /* 501 */
            /* Assume replies are in order (per RFC3977 s3.5) */
            i = stoldest;
            break;
        case NNTP_FAIL_CHECK_REFUSE:    /* 438 */
        case NNTP_OK_CHECK:             /* 238 */
        case NNTP_OK_TAKETHIS:          /* 239 */
        case NNTP_FAIL_TAKETHIS_REJECT: /* 439 */
        case NNTP_FAIL_CHECK_DEFER:     /* 431 */
            /* Looking for the Message-ID reflects the historical
             * design in which we didn't track the order of commands,
             * and so needed to match replies back to commands by
             * parsing out the Message-ID.  Since we track order
             * carefully now, that's no longer necessary, and it
             * functions as a correctness check on the peer only. */
            if ((id = strchr(buff, '<')) != NULL) {
                p = strchr(id, '>');
                if (p)
                    *(p + 1) = '\0';
                hash = stidhash(id);
                i = stindex(id, hash); /* find table entry */
                if (i < 0) {           /* should not happen */
                    syslog(L_NOTICE, CANT_FINDIT, REMhost, REMclean(buff));
                    return (true); /* can't find it! */
                }
                if (i != stoldest) { /* also should not happen */
                    syslog(L_ERROR,
                           "%s: response for %s out of order (should be %s)",
                           REMhost, p, stbuf[i].st_id);
                    return true; /* too broken, can't go on */
                }
            } else {
                syslog(L_NOTICE, CANT_PARSEIT, REMhost, REMclean(buff));
                return (true);
            }
            break;
        case NNTP_FAIL_TERMINATING: /* 400 */
        case NNTP_FAIL_ACTION:      /* 403 */
            /* Most likely out of space -- no point in continuing. */
            syslog(L_NOTICE, IHAVE_FAIL, REMhost, REMclean(buff));
            return true;
            /* NOTREACHED */
        default:
            syslog(L_NOTICE, UNEXPECTED, REMhost, REMclean(buff));
            if (Debug)
                fprintf(stderr, "Unknown reply \"%s\"", buff);
            return (true);
        }

        /* Steal the article details for (re-)submission */
        st = stbuf[i];
        stbuf[i].st_art = NULL; /* steal pointer */
        /* We must always release, to maintain the ring buffer */
        if (strel(i)) { /* release entry */
            return true;
        }
        i = INT_MAX; /* invalid now */
        /* Resubmission callback, if needed */
        submit = NULL;

        switch (resp) {             /* now we take some action */
        case NNTP_FAIL_CHECK_DEFER: /* 431 remote wants it later */
            /* try again now because time has passed */
            if (st.st_retry < STNRETRY) {
                /* fast retry */
                st.st_retry++;
                retries++;
                submit = check;
            } else { /* requeue to disk for later */
                Requeue(st.st_fname, st.st_id);
            }
            break;

        case NNTP_ERR_SYNTAX:        /* 501 */
        case NNTP_FAIL_CHECK_REFUSE: /* 438 remote doesn't want it */
            STATrefused++;
            stnofail = 0;
            break;

        case NNTP_OK_CHECK: /* 238 remote wants article */
            st.st_retry = 0;
            submit = takethis;
            stnofail++;
            break;

        case NNTP_OK_TAKETHIS: /* 239 remote received it OK */
            STATacceptedsize += (double) st.st_size;
            STATaccepted++;
            break;

        case NNTP_FAIL_TAKETHIS_REJECT: /* 439 */
            STATrejectedsize += (double) st.st_size;
            if (logRejects)
                syslog(L_NOTICE, REJ_STREAM, REMhost, st.st_fname,
                       REMclean(buff));
            /* FIXME Caution THERE BE DRAGONS, I don't think this logs
             * properly.  The Message-ID is returned in the peer response...
             * so this is redundant stb.st_id, st.st_fname, REMclean(buff)); */
            STATrejected++;
            stnofail = 0;
            break;
        }
        if (submit != NULL) {
            if ((i = stalloc(st.st_fname, st.st_id, st.st_art, st.st_hash,
                             st.st_retry))
                < 0)
                return true;
            st.st_art = NULL; /* stalloc takes ownership of st_art */
            if (submit(i))
                return true;
        }
        /* Free the article handle if it's still live */
        article_free(st.st_art);
        st.st_art = NULL;
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
    int fd;
    size_t length;
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
            p = wire_from_native(data, article->len, &length);
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
    if (!article)
        return; /* article_free(NULL) is always safe */
    if (article->type == TOKEN_EMPTY) {
        free((char *) article->data);
        free(article);
    } else
        SMfreearticle(article);
}


int
main(int ac, char *av[])
{
    int i;
    char *p;
    ARTHANDLE *art;
    FILE *From;
    FILE *To;
    char buff[8192 + 128];
    char *Article;
    char *MessageID;
    void (*volatile old)(int) = NULL;
    volatile int port = NNTP_PORT;
    bool val;
    char *path;
    volatile unsigned int ConnectTimeout;
    volatile unsigned int TotalTimeout;

    openlog("innxmit", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
    message_program_name = "innxmit";

    /* Set defaults. */
    if (!innconf_read(NULL))
        exit(1);

    ConnectTimeout = 0;
    TotalTimeout = 0;

    umask(NEWSUMASK);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "acdHlpP:rst:T:v")) != EOF)
        switch (i) {
        default:
            Usage();
            /* NOTREACHED */
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
            logRejects = true;
            break;
        case 'p':
            AlwaysRewrite = true;
            Purging = true;
            break;
        case 'P':
            port = atoi(optarg);
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
    if (!SMsetup(SM_PREOPEN, (void *) &val))
        die("cannot set up the storage manager");
    if (!SMinit())
        die("cannot initialize the storage manager: %s", SMerrorstr);

    /* Open the batch file and lock others out. */
    if (BATCHname[0] != '/') {
        BATCHname = concatpath(innconf->pathoutgoing, av[1]);
    }
    if (((i = open(BATCHname, O_RDWR)) < 0)
        || ((BATCHqp = QIOfdopen(i)) == NULL)) {
        syswarn("cannot open %s", BATCHname);
        SMshutdown();
        exit(1);
    }
    if (!inn_lock_file(QIOfileno(BATCHqp), INN_LOCK_WRITE, true)) {
#if defined(EWOULDBLOCK)
        if (errno == EWOULDBLOCK) {
            SMshutdown();
            exit(0);
        }
#endif /* defined(EWOULDBLOCK) */
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
    STATbegin = TMRnow_double();

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
        if (NNTPconnect(REMhost, port, &From, &To, buff, sizeof(buff)) < 0
            || GotAlarm) {
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
            syslog(L_ERROR, CANT_AUTHENTICATE, REMhost,
                   GotAlarm ? "timeout" : strerror(i));
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
            if (!REMwrite(modestream, (int) strlen(modestream), false)) {
                syswarn("cannot negotiate %s", modestream);
            }
            if (Debug)
                fprintf(stderr, ">%s\n", modestream);
            /* Does he understand mode stream? */
            if (!REMread(buff, (int) sizeof buff)) {
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
                case NNTP_OK_STREAM: /* YES! */
                    CanStream = true;
                    break;
                case NNTP_FAIL_AUTH_NEEDED: /* authentication refusal */
                case NNTP_ERR_COMMAND:      /* unknown MODE command */
                case NNTP_ERR_SYNTAX:       /* unknown STREAM keyword */
                    CanStream = false;
                    break;
                }
            }
            if (CanStream) {
                for (i = 0; i < STNBUF; i++) { /* reset buffers */
                    stbuf[i].st_fname[0] = 0;
                    stbuf[i].st_id[0] = 0;
                    stbuf[i].st_art = NULL;
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
                case 250: /* YES! */
                    break;
                case NNTP_FAIL_AUTH_NEEDED: /* authentication refusal */
                case NNTP_ERR_COMMAND:      /* unknown MODE command */
                case NNTP_ERR_SYNTAX:       /* unknown STREAM keyword */
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

    path = concatpath(innconf->pathdb, INN_PATH_HISTORY);
    History = HISopen(path, innconf->hismethod, HIS_RDONLY);
    free(path);

    /* Main processing loop. */
    GotInterrupt = false;
    GotAlarm = false;
    for (Article = NULL, MessageID = NULL;;) {
        if (GotAlarm) {
            warn("timed out");
            /* Don't resend the current article. */
            RequeueRestAndExit((char *) NULL, (char *) NULL);
        }
        if (GotInterrupt)
            Interrupted(Article, MessageID);

        if ((Article = QIOread(BATCHqp)) == NULL) {
            if (QIOtoolong(BATCHqp)) {
                warn("skipping long line in %s", BATCHname);
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
        if (Article[0] == '/' && Article[strlen(innconf->patharticles)] == '/'
            && strncmp(Article, innconf->patharticles,
                       strlen(innconf->patharticles))
                   == 0)
            Article += strlen(innconf->patharticles) + 1;
        if ((MessageID = strchr(Article, ' ')) != NULL) {
            *MessageID++ = '\0';
            if (*MessageID != '<' || (p = strrchr(MessageID, '>')) == NULL
                || *++p != '\0') {
                warn("ignoring line %s %s...", Article, MessageID);
                continue;
            }
        }

        if (*Article == '\0') {
            if (MessageID)
                warn("empty file name for %s in %s", MessageID, BATCHname);
            else
                warn("empty file name, no Message-ID in %s", BATCHname);
            /* We could do a history lookup. */
            continue;
        }

        if (Purging && MessageID != NULL && !Expired(MessageID)) {
            Requeue(Article, MessageID);
            continue;
        }

        /* Drop articles with a Message-ID longer than NNTP_MAXLEN_MSGID to
           avoid overrunning buffers and throwing the server on the
           receiving end a blow from behind. */
        if (MessageID != NULL && strlen(MessageID) > NNTP_MAXLEN_MSGID) {
            warn("dropping article in %s: long Message-ID %s", BATCHname,
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
                warn("Skipping \"%s\" -- %s?\n", Article, "no Message-ID");
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
                    fprintf(stderr, "Skipping duplicate ID %s\n", MessageID);
                article_free(art);
                continue;
            }
            /* This code tries to optimize by sending a burst of "check"
             * commands before flushing the buffer.  This should result
             * in several being sent in one packet reducing the network
             * overhead.
             */
            if (DoCheck && (stnofail < STNC))
                lim = STNBUF;
            else
                lim = STNBUFL;
            if (stnq >= lim) {            /* need to empty a buffer */
                while (stnq >= STNBUFL) { /* or several */
                    if (strlisten()) {
                        RequeueRestAndExit(Article, MessageID);
                    }
                }
            }
            /* save new article in the buffer */
            i = stalloc(Article, MessageID, art, hash, 0);
            if (i < 0) {
                article_free(art);
                RequeueRestAndExit(Article, MessageID);
            }
            if (DoCheck && (stnofail < STNC)) {
                if (check(i)) {
                    RequeueRestAndExit((char *) NULL, (char *) NULL);
                }
            } else {
                STAToffered++;
                if (takethis(i)) {
                    RequeueRestAndExit((char *) NULL, (char *) NULL);
                }
            }
            continue; /* next article */
        }
        snprintf(buff, sizeof(buff), "IHAVE %s", MessageID);
        if (!REMwrite(buff, (int) strlen(buff), false)) {
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
        if (!REMread(buff, (int) sizeof buff)) {
            syswarn("no reply to IHAVE (socket timeout?)");
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
        case NNTP_ERR_COMMAND: /* 500 */
        case NNTP_ERR_ACCESS:  /* 502 */
            /* The receiving server is likely confused... no point in
             * continuing! */
            syslog(L_FATAL, GOT_BADCOMMAND, REMhost, MessageID,
                   REMclean(buff));
            RequeueRestAndExit(Article, MessageID);
            /* NOTREACHED */
        case NNTP_FAIL_ACTION:      /* 403 */
        case NNTP_FAIL_AUTH_NEEDED: /* 480 */
        case NNTP_FAIL_IHAVE_DEFER: /* 436 */
        case NNTP_FAIL_TERMINATING: /* 400 */
            /* Most likely out of space -- no point in continuing. */
            syslog(L_NOTICE, IHAVE_FAIL, REMhost, REMclean(buff));
            RequeueRestAndExit(Article, MessageID);
            /* NOTREACHED */
        case NNTP_CONT_IHAVE: /* 335 */
            if (!REMsendarticle(Article, MessageID, art))
                RequeueRestAndExit(Article, MessageID);
            break;
        case NNTP_FAIL_IHAVE_REFUSE: /* 435 */
        case NNTP_ERR_SYNTAX:        /* 501 e.g. peer disagrees about valid
                                        Message-IDs */
            STATrefused++;
            break;
        }

        article_free(art);
    }
    if (CanStream) { /* need to wait for rest of ACKs */
        while (stnq > 0) {
            if (strlisten()) {
                RequeueRestAndExit((char *) NULL, (char *) NULL);
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
