/*  $Id$
**
**  Article-related routines.
*/

#include "config.h"
#include "clibrary.h"
#include <assert.h>
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#include <sys/uio.h>
#include <ctype.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/wire.h"
#include "nnrpd.h"
#include "inn/ov.h"
#include "tls.h"
#include "cache.h"

#ifdef HAVE_SSL
extern SSL *tls_conn;
#endif 

/*
**  Data structures for use in ARTICLE/HEAD/BODY/STAT common code.
*/
typedef enum _SENDTYPE {
    STarticle,
    SThead,
    STbody,
    STstat
} SENDTYPE;

typedef struct _SENDDATA {
    SENDTYPE    Type;
    int         ReplyCode;
    const char *Item;
} SENDDATA;

static ARTHANDLE        *ARThandle = NULL;
static SENDDATA		SENDbody = {
    STbody,	NNTP_OK_BODY,		"body"
};
static SENDDATA		SENDarticle = {
    STarticle,	NNTP_OK_ARTICLE,	"article"
};
static SENDDATA		SENDstat = {
    STstat,     NNTP_OK_STAT,           "status"
};
static SENDDATA		SENDhead = {
    SThead,	NNTP_OK_HEAD,		"head"
};

static struct iovec	iov[IOV_MAX > 1024 ? 1024 : IOV_MAX];
static int		queued_iov = 0;

static void
PushIOvHelper(struct iovec* vec, int* countp)
{
    int result = 0;

    TMRstart(TMR_NNTPWRITE);

#ifdef HAVE_SASL
    if (sasl_conn && sasl_ssf) {
	int i;

	for (i = 0; i < *countp; i++) {
	    write_buffer(vec[i].iov_base, vec[i].iov_len);
	}
    } else {
#endif /* HAVE_SASL */

#ifdef HAVE_SSL
	if (tls_conn) {
Again:
	    result = SSL_writev(tls_conn, vec, *countp);
	    switch (SSL_get_error(tls_conn, result)) {
	    case SSL_ERROR_NONE:
	    case SSL_ERROR_SYSCALL:
		break;
	    case SSL_ERROR_WANT_WRITE:
		goto Again;
		break;
	    case SSL_ERROR_SSL:
		SSL_shutdown(tls_conn);
		tls_conn = NULL;
		errno = ECONNRESET;
		break;
	    case SSL_ERROR_ZERO_RETURN:
		break;
	    }
	} else
#endif /* HAVE_SSL */
	    result = xwritev(STDOUT_FILENO, vec, *countp);

#ifdef HAVE_SASL
    }
#endif

    TMRstop(TMR_NNTPWRITE);

    if (result == -1) {
	/* We can't recover, since we can't resynchronise with our
	 * peer. */
	ExitWithStats(1, true);
    }
    *countp = 0;
}

static void
PushIOvRateLimited(void)
{
    double              start, end, elapsed, target;
    struct iovec        newiov[IOV_MAX > 1024 ? 1024 : IOV_MAX];
    int                 newiov_len;
    int                 sentiov;
    int                 i;
    int                 bytesfound;
    int                 chunkbittenoff;
    struct timeval      waittime;

    while (queued_iov) {
	bytesfound = newiov_len = 0;
	sentiov = 0;
	for (i = 0; (i < queued_iov) && (bytesfound < MaxBytesPerSecond); i++) {
	    if ((signed)iov[i].iov_len + bytesfound > MaxBytesPerSecond) {
		chunkbittenoff = MaxBytesPerSecond - bytesfound;
		newiov[newiov_len].iov_base = iov[i].iov_base;
		newiov[newiov_len++].iov_len = chunkbittenoff;
		iov[i].iov_base = (char *)iov[i].iov_base + chunkbittenoff;
		iov[i].iov_len -= chunkbittenoff;
		bytesfound += chunkbittenoff;
	    } else {
		newiov[newiov_len++] = iov[i];
		sentiov++;
		bytesfound += iov[i].iov_len;
	    }
	}
	assert(sentiov <= queued_iov);
        start = TMRnow_double();
	PushIOvHelper(newiov, &newiov_len);
        end = TMRnow_double();
        target = (double) bytesfound / MaxBytesPerSecond;
        elapsed = end - start;
        if (elapsed < 1 && elapsed < target) {
            waittime.tv_sec = 0;
            waittime.tv_usec = (target - elapsed) * 1e6;
            start = TMRnow_double();
	    if (select(0, NULL, NULL, NULL, &waittime) != 0)
                syswarn("%s: select in PushIOvRateLimit failed", Client.host);
            end = TMRnow_double();
            IDLEtime += end - start;
	}
	memmove(iov, &iov[sentiov], (queued_iov - sentiov) * sizeof(struct iovec));
	queued_iov -= sentiov;
    }
}

static void
PushIOv(void)
{
    TMRstart(TMR_NNTPWRITE);
    fflush(stdout);
    TMRstop(TMR_NNTPWRITE);
    if (MaxBytesPerSecond != 0)
	PushIOvRateLimited();
    else
	PushIOvHelper(iov, &queued_iov);
}

static void
SendIOv(const char *p, int len)
{
    char                *q;

    if (queued_iov) {
	q = (char *)iov[queued_iov - 1].iov_base + iov[queued_iov - 1].iov_len;
	if (p == q) {
	    iov[queued_iov - 1].iov_len += len;
	    return;
	}
    }
    iov[queued_iov].iov_base = (char*)p;
    iov[queued_iov++].iov_len = len;
    if (queued_iov == IOV_MAX)
	PushIOv();
}

static char		*_IO_buffer_ = NULL;
static int		highwater = 0;

