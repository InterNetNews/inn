/*  $Revision$
**
**  Transmit articles to remote site.
**  Modified for NNTP streaming: 3Jan96 Jerry Aguirre
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#if	defined(DO_NEED_TIME)
#include <time.h>
#endif	/* defined(DO_NEED_TIME) */
#include <sys/time.h>
#include <sys/uio.h>
#include "nntp.h"
#include "paths.h"
#include "logging.h"
#include "libinn.h"
#include "qio.h"
#include "dbz.h"
#include "macros.h"


/*
**  Supported encoding schemes.
*/
typedef enum _MIMEXFERTYPE {
    MTnotmime,
    MTquotedprintable,
    MTbase64
} MIMEXFERTYPE;


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
	QIOSTATE *st_qp;	/* IO to read article contents */
	int   st_hash;		/* hash value to speed searches */
};
static struct stbufs stbuf[STNBUF]; /* we keep track of this many articles */
static int stnq;	/* current number of active entries in stbuf */
static long stnofail;	/* Count of consecutive successful sends */

static int TryStream = TRUE;	/* Should attempt stream negotation? */
static int CanStream = FALSE;	/* Result of stream negotation */
static int DoCheck   = TRUE;	/* Should check before takethis? */
static char modestream[] = "mode stream";
static long retries = 0;
static int logRejects = TRUE ;  /* syslog the 437 responses. */



/*
** Syslog formats - collected together so they remain consistent
*/
STATIC char	STAT1[] =
	"%s stats offered %lu accepted %lu refused %lu rejected %lu";
STATIC char	STAT2[] = "%s times user %.3f system %.3f elapsed %.3f";
STATIC char	GOT_RESENDIT[] = "%s requeued %s %s";
STATIC char	GOT_BADCOMMAND[] = "%s rejected %s %s";
STATIC char	REJECTED[] = "%s rejected %s (%s) %s";
STATIC char	CANT_CONNECT[] = "%s connect failed %s";
STATIC char	CANT_AUTHENTICATE[] = "%s authenticate failed %s";
STATIC char	IHAVE_FAIL[] = "%s ihave failed %s";

STATIC char	CANT_FINDIT[] = "%s can't find %s";
STATIC char	CANT_PARSEIT[] = "%s can't parse ID %s";
STATIC char	UNEXPECTED[] = "%s unexpected response code %s";

/*
**  Global variables.
*/
STATIC BOOL		AlwaysRewrite;
STATIC BOOL		Debug;
STATIC BOOL		DoRequeue = TRUE;
STATIC BOOL		Purging;
STATIC BOOL		STATprint;
STATIC BOOL		Mime;
STATIC MIMEXFERTYPE	MimeArticle = MTnotmime;
STATIC char		*BATCHname;
STATIC char		*BATCHtemp;
STATIC char		*REMhost;
STATIC double		STATbegin;
STATIC double		STATend;
STATIC FILE		*BATCHfp;
STATIC int		FromServer;
STATIC int		ToServer;
STATIC QIOSTATE		*BATCHqp;
STATIC SIGVAR		GotAlarm;
STATIC SIGVAR		GotInterrupt;
STATIC SIGVAR		JMPyes;
STATIC jmp_buf		JMPwhere;
STATIC char		*REMbuffer;
STATIC char		*REMbuffptr;
STATIC char		*REMbuffend;
STATIC unsigned long	STATaccepted;
STATIC unsigned long	STAToffered;
STATIC unsigned long	STATrefused;
STATIC unsigned long	STATrejected;
STATIC char		*AltSpool;
STATIC char		*AltPath;


