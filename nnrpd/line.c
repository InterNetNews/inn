/*
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

#include "portable/system.h"

#include <assert.h>
#ifdef HAVE_SYS_SELECT_H
#    include <sys/select.h>
#endif

#include "inn/messages.h"
#include "nnrpd.h"
#include "tls.h"
#include <signal.h>

#ifdef HAVE_OPENSSL
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

#ifdef HAVE_OPENSSL
/*
**  Alarm signal handler for client timeout.
*/
static void
alarmHandler(int s UNUSED)
{
    /* Send the close_notify shutdown alert to the news reader.
     * No need to call again SSL_shutdown() to complete the bidirectional
     * shutdown handshake as we do not expect more data to process.  Just
     * close the underlying connection without waiting for the response.
     * Such a unidirectional shutdown is allowed per OpenSSL documentation. */
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
**  Reset a line structure.
*/
void
line_reset(struct line *line)
{
    assert(line);
    line->where = line->start;
    line->remaining = 0;
}

/*
**  Timeout is used only if HAVE_OPENSSL is defined.
**  Returns -2 on timeout, -1 on read error, and otherwise the number of
**  bytes read.
*/
static ssize_t
line_doread(void *p, size_t len, int timeout UNUSED)
{
    ssize_t n;

    do {
#if defined(HAVE_ZLIB)
        /* Process data that may already be available in the zlib buffer. */
        if (compression_layer_on
            && (zstream_in->avail_in > 0 || zstream_inflate_needed)) {
            int r;

            zstream_in->next_out = p;
            zstream_in->avail_out = len;

            r = inflate(zstream_in, Z_SYNC_FLUSH);

            if (!(r == Z_OK || r == Z_BUF_ERROR || r == Z_STREAM_END)) {
                sysnotice("inflate() failed: %d; %s", r,
                          zstream_in->msg != NULL ? zstream_in->msg
                                                  : "no detail");
                n = -1;
                break;
            }

            /* Check whether inflate() has finished to process its input.
             * If not, we need to call it again, even though avail_in is 0. */
            zstream_inflate_needed = (r != Z_STREAM_END);

            if (zstream_in->avail_out < len) {
                /* Some data has been uncompressed.  Treat it now. */
                n = len - zstream_in->avail_out;
                break;
            }
            /* If we reach here, then it means that inflate() needs more
             * input, so we go on reading data on the wire. */
        }
#endif /* HAVE_ZLIB */

#ifdef HAVE_OPENSSL
        if (tls_conn) {
            int err;
            xsignal(SIGALRM, alarmHandler);
            do {
                alarm(timeout);
                n = SSL_read(tls_conn, p, len);
                alarm(0);
                if (tls_conn == NULL) {
                    n = -2; /* timeout */
                    break;
                }
                err = SSL_get_error(tls_conn, n);
                switch (err) {
                case SSL_ERROR_ZERO_RETURN:
                    SSL_shutdown(tls_conn);
                    /* fallthrough */
                case SSL_ERROR_SYSCALL:
                case SSL_ERROR_SSL:
                    /* SSL_shutdown() must not be called. */
                    tls_conn = NULL;
                    errno = ECONNRESET;
                    n = -1;
                    break;
                }
            } while (err == SSL_ERROR_WANT_READ);
            xsignal(SIGALRM, SIG_DFL);
        } else
#endif /* HAVE_OPENSSL */
            do {
                n = read(STDIN_FILENO, p, len);
            } while (n == -1 && errno == EINTR);

        if (n <= 0)
            break; /* EOF or error. */

#if defined(HAVE_SASL)
        if (sasl_conn != NULL && sasl_ssf > 0) {
            /* Security layer in place, decode the data.
             * The incoming data is always encoded in chunks of length
             * inferior or equal to NNTP_MAXLEN_COMMAND (the maxbufsize value
             * of SASL_SEC_PROPS passed as part of the SASL exchange).
             * So there's enough data to read in the p buffer. */
            const char *out;
            unsigned outlen;
            int r;

            if ((r = sasl_decode(sasl_conn, p, n, &out, &outlen)) == SASL_OK) {
                if (outlen > len) {
                    sysnotice("sasl_decode() returned too much output");
                    n = -1;
                } else {
                    if (outlen > 0) {
                        memcpy(p, out, outlen);
                    }
                    n = outlen;
                }
            } else {
                const char *ed = sasl_errdetail(sasl_conn);

                sysnotice("sasl_decode() failed: %s; %s",
                          sasl_errstring(r, NULL, NULL),
                          ed != NULL ? ed : "no detail");
                n = -1;
            }
        }
#endif /* HAVE_SASL */

#if defined(HAVE_ZLIB)
        if (compression_layer_on && n > 0) {
            size_t zconsumed;

            if (zstream_in->avail_in > 0 && zstream_in->next_in != Z_NULL) {
                zconsumed = zstream_in->next_in - zbuf_in;
            } else {
                zconsumed = 0;
                zbuf_in_allocated = 0;
            }

            /* Transfer the data we have just read to zstream_in,
             * and loop to actually process it. */
            if ((ssize_t)(zbuf_in_size - zbuf_in_allocated) < n) {
                size_t newsize = zbuf_in_size * 2 + n;

                /* Don't grow the buffer bigger than the maximum
                 * article size we'll accept. */
                if (PERMaccessconf->localmaxartsize > NNTP_MAXLEN_COMMAND) {
                    if (newsize > PERMaccessconf->localmaxartsize) {
                        newsize = PERMaccessconf->localmaxartsize;
                    }
                }
                if (newsize == zbuf_in_size) {
                    warn("%s overflowed our zstream_in buffer (%lu)",
                         Client.host, (unsigned long) newsize);
                    n = -1;
                    break;
                }
                zbuf_in = xrealloc(zbuf_in, newsize);
                zbuf_in_size = newsize;
            }
            memcpy(zbuf_in + zbuf_in_allocated, p, n);
            zstream_in->next_in = zbuf_in + zconsumed;
            zstream_in->avail_in += n;
            zbuf_in_allocated += n;
            zstream_inflate_needed = true;
            /* Loop to actually inflate the compressed data we received. */
            n = 0;
        }
#endif                /* HAVE_ZLIB */
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
                         "discarding further input",
                         Client.host, PERMaccessconf->localmaxartsize);
                    where = line->start;
                    r = RTlong;
                } else {
                    line->start = xrealloc(line->start, newsize);
                    where = line->start + line->allocated;
                    line->allocated = newsize;
                }
            }

#ifdef HAVE_OPENSSL
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
#ifdef HAVE_OPENSSL
            }
#endif
            count = line_doread(where, line->allocated - (where - line->start),
                                timeout);

            /* Give timeout to both real timeouts (count == -2) and
             * read errors (count == -1). */
            if (count < 0) {
                if (count == -1) {
                    sysnotice("%s can't read", Client.host);
                }
                return RTtimeout;
            }
            /* If we hit EOF, terminate the string and send it back. */
            if (count == 0) {
                assert((where + count) < (line->start + line->allocated));
                where[count] = '\0';
                return RTeof;
            }
            /* Search for `\n' in what we just read.  If we find it, we'll
             * drop out and return the line for processing. */
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