static void
PushIOb(void)
{
    write_buffer(_IO_buffer_, highwater);
    highwater = 0;
}

static void
SendIOb(const char *p, int len)
{
    int tocopy;
    
    if (_IO_buffer_ == NULL)
        _IO_buffer_ = xmalloc(BIG_BUFFER);

    while (len > 0) {
        tocopy = (len > (BIG_BUFFER - highwater)) ? (BIG_BUFFER - highwater) : len;
        memcpy(&_IO_buffer_[highwater], p, tocopy);
        p += tocopy;
        highwater += tocopy;
        len -= tocopy;
        if (highwater == BIG_BUFFER)
            PushIOb();
    }
}


/*
**  If we have an article open, close it.
*/
void
ARTclose(void)
{
    if (ARThandle) {
	SMfreearticle(ARThandle);
	ARThandle = NULL;
    }
}

bool
ARTinstorebytoken(TOKEN token)
{
    ARTHANDLE *art;
    struct timeval	stv, etv;

    if (PERMaccessconf->nnrpdoverstats) {
	gettimeofday(&stv, NULL);
    }
    art = SMretrieve(token, RETR_STAT);
    /* XXX This isn't really overstats, is it? */
    if (PERMaccessconf->nnrpdoverstats) {
	gettimeofday(&etv, NULL);
	OVERartcheck+=(etv.tv_sec - stv.tv_sec) * 1000;
	OVERartcheck+=(etv.tv_usec - stv.tv_usec) / 1000;
    }
    if (art) {
	SMfreearticle(art);
	return true;
    } 
    return false;
}

/*
**  If the article name is valid, open it and stuff in the ID.
*/
static bool
ARTopen(ARTNUM artnum)
{
    static ARTNUM	save_artnum;
    TOKEN		token;

    /* Re-use article if it's the same one. */
    if (save_artnum == artnum) {
	if (ARThandle)
	    return true;
    }
    ARTclose();

    if (!OVgetartinfo(GRPcur, artnum, &token))
	return false;
  
    TMRstart(TMR_READART);
    ARThandle = SMretrieve(token, RETR_ALL);
    TMRstop(TMR_READART);
    if (ARThandle == NULL) {
	return false;
    }

    save_artnum = artnum;
    return true;
}


/*
**  Open the article for a given message-ID.
*/
static bool
ARTopenbyid(char *msg_id, ARTNUM *ap, bool final)
{
    TOKEN token;

    *ap = 0;
    token = cache_get(HashMessageID(msg_id), final);
    if (token.type == TOKEN_EMPTY) {
	if (History == NULL) {
	    time_t statinterval;

	    /* Do lazy opens of the history file:  lots of clients
	     * will never ask for anything by message-ID, so put off
	     * doing the work until we have to. */
	    History = HISopen(HISTORY, innconf->hismethod, HIS_RDONLY);
	    if (!History) {
		syslog(L_NOTICE, "can't initialize history");
		Reply("%d NNTP server unavailable; try later\r\n",
		      NNTP_FAIL_TERMINATING);
		ExitWithStats(1, true);
	    }
	    statinterval = 30;
	    HISctl(History, HISCTLS_STATINTERVAL, &statinterval);
	}
	if (!HISlookup(History, msg_id, NULL, NULL, NULL, &token))
	    return false;
    }
    if (token.type == TOKEN_EMPTY)
	return false;
    TMRstart(TMR_READART);
    ARThandle = SMretrieve(token, RETR_ALL);
    TMRstop(TMR_READART);
    if (ARThandle == NULL) {
	return false;
    }

    return true;
}

