/*  $Id$
**
**  Line by line reading support from sockets/pipes.
**
**  Written by Alex Kiernan <alex.kiernan@thus.net>.
**
**  This code implements an infinitely (well size_t) long single line
**  read routine.  To protect against eating all available memory, it
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
#include <signal.h>
#include "tls.h"

#ifdef HAVE_SSL
extern SSL *tls_conn;
#endif

/*
**  Free a previously allocated line structure.
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

#ifdef HAVE_SSL
/*
**  Alarm signal handler for client timeout.
*/
static void
alarmHandler(int s UNUSED)
{
    SSL_shutdown(tls_conn);
    tls_conn = NULL;
    errno = ECONNRESET;
}
#endif
  
/*
**  Initialise a new line structure.
*/
void
line_init(struct line *line)
{
    assert(line);
    line->allocated = NNTP_MAXLEN_COMMAND;
    line->where = line->start = xmalloc(line->allocated);
    line->remaining = 0;
}

/*
**  Timeout is used only if HAVE_SSL is defined.
*/
static ssize_t
line_doread(void *p, size_t len, int timeout UNUSED)
{
    ssize_t n;

    do {
#ifdef HAVE_SSL
	if (tls_conn) {
	    int err;
            xsignal(SIGALRM, alarmHandler);
	    do {
                alarm(timeout);
                n = SSL_read(tls_conn, p, len);
                alarm(0);
                if (tls_conn == NULL) {
                    break;
                }
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
            xsignal (SIGALRM, SIG_DFL);
	} else
#endif /* HAVE_SSL */
	    do {
		n = read(STDIN_FILENO, p, len);
	    } while (n == -1 && errno == EINTR);

	if (n <= 0)
            break; /* EOF or error. */

#ifdef HAVE_SASL
	if (sasl_conn && sasl_ssf) {
	    /* Security layer in place, decode the data. */
	    const char *out;
	    unsigned outlen;
	    int r;

	    if ((r = sasl_decode(sasl_conn, p, n, &out, &outlen)) == SASL_OK) {
		if (outlen)
                    memcpy(p, out, outlen);
		n = outlen;
	    } else {
		sysnotice("sasl_decode() failed: %s; %s",
			  sasl_errstring(r, NULL, NULL),
			  sasl_errdetail(sasl_conn));
		n = -1;
	    }
	}
#endif /* HAVE_SASL */
    } while (n == 0); /* Split SASL blob, need to read more data. */

    return n;
}

READTYPE
line_read(struct line *line, int timeout, const char **p, size_t *len,
          size_t *stripped)
{
    char *where;
    char *lf = NULL;
    READTYPE r = RTok;

    assert(line != NULL);
    assert(line->start != NULL);
    /* Shuffle any trailing portion not yet processed to the start of
     * the buffer. */
    if (line->remaining != 0) {
	if (line->start != line->where) {
	    memmove(line->start, line->where, line->remaining);
	}
	lf = memchr(line->start, '\n', line->remaining);
    }
    where = line->start + line->remaining;

    /* If we found a line terminator in the data we have, we don't need
     * to ask for any more. */
    if (lf == NULL) {
	do {
	    fd_set rmask;
	    int i;
	    ssize_t count;

	    /* If we've filled the line buffer, double the size,
	     * reallocate the buffer and try again. */
	    if (where == line->start + line->allocated) {
		size_t newsize = line->allocated * 2;
	    
		/* Don't grow the buffer bigger than the maximum
		 * article size we'll accept. */
                if (PERMaccessconf->localmaxartsize > NNTP_MAXLEN_COMMAND)
                    if (newsize > PERMaccessconf->localmaxartsize)
                        newsize = PERMaccessconf->localmaxartsize;

		/* If we're trying to grow from the same size, to the
		 * same size, we must have hit the localmaxartsize
		 * buffer for a second (or subsequent) time -- the user
		 * is likely trying to DOS us, so don't double the
		 * size any more, just overwrite characters until they
		 * stop, then discard the whole thing. */
		if (newsize == line->allocated) {
		    warn("%s overflowed our line buffer (%lu), "
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

#ifdef HAVE_SSL
            /* It seems that the SSL_read cannot be mixed with select()
             * as in the current code.  SSL communicates in its own data
             * blocks and hand shaking.  The do_readline using SSL_read
             * could return, but still with a partial line in the SSL_read
             * buffer.  Then the server SSL routine would sit there waiting
             * for completion of that data block while nnrpd sat at the
             * select() routine waiting for more data from the server.
             *
             * Here, we decide to just bypass the select() wait.  Unlike
             * innd with multiple threads, the select on nnrpd is just
             * waiting on a single file descriptor, so it is not really
             * essential with blocked read like SSL_read.  Using an alarm
             * signal around SSL_read for non active timeout, SSL works
             * without dead locks.  However, without the select() wait,
             * the IDLE timer stat won't be collected...
             */
            if (tls_conn == NULL) {
#endif
                /* Wait for activity on stdin, updating timer stats as we
                 * go. */
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

                /* If stdin didn't select, we must have timed out. */
                if (i == 0 || !FD_ISSET(STDIN_FILENO, &rmask))
                    return RTtimeout;
#ifdef HAVE_SSL
            }
#endif
            count = line_doread(where,
                                line->allocated - (where - line->start), 
                                timeout);

	    /* Give timeout for read errors. */
	    if (count < 0) {
		sysnotice("%s can't read", Client.host);
		return RTtimeout;
	    }
	    /* If we hit EOF, terminate the string and send it back. */
	    if (count == 0) {
		assert((where + count) < (line->start + line->allocated));
		where[count] = '\0';
		return RTeof;
	    }
	    /* Search for `\n' in what we just read.  If we find it we'll
	     * drop out and return the line for processing */
	    lf = memchr(where, '\n', count);
	    where += count;
	} while (lf == NULL);
    }

    /* Remember where we've processed up to, so we can start off there
     * next time. */
    line->where = lf + 1;
    line->remaining = where - line->where;

    if (r == RTok) {
	/* If we see a full CRLF pair, strip them both off before
	 * returning the line to our caller.  If we just get an LF
	 * we'll accept that too (debugging INN can then be less annoying). */
	if (lf > line->start && lf[-1] == '\r') {
	    --lf;
            if (stripped != NULL)
                (*stripped)++;
	}
	*lf = '\0';
        if (stripped != NULL)
            (*stripped)++;
	*len = lf - line->start;
	*p = line->start;
    }
    return r;
}
