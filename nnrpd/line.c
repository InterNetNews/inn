/*  $Id$
**
**  Line by line reading support from sockets/pipes
**
**  Written by Alex Kiernan (alex.kiernan@thus.net)
**
**  This code implements a infinitely (well size_t) long single line
**  read routine, to protect against eating all available memory it
**  actually starts discarding characters if you try to send more than
**  the maximum article size in a single line.
** 
*/

#include "config.h"
#include "clibrary.h"
#include <assert.h>
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#include "inn/messages.h"
#include "nnrpd.h"
#include "tls.h"

#ifdef HAVE_SSL
extern SSL *tls_conn;
#endif

/*
**  free a previously allocated line structure
*/
void
line_free(struct line *line)
{
    static const struct line nullline = {0, 0, 0, 0};

    if (line && line->start) {
	free(line->start);
	*line = nullline;
    }
}

/*
**  initialise a new line structure
*/
void
line_init(struct line *line)
{
    assert(line);
    line->allocated = NNTP_STRLEN;
    line->where = line->start = xmalloc(line->allocated);
    line->remaining = 0;
}

static ssize_t
line_doread(void *p, size_t len)
{
    ssize_t n;

    do {
#ifdef HAVE_SSL
	if (tls_conn) {
	    int err;
	    do {
		n = SSL_read(tls_conn, p, len);
		err = SSL_get_error(tls_conn, n);
		switch (err) {
		case SSL_ERROR_SYSCALL:
		    break;
		    
		case SSL_ERROR_SSL:
		    SSL_shutdown(tls_conn);
		    tls_conn = NULL;
		    errno = ECONNRESET;
		    break;
		}
	    } while (err == SSL_ERROR_WANT_READ);
	} else
#endif /* HAVE_SSL */
	    do {
		n = read(STDIN_FILENO, p, len);
	    } while (n == -1 && errno == EINTR);

	if (n <= 0) break; /* EOF or error */

#ifdef HAVE_SASL
	if (sasl_conn && sasl_ssf) {
	    /* security layer in place, decode the data */
	    const char *out;
	    unsigned outlen;
	    int r;

	    if ((r = sasl_decode(sasl_conn, p, n, &out, &outlen)) == SASL_OK) {
		if (outlen) memcpy(p, out, outlen);
		n = outlen;
	    } else {
		sysnotice("sasl_decode() failed: %s; %s",
			  sasl_errstring(r, NULL, NULL),
			  sasl_errdetail(sasl_conn));
		n = -1;
	    }
	}
#endif /* HAVE_SASL */
    } while (n == 0); /* split SASL blob, need to read more data */

    return n;
}

READTYPE
line_read(struct line *line, int timeout, const char **p, size_t *len)
{
    char *where;
    char *lf = NULL;
    READTYPE r = RTok;

    assert(line != NULL);
    assert(line->start != NULL);
    /* shuffle any trailing portion not yet processed to the start of
     * the buffer */
    if (line->remaining != 0) {
	if (line->start != line->where) {
	    memmove(line->start, line->where, line->remaining);
	}
	lf = memchr(line->start, '\n', line->remaining);
    }
    where = line->start + line->remaining;

    /* if we found a line terminator in the data we have we don't need
     * to ask for any more */
    if (lf == NULL) {
	do {
	    fd_set rmask;
	    int i;
	    ssize_t count;

	    /* if we've filled the line buffer, double the size,
	     * reallocate the buffer and try again */
	    if (where == line->start + line->allocated) {
		size_t newsize = line->allocated * 2;
	    
		/* don't grow the buffer bigger than the maximum
		 * article size we'll accept */
                if (PERMaccessconf->localmaxartsize > 0)
                    if (newsize > (unsigned)PERMaccessconf->localmaxartsize)
                        newsize = PERMaccessconf->localmaxartsize;

		/* if we're trying to grow from the same size, to the
		 * same size, we must have hit the localmaxartsize
		 * buffer for a second (or subsequent) time - the user
		 * is likely trying to DOS us, so don't double the
		 * size any more, just overwrite characters until they
		 * stop, then discard the whole thing */
		if (newsize == line->allocated) {
		    warn("%s overflowed our line buffer (%ld), "
			 "discarding further input", Client.host,
			 PERMaccessconf->localmaxartsize);
		    where = line->start;
		    r = RTlong;
		} else {
		    line->start = xrealloc(line->start, newsize);
		    where = line->start + line->allocated;
		    line->allocated = newsize;
		}
	    }
	    /* wait for activity on stdin, updating timer stats as we
	     * go */
	    do {
		struct timeval t;

		FD_ZERO(&rmask);
		FD_SET(STDIN_FILENO, &rmask);
		t.tv_sec = timeout;
		t.tv_usec = 0;
		TMRstart(TMR_IDLE);
		i = select(STDIN_FILENO + 1, &rmask, NULL, NULL, &t);
		TMRstop(TMR_IDLE);
		if (i == -1 && errno != EINTR) {
		    syswarn("%s can't select", Client.host);
		    return RTtimeout;
		}
	    } while (i == -1);

	    /* if stdin didn't select, we must have timed out */
	    if (i == 0 || !FD_ISSET(STDIN_FILENO, &rmask))
		return RTtimeout;
	    count = line_doread(where,
				line->allocated - (where - line->start));

	    /* give timeout for read errors */
	    if (count < 0) {
		sysnotice("%s can't read", Client.host);
		return RTtimeout;
	    }
	    /* if we hit EOF, terminate the string and send it back */
	    if (count == 0) {
		assert((where + count) < (line->start + line->allocated));
		where[count] = '\0';
		return RTeof;
	    }
	    /* search for `\n' in what we just read, if we find it we'll
	     * drop out and return the line for processing */
	    lf = memchr(where, '\n', count);
	    where += count;
	} while (lf == NULL);
    }

    /* remember where we've processed up to so we can start off there
     * next time */
    line->where = lf + 1;
    line->remaining = where - line->where;

    if (r == RTok) {
	/* if we see a full CRLF pair strip them both off before
	 * returning the line to our caller, if we just get an LF
	 * we'll accept that too */
	if (lf > line->start && lf[-1] == '\r') {
	    --lf;
	}
	*lf = '\0';
	*len = lf - line->start;
	*p = line->start;
    }
    return r;
}