/*
**  Send a (part of) a file to stdout, doing newline and dot conversion.
*/
static void
ARTsendmmap(SENDTYPE what)
{
    const char		*p, *q, *r;
    const char		*s, *path, *xref, *endofpath;
    char		lastchar;

    ARTcount++;
    GRParticles++;
    lastchar = -1;

    /* Get the headers and detect if wire format. */
    if (what == STarticle) {
	q = ARThandle->data;
	p = ARThandle->data + ARThandle->len;
     } else {
	for (q = p = ARThandle->data; p < (ARThandle->data + ARThandle->len); p++) {
	    if (*p == '\r')
		continue;
	    if (*p == '\n') {
		if (lastchar == '\n') {
		    if (what == SThead) {
			if (*(p-1) == '\r')
			    p--;
			break;
		    } else {
			q = p + 1;
			p = ARThandle->data + ARThandle->len;
			break;
		    }
		}
	    }
	    lastchar = *p;
	}
    }

    /* q points to the start of the article buffer, p to the end of it. */
    if (VirtualPathlen > 0 && (what != STbody)) {
        path = wire_findheader(ARThandle->data, ARThandle->len, "Path", true);
        if (path == NULL) {
	    SendIOv(".\r\n", 3);
	    ARTgetsize += 3;
	    PushIOv();
	    ARTget++;
	    return;
	} else {
            xref = wire_findheader(ARThandle->data, ARThandle->len, "Xref", true);
            if (xref == NULL) {
                SendIOv(".\r\n", 3);
                ARTgetsize += 3;
                PushIOv();
                ARTget++;
                return;
            }
        }
        endofpath = wire_endheader(path, ARThandle->data + ARThandle->len - 1);
        if (endofpath == NULL) {
	    SendIOv(".\r\n", 3);
	    ARTgetsize += 3;
	    PushIOv();
	    ARTget++;
	    return;
	}
	if ((r = memchr(xref, ' ', p - xref)) == NULL || r == p) {
	    SendIOv(".\r\n", 3);
	    ARTgetsize += 3;
	    PushIOv();
	    ARTget++;
	    return;
	}
	/* r points to the first space in the Xref: header. */

	for (s = path, lastchar = '\0';
	    s + VirtualPathlen + 1 < endofpath;
	    lastchar = *s++) {
	    if ((lastchar != '\0' && lastchar != '!') || *s != *VirtualPath ||
		strncasecmp(s, VirtualPath, VirtualPathlen - 1) != 0)
		continue;
	    if (*(s + VirtualPathlen - 1) != '\0' &&
		*(s + VirtualPathlen - 1) != '!')
		continue;
	    break;
	}
	if (s + VirtualPathlen + 1 < endofpath) {
	    if (xref > path) {
	        SendIOv(q, path - q);
	        SendIOv(s, xref - s);
	        SendIOv(VirtualPath, VirtualPathlen - 1);
	        SendIOv(r, p - r);
	    } else {
	        SendIOv(q, xref - q);
	        SendIOv(VirtualPath, VirtualPathlen - 1);
	        SendIOv(r, path - r);
	        SendIOv(s, p - s);
	    }
	} else {
            /* Double the '!' (thus, adding one '!') in Path: header. */
	    if (xref > path) {
	        SendIOv(q, path - q);
	        SendIOv(VirtualPath, VirtualPathlen);
                SendIOv("!", 1);
	        SendIOv(path, xref - path);
	        SendIOv(VirtualPath, VirtualPathlen - 1);
	        SendIOv(r, p - r);
	    } else {
	        SendIOv(q, xref - q);
	        SendIOv(VirtualPath, VirtualPathlen - 1);
	        SendIOv(r, path - r);
	        SendIOv(VirtualPath, VirtualPathlen);
                SendIOv("!", 1);
	        SendIOv(path, p - path);
	    }
	}
    } else
	SendIOv(q, p - q);
    ARTgetsize += p - q;
    if (what == SThead) {
	SendIOv(".\r\n", 3);
	ARTgetsize += 3;
    } else if (memcmp((ARThandle->data + ARThandle->len - 5), "\r\n.\r\n", 5)) {
	if (memcmp((ARThandle->data + ARThandle->len - 2), "\r\n", 2)) {
	    SendIOv("\r\n.\r\n", 5);
	    ARTgetsize += 5;
	} else {
	    SendIOv(".\r\n", 3);
	    ARTgetsize += 3;
	}
    }
    PushIOv();

    ARTget++;
}

/*
**  Return the header from the specified file, or NULL if not found.
*/
char *
GetHeader(const char *header, bool stripspaces)
{
    const char		*p, *q, *r, *s, *t;
    char		*w, *wnew, prevchar;
    /* Bogus value here to make sure that it isn't initialized to \n. */
    char		lastchar = ' ';
    const char		*limit;
    const char		*cmplimit;
    static char		*retval = NULL;
    static int		retlen = 0;
    int			headerlen;
    bool		pathheader = false;
    bool		xrefheader = false;

    limit = ARThandle->data + ARThandle->len;
    cmplimit = ARThandle->data + ARThandle->len - strlen(header) - 1;
    for (p = ARThandle->data; p < cmplimit; p++) {
	if (*p == '\r')
	    continue;
	if ((lastchar == '\n') && (*p == '\n')) {
	    return NULL;
	}
	if ((lastchar == '\n') || (p == ARThandle->data)) {
	    headerlen = strlen(header);
	    if (strncasecmp(p, header, headerlen) == 0 && p[headerlen] == ':' &&
                ISWHITE(p[headerlen+1])) {
                p += headerlen + 2;
                if (stripspaces) {
                    for (; (p < limit) && ISWHITE(*p) ; p++);
                }
		for (q = p; q < limit; q++) 
		    if ((*q == '\r') || (*q == '\n')) {
			/* Check for continuation header lines. */
			t = q + 1;
			if (t < limit) {
			    if ((*q == '\r' && *t == '\n')) {
				t++;
				if (t == limit)
				    break;
			    }
			    if ((*t == '\t' || *t == ' ')) {
				for (; (t < limit) && ISWHITE(*t) ; t++);
				q = t;
			    } else {
				break;
			    }
			} else {
			    break;
			}
		    }
		if (q == limit)
		    return NULL;
		if (strncasecmp("Path", header, headerlen) == 0)
		    pathheader = true;
		else if (strncasecmp("Xref", header, headerlen) == 0)
		    xrefheader = true;
		if (retval == NULL) {
                    /* Possibly add '!' (a second one) at the end of the virtual path.
                     * So it is +2 because of '\0'. */
		    retlen = q - p + VirtualPathlen + 2;
		    retval = xmalloc(retlen);
		} else {
		    if ((q - p + VirtualPathlen + 2) > retlen) {
			retlen = q - p + VirtualPathlen + 2;
                        retval = xrealloc(retval, retlen);
		    }
		}
		if (pathheader && (VirtualPathlen > 0)) {
                    const char *endofpath;
                    const char *endofarticle;

                    endofarticle = ARThandle->data + ARThandle->len - 1;
                    endofpath = wire_endheader(p, endofarticle);
                    if (endofpath == NULL)
                        return NULL;
		    for (s = p, prevchar = '\0';
			s + VirtualPathlen + 1 < endofpath;
			prevchar = *s++) {
			if ((prevchar != '\0' && prevchar != '!') ||
			    *s != *VirtualPath ||
			    strncasecmp(s, VirtualPath, VirtualPathlen - 1) != 0)
			    continue;
			if (*(s + VirtualPathlen - 1) != '\0' &&
			    *(s + VirtualPathlen - 1) != '!')
			    continue;
			break;
		    }
		    if (s + VirtualPathlen + 1 < endofpath) {
			memcpy(retval, s, q - s);
			*(retval + (int)(q - s)) = '\0';
		    } else {
			memcpy(retval, VirtualPath, VirtualPathlen);
                        *(retval + VirtualPathlen) = '!';
			memcpy(retval + VirtualPathlen + 1, p, q - p);
			*(retval + (int)(q - p) + VirtualPathlen + 1) = '\0';
		    }
		} else if (xrefheader && (VirtualPathlen > 0)) {
		    if ((r = memchr(p, ' ', q - p)) == NULL)
			return NULL;
		    for (; (r < q) && isspace((unsigned char) *r) ; r++);
		    if (r == q)
			return NULL;
                    /* Copy the virtual path without its final '!'. */
		    memcpy(retval, VirtualPath, VirtualPathlen - 1);
		    memcpy(retval + VirtualPathlen - 1, r - 1, q - r + 1);
		    *(retval + (int)(q - r) + VirtualPathlen) = '\0';
		} else {
		    memcpy(retval, p, q - p);
		    *(retval + (int)(q - p)) = '\0';
		}
		for (w = retval, wnew = retval; *w; w++) {
                    if (*w == '\r' && w[1] == '\n') {
                        w++;
                        continue;
                    }
		    if (*w == '\0' || *w == '\t' || *w == '\r' || *w == '\n') {
			*wnew = ' ';
                    } else {
                        *wnew = *w;
                    }
                    wnew++;
                }
                *wnew = '\0';
		return retval;
	    }
	}
	lastchar = *p;
    }
    return NULL;
}