/*
**  Find the history file entry for the Message-ID and return a file
**  positioned at the third field.
*/
STATIC FILE *HistorySeek(char *MessageID)
{
    static char		*History = NULL;
    static FILE		*F;
    OFFSET_T		offset;
#ifndef	DO_TAGGED_HASH
    idxrec		ionevalue;
    idxrecext		iextvalue;
#endif

    /* Open the history file. */
    if (F == NULL) {
	if (History == NULL)
	    History = COPY(cpcatpath(innconf->pathdb, _PATH_HISTORY));
	if (!dbzinit(History)) {
	    (void)fprintf(stderr, "Can't set up \"%s\" database, %s\n",
		    History, strerror(errno));
	    SMshutdown();
	    exit(1);
	}
	if ((F = fopen(History, "r")) == NULL) {
	    (void)fprintf(stderr, "Can't open \"%s\" for reading, %s\n",
		    History, strerror(errno));
	    SMshutdown();
	    exit(1);
	}
    }

    /* Do the lookup. */
#ifdef	DO_TAGGED_HASH
    if ((offset = dbzfetch(HashMessageID(MessageID))) < 0)
	return NULL;
#else
    if (innconf->extendeddbz) {
	if (!dbzfetch(HashMessageID(MessageID), &iextvalue))
	    return NULL;
	offset = iextvalue.offset[HISTOFFSET];
    } else {
	if (!dbzfetch(HashMessageID(MessageID), &ionevalue))
	    return NULL;
	offset = ionevalue.offset;
    }
#endif

    /* Get the seek offset, and seek. */
    if (fseek(F, offset, SEEK_SET) == -1)
	return NULL;
    return F;
}


/*
**  Return TRUE if the history file has the article expired.
*/
STATIC BOOL
Expired(MessageID)
    char		*MessageID;
{
    register int	c;
    register int	i;
    register FILE	*F;

    if ((F = HistorySeek(MessageID)) == NULL)
	/* Assume the worst. */
	return TRUE;

    /* Move to the filename fields. */
    for (i = 2; (c = getc(F)) != EOF && c != '\n'; )
	if (c == HIS_FIELDSEP && --i == 0)
	    break;
    if (c != HIS_FIELDSEP)
	return TRUE;

    /* See if we get any filename before the end of the line. */
    while ((c = getc(F)) != EOF && c != '\n')
	if (!ISWHITE(c))
	    /* Found non-whitespace; assume it's a filename. */
	    return FALSE;
    return TRUE;
}