/*
**  Fetch part or all of an article and send it to the client.
*/
void
CMDfetch(int ac, char *av[])
{
    char		buff[SMBUF];
    SENDDATA		*what;
    bool                mid, ok;
    ARTNUM		art;
    char		*msgid;
    ARTNUM		tart;
    bool                final = false;

    mid = (ac > 1 && IsValidMessageID(av[1], true));

    /* Check the syntax of the arguments first. */
    if (ac > 1 && !IsValidArticleNumber(av[1])) {
        /* It is better to check for a number before a message-ID because
         * '<' could have been forgotten and the error message should then
         * report a syntax error in the message-ID. */
        if (isdigit((unsigned char) av[1][0])) {
            Reply("%d Syntax error in article number\r\n", NNTP_ERR_SYNTAX);
            return;
        } else if (!mid) {
            Reply("%d Syntax error in message-ID\r\n", NNTP_ERR_SYNTAX);
            return;
        }
    }

    /* Find what to send; get permissions. */
    ok = PERMcanread;
    switch (*av[0]) {
    default:
	what = &SENDbody;
	final = true;
	break;
    case 'a': case 'A':
	what = &SENDarticle;
	final = true;
	break;
    case 's': case 'S':
	what = &SENDstat;
	break;
    case 'h': case 'H':
	what = &SENDhead;
	/* Poster might do a HEAD command to verify the article. */
	ok = PERMcanread || PERMcanpost;
	break;
    }

    /* Trying to read. */
    if (GRPcount == 0 && !mid) {
        Reply("%d Not in a newsgroup\r\n", NNTP_FAIL_NO_GROUP);
        return;
    }

    /* Check authorizations.  If an article number is requested
     * (not a message-ID), we check whether the group is still readable. */
    if (!ok || (!mid && PERMgroupmadeinvalid)) {
	Reply("%d Read access denied\r\n",
              PERMcanauthenticate ? NNTP_FAIL_AUTH_NEEDED : NNTP_ERR_ACCESS);
	return;
    }

    /* Requesting by message-ID? */
    if (mid) {
	if (!ARTopenbyid(av[1], &art, final)) {
	    Reply("%d No such article\r\n", NNTP_FAIL_MSGID_NOTFOUND);
	    return;
	}
	if (!PERMartok()) {
	    ARTclose();
	    Reply("%d Read access denied for this article\r\n",
                  PERMcanauthenticate ? NNTP_FAIL_AUTH_NEEDED : NNTP_ERR_ACCESS);
	    return;
	}
	tart=art;
	Reply("%d %lu %s %s\r\n", what->ReplyCode, art, av[1], what->Item);
	if (what->Type != STstat) {
	    ARTsendmmap(what->Type);
	}
	ARTclose();
	return;
    }

    /* Default is to get current article, or specified article. */
    if (ac == 1) {
        if (ARTnumber < ARTlow || ARTnumber > ARThigh) {
            Reply("%d Current article number %lu is invalid\r\n",
                  NNTP_FAIL_ARTNUM_INVALID, ARTnumber);
            return;
        }
        snprintf(buff, sizeof(buff), "%lu", ARTnumber);
        tart = ARTnumber;
    } else {
        /* We have already checked that the article number is valid. */
        strlcpy(buff, av[1], sizeof(buff));
        tart = (ARTNUM)atol(buff);
    }

    /* Open the article and send the reply. */
    if (!ARTopen(tart)) {
        Reply("%d No such article number %lu\r\n", NNTP_FAIL_ARTNUM_NOTFOUND, tart);
        return;
    }
    if ((msgid = GetHeader("Message-ID", true)) == NULL) {
        ARTclose();
        Reply("%d No such article number %lu\r\n", NNTP_FAIL_ARTNUM_NOTFOUND, tart);
        return;
    }
    if (ac > 1)
        ARTnumber = tart;

    /* A message-ID does not have more than 250 octets. */
    Reply("%d %s %.250s %s\r\n", what->ReplyCode, buff, msgid, what->Item); 
    if (what->Type != STstat)
        ARTsendmmap(what->Type);
    ARTclose();
}


/*
**  Go to the next or last (really previous) article in the group.
*/
void
CMDnextlast(int ac UNUSED, char *av[])
{
    char *msgid;
    int	save, delta, errcode;
    bool next;
    const char *message;

    /* Trying to read. */
    if (GRPcount == 0) {
        Reply("%d Not in a newsgroup\r\n", NNTP_FAIL_NO_GROUP);
        return;
    }

    /* No syntax to check.  Only check authorizations. */
    if (!PERMcanread || PERMgroupmadeinvalid) {
	Reply("%d Read access denied\r\n",
              PERMcanauthenticate ? NNTP_FAIL_AUTH_NEEDED : NNTP_ERR_ACCESS);
	return;
    }

    if (ARTnumber < ARTlow || ARTnumber > ARThigh) {
        Reply("%d Current article number %lu is invalid\r\n",
              NNTP_FAIL_ARTNUM_INVALID, ARTnumber);
        return;
    }

    /* NEXT? */
    next = (av[0][0] == 'n' || av[0][0] == 'N');
    if (next) {
	delta = 1;
	errcode = NNTP_FAIL_NEXT;
	message = "next";
    }
    else {
	delta = -1;
	errcode = NNTP_FAIL_PREV;
	message = "previous";
    }

    save = ARTnumber;
    msgid = NULL;
    do {
        ARTnumber += delta;
        if (ARTnumber < ARTlow || ARTnumber > ARThigh) {
            Reply("%d No %s article to retrieve\r\n", errcode, message);
            ARTnumber = save;
            return;
        }
        if (!ARTopen(ARTnumber))
            continue;
        msgid = GetHeader("Message-ID", true);
        ARTclose();
    } while (msgid == NULL);

    Reply("%d %lu %s Article retrieved; request text separately\r\n",
	   NNTP_OK_STAT, ARTnumber, msgid);
}


/*
**  Parse a range (in av[1]) which may be any of the following:
**    - An article number.
**    - An article number followed by a dash to indicate all following.
**    - An article number followed by a dash followed by another article
**      number.
**
**  In the last case, if the second number is less than the first number,
**  then the range contains no articles.
**
**  In addition to RFC 3977, we also accept:
**    - A dash followed by an article number to indicate all previous.
**    - A dash for everything.
**
**  ac is the number of arguments in the command:
**    LISTGROUP news.software.nntp 12-42
**  gives ac=3 and av[1] should match "12-42" (whence the "av+1" call
**  of CMDgetrange).
**
**  rp->Low and rp->High will contain the values of the range.
**  *DidReply will be true if this function sends an answer.
*/
bool
CMDgetrange(int ac, char *av[], ARTRANGE *rp, bool *DidReply)
{
    char		*p;

    *DidReply = false;

    if (ac == 1) {
        /* No arguments, do only current article. */
        if (ARTnumber < ARTlow || ARTnumber > ARThigh) {
            Reply("%d Current article number %lu is invalid\r\n",
                  NNTP_FAIL_ARTNUM_INVALID, ARTnumber);
            *DidReply = true;
            return false;
        }
        rp->High = rp->Low = ARTnumber;
        return true;
    }

    /* Check the syntax. */
    if (!IsValidRange(av[1])) {
        Reply("%d Syntax error in range\r\n", NNTP_ERR_SYNTAX);
        *DidReply = true;
        return false;
    }

    /* Got just a single number? */
    if ((p = strchr(av[1], '-')) == NULL) {
        rp->Low = rp->High = atol(av[1]);
        return true;
    }

    /* "-" becomes "\0" and we parse the low water mark.
     * Note that atol() returns 0 if no valid number
     * is found at the beginning of *p. */
    *p++ = '\0';
    rp->Low = atol(av[1]);

    /* Adjust the low water mark. */
    if (rp->Low < ARTlow)
        rp->Low = ARTlow;

    /* Parse and adjust the high water mark.
     * "12-" gives everything from 12 to the end.
     * We do not bother about "42-12" or "42-0" in this function. */
    if ((*p == '\0') || ((rp->High = atol(p)) > ARThigh))
        rp->High = ARThigh;

    p--;
    *p = '-';

    return true;
}


/*
**  Apply virtual hosting to an Xref: field.
*/
static char *
vhost_xref(char *p)
{
    char *space;
    size_t offset;
    char *field = NULL;

    space = strchr(p, ' ');
    if (space == NULL) {
	warn("malformed Xref: `%s'", field);
	goto fail;
    }
    offset = space - p;
    space = strchr(p + offset, ' ');
    if (space == NULL) {
	warn("malformed Xref: `%s'", field);
	goto fail;
    }
    field = concat(PERMaccessconf->domain, space, NULL);
 fail:
    free(p);
    return field;
}