/*
**  Flush and reset the site's output buffer.  Return FALSE on error.
*/
STATIC BOOL
REMflush()
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
STATIC int
stindex(MessageID, hash)
    char		*MessageID;
    int hash;
{
    register int i;

    for (i = 0; i < STNBUF; i++) { /* linear search for ID */
	if ((stbuf[i].st_id) && (stbuf[i].st_id[0])
	 && (stbuf[i].st_hash == hash)) {
	    register int n;

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
STATIC int
stidhash(MessageID)
    char                *MessageID;
{
    register char *p;
    register int hash;

    hash = 0;
    for (p = MessageID + 1; *p && (*p != '>'); p++) {
	hash <<= 1;
	if (isascii(*p) && isupper(*p)) {
	    hash += tolower(*p);
	} else {
	    hash += *p;
	}
    }
    return hash;
}

/* stalloc(): save path, ID, and qp into one of the streaming mode entries */
STATIC int
stalloc(Article, MessageID, qp, hash)
    char		*Article;
    char		*MessageID;
    QIOSTATE		*qp;
    int			hash;
{
    register int i;

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
    stbuf[i].st_qp = qp;
    stbuf[i].st_hash = hash;
    stbuf[i].st_retry = 0;
    stbuf[i].st_age = 0;
    stnq++;
    return i;
}

/* strel(): release for reuse one of the streaming mode entries */
STATIC void
strel(i)
    int		i;
{
	if (stbuf[i].st_qp) {
	    QIOclose(stbuf[i].st_qp);
	    stbuf[i].st_qp = 0;
	}
	if (stbuf[i].st_id) stbuf[i].st_id[0] = '\0';
	if (stbuf[i].st_fname) stbuf[i].st_fname[0] = '\0';
	stnq--;
}

/*
**  Send a line to the server, adding the dot escape and \r\n.
*/
STATIC BOOL
REMwrite(p, i, escdot)
    register char	*p;
    register int	i;
    register BOOL	escdot;
{
    static char		HDR[] = "Content-Transfer-Encoding:";
    static char		COD[] =
		"Content-Transfer-Encoding: quoted-printable\r\n";
    int			size;

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

    if (MimeArticle != MTnotmime)
        if ((*p == 'C' && EQn(p, HDR, STRLEN(HDR)))
         || ((*p == 'C' || *p == 'c') && caseEQn(p, HDR, STRLEN(HDR)))) {
	    (void)memcpy((POINTER)REMbuffptr, (POINTER)COD, STRLEN(COD));
	    REMbuffptr += STRLEN(COD);
	    return TRUE;
        }

    /* Dot escape, text of the line, line terminator. */
    if (escdot && (*p == '.'))
	*REMbuffptr++ = '.';
    (void)memcpy((POINTER)REMbuffptr, (POINTER)p, (SIZE_T)i);
    REMbuffptr += i;
    *REMbuffptr++ = '\r';
    *REMbuffptr++ = '\n';

    return TRUE;
}


/*
**  Send a line to the server, adding the dot escape and \r\n.
*/
STATIC BOOL
REMwriteQuoted(p, i)
    register char	*p;
    register int	i;
{
    static char		HEXDIGITS[] = "0123456789ABCDEF";
    register char	*dest;
    register int	size;
    register int	count;
    register int	prev;

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

    for (count = 0, prev = 255, dest = REMbuffptr, i++; --i > 0; ) {
	if ((*p < 32 && *p != '\t')
	 || *p == '='
	 || *p >= 127
	 || (count == 0 && *p =='.')) {
	    *dest++ = '=';
	    *dest++ = HEXDIGITS[*p >> 4];
	    *dest++ = HEXDIGITS[*p & 0x0F];
	    p++;
	    count += 3;
	    prev = 'A';
	}
	else {
	    prev = *dest++ = *p++;
	    count++;
	}
        if (count > 72) {
	    *dest++ = '=';
	    *dest++ = '\r';
	    *dest++ = '\n';
	    count = 0;
	    prev = '\n';
        }
    }
    if (prev == ' ' || prev == '\t')
	*dest++ = '=';

    REMbuffptr = dest;
    *REMbuffptr++ = '\r';
    *REMbuffptr++ = '\n';

    return TRUE;
}


/*
**  Print transfer statistics, clean up, and exit.
*/
STATIC NORETURN
ExitWithStats(x)
    int			x;
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
	(void)printf(STAT1,
	    REMhost, STAToffered, STATaccepted, STATrefused, STATrejected);
	(void)printf("\n");
	(void)printf(STAT2, REMhost, usertime, systime, STATend - STATbegin);
	(void)printf("\n");
    }

    syslog(L_NOTICE, STAT1,
	REMhost, STAToffered, STATaccepted, STATrefused, STATrejected);
    syslog(L_NOTICE, STAT2, REMhost, usertime, systime, STATend - STATbegin);
    if (retries)
	syslog(L_NOTICE, "%s %lu Streaming retries", REMhost, retries);

    if (BATCHfp != NULL && unlink(BATCHtemp) < 0 && errno != ENOENT)
	(void)fprintf(stderr, "Can't remove \"%s\", %s\n",
		BATCHtemp, strerror(errno));
    (void)sleep(1);
    SMshutdown();
    exit(x);
    /* NOTREACHED */
}


/*
**  Close the batchfile and the temporary file, and rename the temporary
**  to be the batchfile.
*/
STATIC void
CloseAndRename()
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
STATIC void
Requeue(Article, MessageID)
    char	*Article;
    char	*MessageID;
{
    /* Temp file already open? */
    if (BATCHfp == NULL) {
	(void)mktemp(BATCHtemp);
	if ((BATCHfp = fopen(BATCHtemp, "w")) == NULL) {
	    (void)fprintf(stderr, "Can't open \"%s\", %s\n",
		    BATCHtemp, strerror(errno));
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
STATIC void
RequeueRestAndExit(Article, MessageID)
    char		*Article;
    char		*MessageID;
{
    register char	*p;

    if (!AlwaysRewrite
     && STATaccepted == 0 && STATrejected == 0 && STATrefused == 0) {
	(void)fprintf(stderr, "Nothing sent -- leaving batchfile alone.\n");
	ExitWithStats(1);
    }

    (void)fprintf(stderr, "Rewriting batch file and exiting.\n");
    if (CanStream) {	/* streaming mode has a buffer of articles */
	register int i;

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
STATIC char *
REMclean(buff)
    char	*buff;
{
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
STATIC BOOL
REMread(start, size)
    char		*start;
    int			size;
{
    static int		count;
    static char		buffer[BUFSIZ];
    static char		*bp;
    register char	*p;
    register char	*q;
    register char	*end;
    struct timeval	t;
    FDSET		rmask;
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
	    i = select(FromServer + 1, &rmask, (FDSET *)NULL,
			(FDSET *)NULL, &t);
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
Interrupted(Article, MessageID)
    char	*Article;
    char	*MessageID;
{
    (void)fprintf(stderr, "Interrupted\n");
    RequeueRestAndExit(Article, MessageID);
}


/*
**  Send a whole article to the server.
*/
STATIC BOOL
REMsendarticle(Article, MessageID, qp)
    char		*Article;
    char		*MessageID;
    register QIOSTATE	*qp;
{
    register char	*p;
    register BOOL	ok;
    register BOOL	InHeaders;
    long		length;
    char		buff[NNTP_STRLEN];

    length = 0;
    for (InHeaders = TRUE; ; ) {
	if ((p = QIOread(qp)) == NULL) {
	    if (QIOtoolong(qp)) {
		(void)fprintf(stderr, "Line too long in \"%s\"\n", Article);
		(void)QIOread(qp);
		continue;
	    }
	    if (QIOerror(qp)) {
		(void)fprintf(stderr, "Can't read \"%s\", %s\n",
			Article, strerror(errno));
		return FALSE;
	    }

	    /* Normal EOF. */
	    break;
	}
	if (*p == '\0')
	    InHeaders = FALSE;

	if (InHeaders || MimeArticle == MTnotmime) {
	    if (!REMwrite(p, QIOlength(qp), TRUE)) {
	        (void)fprintf(stderr, "Can't send \"%s\", %s\n",
		        Article, strerror(errno));
	        return FALSE;
	    }
	    length += QIOlength(qp);
	}
	else {
	    switch (MimeArticle) {
	    default:
	    case MTbase64:
		ok = FALSE;
		break;
	    case MTquotedprintable:
		ok = REMwriteQuoted(p, QIOlength(qp));
		length += QIOlength(qp);
		break;
	    }
	    if (!ok) {
		(void)fprintf(stderr, "Can't send \"%s\", %s\n",
			Article, strerror(errno));
		return FALSE;
	    }
	}
	if (GotInterrupt)
	    Interrupted(Article, MessageID);
    }
    /* Write the terminator. */
    if (!REMwrite(".", 1, FALSE)) {
	(void)fprintf(stderr, "Can't send \"%s\", %s\n",
		Article, strerror(errno));
	return FALSE;
    }
    if (Debug)
	(void)fprintf(stderr, "> [ article %ld ]%s\n", length,
	     MimeArticle == MTnotmime ? "" : " (Mime: quoted-printable)");
    if (GotInterrupt)
	Interrupted(Article, MessageID);
    if (Debug)
	(void)fprintf(stderr, "> .\n");

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
#if 0
	syslog(L_NOTICE, GOT_RESENDIT, REMhost, MessageID, REMclean(buff));
#endif
	Requeue(Article, MessageID);
	break;
    case NNTP_TOOKIT_VAL:
	STATaccepted++;
	break;
    case NNTP_REJECTIT_VAL:
        if (logRejects)
            syslog(L_NOTICE, REJECTED, REMhost,
                   MessageID, Article, REMclean(buff));
	STATrejected++;
	break;
    }

    /* Article sent, or we requeued it. */
    return TRUE;
}


/*
**  Get the Message-ID header from an open article.
*/
STATIC char *
GetMessageID(qp)
    register QIOSTATE	*qp;
{
    static char		HDR[] = "Message-ID:";
    static char		*buff;
    static int          buffsize = 0;
    register char	*p;

    while ((p = QIOread(qp)) != NULL)
	if ((*p == 'M' && EQn(p, HDR, STRLEN(HDR)))
	 || ((*p == 'M' || *p == 'm') && caseEQn(p, HDR, STRLEN(HDR)))) {
	    /* Found the header -- skip whitespace. */
	    for (p += STRLEN(HDR); ISWHITE(*p); p++)
		continue;
	    if (*p == '\0')
		/* Header is empty*/
		break;
	    if (strlen(p) >= buffsize) {
		if (buffsize)
		    buff = RENEW(buff, char, strlen(p) + 1);
		else
		    buff = NEW(char, strlen(p) + 1);
		
	    }
	    (void)strcpy(buff, p);
	    return buff;
	}
    return NULL;
}


/*
**  Get the MIME Content headers from an open article.
*/
STATIC void
GetMimeHeaders(qp, Encodingp, Typep)
    register QIOSTATE	*qp;
    register char	**Encodingp;
    register char	**Typep;
{
    static char		ENC_HDR[] = "Content-Transfer-Encoding:";
    static char		TYPE_HDR[] = "Content-Type:";
    static char		Encoding[SMBUF + 1];
    static char		ContentType[SMBUF + 1];
    register char	*p;

    for (*Encodingp = *Typep = NULL; (p = QIOread(qp)) != NULL && *p; ) {
	if (*p != 'C' && *p != 'c')
	    continue;
	if (caseEQn(p, ENC_HDR, STRLEN(ENC_HDR))) {
	    for (p += STRLEN(ENC_HDR); ISWHITE(*p); p++)
		continue;
	    if (*p == '\0' || (int)strlen(p) > sizeof Encoding)
		/* Header is empty or too long. */
		continue;
	    (void)strcpy(Encoding, p);
	    *Encodingp = Encoding;
	    if (*Typep)
		break;
	}
	else if (caseEQn(p, TYPE_HDR, STRLEN(TYPE_HDR))) {
	    for (p += STRLEN(TYPE_HDR); ISWHITE(*p); p++)
		continue;
	    if (*p == '\0' || (int)strlen(p) > sizeof ContentType)
		/* Header is empty or too long. */
		break;
	    (void)strcpy(ContentType, p);
	    /* Strip off any subtype part. */
	    for (p = ContentType; *p; p++)
		if (*p == '/' || *p == ';') {
		    *p = '\0';
		    break;
		}
	    *Typep = ContentType;
	    if (*Encodingp)
		break;
	}
    }
}



/*
**  Mark that we got interrupted.
*/
STATIC SIGHANDLER
CATCHinterrupt(s)
    int		s;
{
    GotInterrupt = TRUE;
    /* Let two interrupts kill us. */
    (void)signal(s, SIG_DFL);
}


/*
**  Mark that the alarm went off.
*/
/* ARGSUSED0 */
STATIC SIGHANDLER
CATCHalarm(s)
    int		s;
{
    GotAlarm = TRUE;
    if (JMPyes)
	longjmp(JMPwhere, 1);
}

/* check articles in streaming NNTP mode
** return TRUE on failure.
*/
STATIC BOOL
check(i)
    int	i; /* index of stbuf to send check for */
{
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
STATIC BOOL
takethis(i)
    int i;	/* index to stbuf to be sent */
{
    char	buff[NNTP_STRLEN];

    if (!stbuf[i].st_qp) { /* should already be open but ... */
	/* Open the article. */
	if (!(stbuf[i].st_qp = QIOopen(stbuf[i].st_fname))) {
	    /* can not open it. Should check AltPath */
	    if (AltPath && (*(stbuf[i].st_fname) != '/')) {
		(void)sprintf(AltPath, "%s/%s", AltSpool, stbuf[i].st_fname);
		stbuf[i].st_qp = QIOopen(AltPath);
	    }
	    if (!(stbuf[i].st_qp)) {
		strel(i);
		return FALSE; /* Not an error. Could be canceled or expired */
	    }
	}
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
    if (!REMsendarticle(stbuf[i].st_fname, stbuf[i].st_id,
	    stbuf[i].st_qp))
	return TRUE;
    QIOclose(stbuf[i].st_qp);	/* should not need file again */
    stbuf[i].st_qp = 0;		/* so close to free descriptor */
    stbuf[i].st_age = 0;
    /* That all.  Response is checked later by strlisten() */
    return FALSE;
}


/* listen for responses.  Process acknowledgments to remove items from
** the queue.  Also sends the articles on request.  Returns TRUE on error.
** return TRUE on failure.
*/
STATIC BOOL
strlisten()
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
	    strel(i); /* release entry */
	    STATaccepted++;
	    break;
		
	case NNTP_ERR_FAILID_VAL:
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
STATIC NORETURN
Usage()
{
    (void)fprintf(stderr,
	"Usage: innxmit [-a] [-c] [-d] [-M] [-p] [-r] [-s] [-t#] [-T#] host file\n");
    exit(1);
}


int main(int ac, char *av[])
{
    static char		SKIPPING[] = "Skipping \"%s\" --%s?\n";
    int	                i;
    char	        *p;
    QIOSTATE	        *qp;
    TIMEINFO		Now;
    FILE		*From;
    FILE		*To;
    char		buff[8192+128];
    char		*Article;
    char		*ContentEncoding;
    char		*ContentType;
    char		*MessageID;
    SIGHANDLER		(*old)();
    unsigned int	ConnectTimeout;
    unsigned int	TotalTimeout;
    int                 port = NNTP_PORT;

    /* Set defaults. */
    if (ReadInnConf() < 0) exit(1);

    ConnectTimeout = 0;
    TotalTimeout = 0;
    AltSpool = NULL;
    
    (void)umask(NEWSUMASK);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "lA:acdMprst:T:vP:")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'A':
	    AltSpool = optarg;
	    AltPath = NEW(char, SPOOLNAMEBUFF + strlen(AltSpool));
	    break;
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
        case 'l':
            logRejects = FALSE ;
            break ;
	case 'M':
	    Mime = TRUE;
	    break;
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

    (void)openlog("innxmit", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);

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
    if (LockFile(QIOfileno(BATCHqp), TRUE) < 0) {
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
	    old = signal(SIGALRM, CATCHalarm);
	    (void)alarm(ConnectTimeout);
	    JMPyes = TRUE;
	    if (setjmp(JMPwhere)) {
		(void)fprintf(stderr, "Can't connect to %s, timed out\n",
			REMhost);
		SMshutdown();
		exit(1);
	    }
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
	    (void)signal(SIGALRM, old);
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
		int i;

		for (i = 0; i < STNBUF; i++) { /* reset buffers */
		    stbuf[i].st_fname = 0;
		    stbuf[i].st_id = 0;
		    stbuf[i].st_qp = 0;
		}
		stnq = 0;
	    }
	}
    }

    /* Set up signal handlers. */
    (void)signal(SIGHUP, CATCHinterrupt);
    (void)signal(SIGINT, CATCHinterrupt);
    (void)signal(SIGTERM, CATCHinterrupt);
    (void)signal(SIGPIPE, SIG_IGN);
    if (TotalTimeout) {
	(void)alarm(TotalTimeout);
	(void)signal(SIGALRM, CATCHalarm);
    }

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

        /*
         * If the IHAVE plus the "message-id", separating space and trailing
	 * CR-NL will exceed the maximum command length permitted by the RFC
	 * (i.e. NNTP_STRLEN), then reject the article and continue to avoid
	 * overrunning buffers and throwing the server on the recieving end a
	 * blow from behind.  
         */
        if (MessageID != NULL && (strlen(MessageID) > NNTP_STRLEN - 12)) {
            (void)fprintf(stderr, "Dropping article in \"%s\" - long message id \"%s\"\n",
                          BATCHname, MessageID);
            continue;
        }

	/* Open the article. */
	if ((qp = QIOopen(Article)) == NULL
	 && AltSpool
	 && *Article != '/') {
	    (void)sprintf(AltPath, "%s/%s", AltSpool, Article);
	    qp = QIOopen(AltPath);
	}

	if (qp == NULL) {
	    if (IsToken(Article)) {
		if ((SMerrno == SMERR_NOENT) || (SMerrno == SMERR_UNINIT))
		    continue;
		else {
		    (void)fprintf(stderr, "Requeue \"%s\", %s\n",
			Article, SMerrorstr);
		    Requeue(Article, MessageID);
		}
	    }
	    switch (errno) {
	    default:
		(void)fprintf(stderr, "Requeue \"%s\", %s\n",
			Article, strerror(errno));
		Requeue(Article, MessageID);
		break;
	    case ENOENT:
		/* Cancelled or expired.  We could look the file up
		 * in the history database and see if it does exist. */
		break;
	    case ENOTDIR:
		(void)fprintf(stderr, SKIPPING, Article, "mangled");
		break;
	    }
	    continue;
	}

	if (Purging) {
	    QIOclose(qp);
	    Requeue(Article, MessageID);
	    continue;
	}

	/* Get the Message-ID from the article if we need to. */
	if (MessageID == NULL) {
	    if ((MessageID = GetMessageID(qp)) == NULL) {
		(void)fprintf(stderr, SKIPPING, Article, "no Message-ID");
		QIOclose(qp);
		continue;
	    }
	    if (QIOrewind(qp) < 0) {
		(void)fprintf(stderr, "Can't rewind \"%s\", %s -- requeue\n",
			Article, strerror(errno));
		QIOclose(qp);
		Requeue(Article, (char *)NULL);
		continue;
	    }
	}
	if (Mime == TRUE) {
	    MimeArticle = MTnotmime;
	    GetMimeHeaders(qp, &ContentEncoding, &ContentType);
	    if (QIOrewind(qp) < 0) {
		(void)fprintf(stderr, "Can't rewind \"%s\", %s -- requeue\n",
			Article, strerror(errno));
		QIOclose(qp);
		Requeue(Article, (char *)NULL);
		continue;
	    }
	    if (ContentEncoding 
	     && (caseEQ(ContentEncoding, "binary")
	      || caseEQ(ContentEncoding, "8bit")))
		if (ContentType == NULL || caseEQ(ContentType, "text"))
		    MimeArticle = MTquotedprintable;
		else
		    /* Shouldbe MTbase64, but not implemented yet. */
		    MimeArticle = MTnotmime;
	}
	if (GotInterrupt)
	    Interrupted(Article, MessageID);

	/* Offer the article. */
	if (CanStream) {
	    int i;
	    int lim;
	    int hash;

	    hash = stidhash(MessageID);
	    if (stindex(MessageID, hash) >= 0) { /* skip duplicates in queue */
		if (Debug)
		    (void)fprintf(stderr, "Skipping duplicate ID %s\n",
							    MessageID);
		QIOclose(qp);
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
	    i = stalloc(Article, MessageID, qp, hash);
	    if (i < 0) {
		QIOclose(qp);
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
	    QIOclose(qp);
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
	    QIOclose(qp);
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
	    if (!REMsendarticle(Article, MessageID, qp))
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

	QIOclose(qp);
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
}