/*
**  Dump parts of the overview database with the OVER command.
**  The legacy XOVER is also kept, with its specific behaviour.
*/
void
CMDover(int ac, char *av[])
{
    bool	        DidReply, HasNotReplied;
    ARTRANGE		range;
    struct timeval	stv, etv;
    ARTNUM		artnum;
    void		*handle;
    char		*data, *r;
    const char		*p, *q;
    int			len, useIOb = 0;
    TOKEN		token;
    struct cvector *vector = NULL;
    bool                xover, mid;

    xover = (strcasecmp(av[0], "XOVER") == 0);
    mid = (ac > 1 && IsValidMessageID(av[1], true));

    if (mid && !xover) {
        /* FIXME:  We still do not support OVER MSGID, sorry! */
        Reply("%d Overview by message-ID unsupported\r\n", NNTP_ERR_UNAVAILABLE);
        return;
    }

    /* Check the syntax of the arguments first.
     * We do not accept a message-ID for XOVER, contrary to OVER.  A range
     * is accepted for both of them. */
    if (ac > 1 && !IsValidRange(av[1])) {
        /* It is better to check for a range before a message-ID because
         * '<' could have been forgotten and the error message should then
         * report a syntax error in the message-ID. */
        if (xover || isdigit((unsigned char) av[1][0]) || av[1][0] == '-') {
            Reply("%d Syntax error in range\r\n", NNTP_ERR_SYNTAX);
            return;
        } else if (!mid) {
            Reply("%d Syntax error in message-ID\r\n", NNTP_ERR_SYNTAX);
            return;
        }   
    }

    /* Trying to read. */
    if (GRPcount == 0) {
        Reply("%d Not in a newsgroup\r\n", NNTP_FAIL_NO_GROUP);
        return;
    }

    /* Check authorizations.  If a range is requested (not a message-ID),
     * we check whether the group is still readable. */
    if (!PERMcanread || (!mid && PERMgroupmadeinvalid)) {
	Reply("%d Read access denied\r\n",
              PERMcanauthenticate ? NNTP_FAIL_AUTH_NEEDED : NNTP_ERR_ACCESS);
	return;
    }

    /* Parse range.  CMDgetrange() correctly sets the range when
     * there is no arguments. */
    if (!CMDgetrange(ac, av, &range, &DidReply))
        if (DidReply)
            return;

    if (PERMaccessconf->nnrpdoverstats) {
        OVERcount++;
        gettimeofday(&stv, NULL);
    }
    if ((handle = (void *)OVopensearch(GRPcur, range.Low, range.High)) == NULL) {
        /* The response code for OVER is different if a range is provided.
         * Note that XOVER answers OK. */
        if (ac > 1)
            Reply("%d No articles in %s\r\n",
                  xover ? NNTP_OK_OVER : NNTP_FAIL_ARTNUM_NOTFOUND, av[1]);
        else
            Reply("%d No such article number %lu\r\n",
                  xover ? NNTP_OK_OVER : NNTP_FAIL_ARTNUM_NOTFOUND, ARTnumber);
        if (xover)
            Printf(".\r\n");
        return;
    }
    if (PERMaccessconf->nnrpdoverstats) {
	gettimeofday(&etv, NULL);
	OVERtime+=(etv.tv_sec - stv.tv_sec) * 1000;
	OVERtime+=(etv.tv_usec - stv.tv_usec) / 1000;
    }

    if (PERMaccessconf->nnrpdoverstats)
	gettimeofday(&stv, NULL);

    /* If OVSTATICSEARCH is true, then the data returned by OVsearch is only
       valid until the next call to OVsearch.  In this case, we must use
       SendIOb because it copies the data. */
    OVctl(OVSTATICSEARCH, &useIOb);

    HasNotReplied = true;
    while (OVsearch(handle, &artnum, &data, &len, &token, NULL)) {
	if (PERMaccessconf->nnrpdoverstats) {
	    gettimeofday(&etv, NULL);
	    OVERtime+=(etv.tv_sec - stv.tv_sec) * 1000;
	    OVERtime+=(etv.tv_usec - stv.tv_usec) / 1000;
	}
	if (len == 0 || (PERMaccessconf->nnrpdcheckart && !ARTinstorebytoken(token))) {
	    if (PERMaccessconf->nnrpdoverstats) {
		OVERmiss++;
		gettimeofday(&stv, NULL);
	    }
	    continue;
	}
	if (PERMaccessconf->nnrpdoverstats) {
	    OVERhit++;
	    OVERsize += len;
	}

        if (HasNotReplied) {
            if (ac > 1)
                Reply("%d Overview information for %s follows\r\n",
                      NNTP_OK_OVER, av[1]);
            else
                Reply("%d Overview information for %lu follows\r\n",
                      NNTP_OK_OVER, ARTnumber);
            fflush(stdout);
            HasNotReplied = false;
        }

        vector = overview_split(data, len, NULL, vector);
        r = overview_get_standard_header(vector, OVERVIEW_MESSAGE_ID);
        if (r == NULL) {
            if (PERMaccessconf->nnrpdoverstats) {
                gettimeofday(&stv, NULL);
            }
            continue;
        }
	cache_add(HashMessageID(r), token);
	free(r);
	if (VirtualPathlen > 0 && overhdr_xref != -1) {
            if ((size_t)(overhdr_xref + 1) >= vector->count) {
                if (PERMaccessconf->nnrpdoverstats) {
                    gettimeofday(&stv, NULL);
                }
                continue;
            }
	    p = vector->strings[overhdr_xref] + sizeof("Xref: ") - 1;
	    while ((p < data + len) && *p == ' ')
		++p;
	    q = memchr(p, ' ', data + len - p);
            if (q == NULL) {
                if (PERMaccessconf->nnrpdoverstats) {
                    gettimeofday(&stv, NULL);
                }
                continue;
            }
            /* Copy the virtual path without its final '!'. */
	    if(useIOb) {
		SendIOb(data, p - data);
		SendIOb(VirtualPath, VirtualPathlen - 1);
		SendIOb(q, len - (q - data));
	    } else {
		SendIOv(data, p - data);
		SendIOv(VirtualPath, VirtualPathlen - 1);
		SendIOv(q, len - (q - data));
	    }
	} else {
	    if(useIOb)
		SendIOb(data, len);
	    else
		SendIOv(data, len);
	}
	if (PERMaccessconf->nnrpdoverstats)
	    gettimeofday(&stv, NULL);
    }

    if (PERMaccessconf->nnrpdoverstats) {
        gettimeofday(&etv, NULL);
        OVERtime+=(etv.tv_sec - stv.tv_sec) * 1000;
        OVERtime+=(etv.tv_usec - stv.tv_usec) / 1000;
    }

    if (vector)
        cvector_free(vector);

    if (HasNotReplied) {
        /* The response code for OVER is different if a range is provided.
         * Note that XOVER answers OK. */
        if (ac > 1)
            Reply("%d No articles in %s\r\n",
                  xover ? NNTP_OK_OVER : NNTP_FAIL_ARTNUM_NOTFOUND, av[1]);
        else
            Reply("%d No such article number %lu\r\n",
                  xover ? NNTP_OK_OVER : NNTP_FAIL_ARTNUM_NOTFOUND, ARTnumber);
        if (xover)
            Printf(".\r\n");
    } else {
        if(useIOb) {
            SendIOb(".\r\n", 3);
            PushIOb();
        } else {
            SendIOv(".\r\n", 3);
            PushIOv();
        }
    }

    if (PERMaccessconf->nnrpdoverstats)
	gettimeofday(&stv, NULL);
    OVclosesearch(handle);
    if (PERMaccessconf->nnrpdoverstats) {
        gettimeofday(&etv, NULL);
        OVERtime+=(etv.tv_sec - stv.tv_sec) * 1000;
        OVERtime+=(etv.tv_usec - stv.tv_usec) / 1000;
    }

}

/*
**  Access specific fields from an article with HDR.
**  The legacy XHDR and XPAT are also kept, with their specific behaviours.
*/
void
CMDpat(int ac, char *av[])
{
    char	        *p;
    unsigned long      	i;
    ARTRANGE		range;
    bool		IsBytes, IsLines;
    bool                IsMetaBytes, IsMetaLines;
    bool		DidReply, HasNotReplied;
    const char		*header;
    char		*pattern;
    char		*text;
    int			Overview;
    ARTNUM		artnum;
    char		buff[SPOOLNAMEBUFF];
    void                *handle;
    char                *data;
    int                 len;
    TOKEN               token;
    struct cvector *vector = NULL;
    bool                hdr, mid;

    hdr = (strcasecmp(av[0], "HDR") == 0);
    mid = (ac > 2 && IsValidMessageID(av[2], true));

    /* Check the syntax of the arguments first. */
    if (ac > 2 && !IsValidRange(av[2])) {
        /* It is better to check for a range before a message-ID because
         * '<' could have been forgotten and the error message should then
         * report a syntax error in the message-ID. */
        if (isdigit((unsigned char) av[2][0]) || av[2][0] == '-') {
            Reply("%d Syntax error in range\r\n", NNTP_ERR_SYNTAX);
            return;
        } else if (!mid) {
            Reply("%d Syntax error in message-ID\r\n", NNTP_ERR_SYNTAX);
            return;
        }
    }

    header = av[1];

    /* If metadata is asked for, convert it to headers that
     * the overview database knows. */
    IsBytes = (strcasecmp(header, "Bytes") == 0);
    IsLines = (strcasecmp(header, "Lines") == 0);
    IsMetaBytes = (strcasecmp(header, ":bytes") == 0);
    IsMetaLines = (strcasecmp(header, ":lines") == 0);
    /* Make these changes because our overview database does
     * not currently know metadata names. */
    if (IsMetaBytes)
        header = "Bytes";
    if (IsMetaLines)
        header = "Lines";

    /* We only allow :bytes and :lines for metadata. */
    if (!IsMetaLines && !IsMetaBytes) {
        p = av[1];
        p++;
        if (strncasecmp(header, ":", 1) == 0 && IsValidHeaderName(p)) {
            Reply("%d Unsupported metadata request\r\n",
                  NNTP_ERR_UNAVAILABLE);
            return;
        } else if (!IsValidHeaderName(header)) {
            Reply("%d Syntax error in header name\r\n", NNTP_ERR_SYNTAX);
            return;
        }
    }

    /* Trying to read. */
    if (GRPcount == 0 && !mid) {
        Reply("%d Not in a newsgroup\r\n", NNTP_FAIL_NO_GROUP);
        return;
    }

    /* Check authorizations.  If a range is requested (not a message-ID),
     * we check whether the group is still readable. */
    if (!PERMcanread || (!mid && PERMgroupmadeinvalid)) {
        Reply("%d Read access denied\r\n",
              PERMcanauthenticate ? NNTP_FAIL_AUTH_NEEDED : NNTP_ERR_ACCESS);
        return;
    }

    if (ac > 3) /* Necessarily XPAT. */
	pattern = Glom(&av[3]);
    else
	pattern = NULL;

    /* We will only do the loop once.  It is just in order to easily break. */
    do {
	/* Message-ID specified? */
	if (mid) {
            /* FIXME:  We do not handle metadata requests by message-ID. */
            if (hdr && (IsMetaBytes || IsMetaLines)) {
                Reply("%d Metadata requests by message-ID unsupported\r\n",
                      NNTP_ERR_UNAVAILABLE);
                break;
            }

	    p = av[2];
	    if (!ARTopenbyid(p, &artnum, false)) {
		Reply("%d No such article\r\n", NNTP_FAIL_MSGID_NOTFOUND);
		break;
	    }

            if (!PERMartok()) {
                ARTclose();
                Reply("%d Read access denied for this article\r\n",
                      PERMcanauthenticate ? NNTP_FAIL_AUTH_NEEDED : NNTP_ERR_ACCESS);
                break;
            }

	    Reply("%d Header information for %s follows (from the article)\r\n",
                   hdr ? NNTP_OK_HDR : NNTP_OK_HEAD, av[1]);

	    if ((text = GetHeader(av[1], false)) != NULL
		&& (!pattern || uwildmat_simple(text, pattern)))
		Printf("%s %s\r\n", hdr ? "0" : p, text);
            else if (hdr) {
                /* We always have to answer something with HDR. */
                Printf("0 \r\n");
            }

	    ARTclose();
	    Printf(".\r\n");
	    break;
	}

        /* Parse range.  CMDgetrange() correctly sets the range when
         * there is no arguments. */
        if (!CMDgetrange(ac - 1, av + 1, &range, &DidReply))
            if (DidReply)
                break;

	/* In overview? */
        Overview = overview_index(header, OVextra);

        HasNotReplied = true;

	/* Not in overview, we have to fish headers out from the articles. */
	if (Overview < 0 || IsBytes || IsLines) {
	    for (i = range.Low; i <= range.High && range.High > 0; i++) {
		if (!ARTopen(i))
		    continue;
                if (HasNotReplied) {
                    Reply("%d Header information for %s follows (from articles)\r\n",
                          hdr ? NNTP_OK_HDR : NNTP_OK_HEAD, av[1]);
                    HasNotReplied = false;
                }
		p = GetHeader(header, false);
		if (p && (!pattern || uwildmat_simple(p, pattern))) {
		    snprintf(buff, sizeof(buff), "%lu ", i);
		    SendIOb(buff, strlen(buff));
		    SendIOb(p, strlen(p));
		    SendIOb("\r\n", 2);
		} else if (hdr) {
                    /* We always have to answer something with HDR. */
                    snprintf(buff, sizeof(buff), "%lu \r\n", i);
                    SendIOb(buff, strlen(buff));
                }
                ARTclose();
	    }
            if (HasNotReplied) {
                if (hdr) {
                    if (ac > 2)
                        Reply("%d No articles in %s\r\n",
                              NNTP_FAIL_ARTNUM_NOTFOUND, av[2]);
                    else
                        Reply("%d No such article number %lu\r\n",
                              NNTP_FAIL_ARTNUM_NOTFOUND, ARTnumber);
                } else {
                    Reply("%d No header information for %s follows (from articles)\r\n",
                          NNTP_OK_HEAD, av[1]);
                    Printf(".\r\n");
                }
                break;
            } else {
                SendIOb(".\r\n", 3);
            }
	    PushIOb();
	    break;
	}

	/* Okay then, we can grab values from the overview database. */
	handle = (void *)OVopensearch(GRPcur, range.Low, range.High);
	if (handle == NULL) {
            if (hdr) {
                if (ac > 2)
                    Reply("%d No articles in %s\r\n",
                          NNTP_FAIL_ARTNUM_NOTFOUND, av[2]);
                else
                    Reply("%d No such article number %lu\r\n",
                          NNTP_FAIL_ARTNUM_NOTFOUND, ARTnumber);
            } else {
                Reply("%d No header information for %s follows (from overview)\r\n",
                      NNTP_OK_HEAD, av[1]);
                Printf(".\r\n");
            }
	    break;
	}	
	
	while (OVsearch(handle, &artnum, &data, &len, &token, NULL)) {
	    if (len == 0 || (PERMaccessconf->nnrpdcheckart
		&& !ARTinstorebytoken(token)))
		continue;
            if (HasNotReplied) {
                Reply("%d Header or metadata information for %s follows (from overview)\r\n",
                      hdr ? NNTP_OK_HDR : NNTP_OK_HEAD, av[1]);
                HasNotReplied = false;
            }
	    vector = overview_split(data, len, NULL, vector);
            if (Overview < OVERVIEW_MAX) {
                p = overview_get_standard_header(vector, Overview);
            } else {
                p = overview_get_extra_header(vector, header);
            }
	    if (p != NULL) {
		if (PERMaccessconf->virtualhost &&
			   Overview == overhdr_xref) {
		    p = vhost_xref(p);
		    if (p == NULL) {
                        if (hdr) {
                            snprintf(buff, sizeof(buff), "%lu \r\n", artnum);
                            SendIOb(buff, strlen(buff));
                        }
			continue;
                    }
		}
		if (!pattern || uwildmat_simple(p, pattern)) {
		    snprintf(buff, sizeof(buff), "%lu ", artnum);
		    SendIOb(buff, strlen(buff));
		    SendIOb(p, strlen(p));
		    SendIOb("\r\n", 2);
		}
                /* No need to have another condition for HDR because
                 * pattern is NULL for it, and p is not NULL here. */
		free(p);
	    } else if (hdr) {
                snprintf(buff, sizeof(buff), "%lu \r\n", artnum);
                SendIOb(buff, strlen(buff));
            }
	}
        if (HasNotReplied) {
            if (hdr) {
                if (ac > 2)
                    Reply("%d No articles in %s\r\n",
                          NNTP_FAIL_ARTNUM_NOTFOUND, av[2]);
                else
                    Reply("%d Current article number %lu is invalid\r\n",
                          NNTP_FAIL_ARTNUM_INVALID, ARTnumber);
            } else {
                Reply("%d No header or metadata information for %s follows (from overview)\r\n",
                      NNTP_OK_HEAD, av[1]);
                Printf(".\r\n");
            }
            break;
        } else {
            SendIOb(".\r\n", 3);
        }
	PushIOb();
	OVclosesearch(handle);
    } while (0);

    if (vector)
	cvector_free(vector);

    if (pattern)
	free(pattern);
}
