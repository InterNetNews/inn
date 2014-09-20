/*  $Id$
**
**  Utility functions for speaking the NNTP protocol.
**
**  These functions speak the NNTP protocol over stdio FILEs.  So far, only
**  the server functions are implemented; there is no client support as yet.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include <ctype.h>
#include <errno.h>

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>

#include "inn/buffer.h"
#include "inn/innconf.h"
#include "inn/network.h"
#include "inn/nntp.h"
#include "inn/vector.h"
#include "inn/libinn.h"

/* State for an NNTP connection. */
struct nntp {
    int in_fd;
    int out_fd;
    struct buffer in;
    struct buffer out;
    size_t maxsize;
    time_t timeout;
};


/*
**  Allocate a new nntp struct and return it.  Takes the file descriptor to
**  read from, the file descriptor to write to, the maximum multiline size
**  allowed, and the timeout in seconds for reads.
*/
struct nntp *
nntp_new(int in, int out, size_t maxsize, time_t timeout)
{
    struct nntp *nntp;

    nntp = xmalloc(sizeof(struct nntp));
    nntp->in_fd = in;
    nntp->out_fd = out;
    nntp->maxsize = maxsize;
    nntp->timeout = timeout;
    nntp->in.data = NULL;
    nntp->in.size = 0;
    nntp->in.used = 0;
    nntp->in.left = 0;
    nntp->out.data = NULL;
    nntp->out.size = 0;
    nntp->out.used = 0;
    nntp->out.left = 0;
    return nntp;
}


/*
**  Free an nntp struct and close the associated file descriptors.
*/
void
nntp_free(struct nntp *nntp)
{
    if (nntp == NULL)
        return;
    if (nntp->in.data != NULL)
        free(nntp->in.data);
    if (nntp->out.data != NULL)
        free(nntp->out.data);
    if (nntp->in_fd >= 0)
        close(nntp->in_fd);
    if (nntp->out_fd >= 0 && nntp->out_fd != nntp->in_fd)
        close(nntp->out_fd);
    free(nntp);
}


/*
**  Connect to a remote NNTP server.  Allocates and returns a new nntp struct.
**  Takes the server name, the port to connect to, the maximum buffer size
**  to use, and the timeout value.  On failure to connect to the remote host,
**  returns NULL.
*/
struct nntp *
nntp_connect(const char *host, unsigned short port, size_t maxsize,
             time_t timeout)
{
    int fd;

    fd = network_connect_host(host, port, NULL, timeout);
    if (fd < 0)
        return NULL;
    return nntp_new(fd, fd, maxsize, timeout);
}


/*
**  Sets the read timeout in seconds (0 to wait forever).
*/
void
nntp_timeout(struct nntp *nntp, time_t timeout)
{
    nntp->timeout = timeout;
}


/*
**  Read data from an NNTP connection, as much as is available, putting it
**  into the reader buffer.  Return an nntp_status code.
*/
static enum nntp_status
nntp_read_data(struct nntp *nntp)
{
    ssize_t count;
    int status;

    /* Resize the buffer if we're out of space, but don't allow the buffer to
       grow longer than maxsize. */
    if (nntp->in.size == 0)
        buffer_resize(&nntp->in, 1024);
    if (nntp->in.used + nntp->in.left == nntp->in.size) {
        size_t size;

        if (nntp->maxsize > 0 && nntp->in.size >= nntp->maxsize)
            return NNTP_READ_LONG;
        if (nntp->in.size >= 1024 * 1024)
            size = nntp->in.size + 1024 * 1024;
        else
            size = nntp->in.size * 2;
        if (nntp->maxsize > 0 && size > nntp->maxsize)
            size = nntp->maxsize;
        buffer_resize(&nntp->in, size);
    }

    /* Wait for activity. */
    do {
        fd_set mask;
        struct timeval tv;

        FD_ZERO(&mask);
        FD_SET(nntp->in_fd, &mask);
        tv.tv_sec = nntp->timeout;
        tv.tv_usec = 0;
        status = select(nntp->in_fd + 1, &mask, NULL, NULL, &tv);
        if (status == -1 && errno != EINTR)
            return NNTP_READ_ERROR;
    } while (status == -1);

    /* Check for timeout. */
    if (status == 0)
        return NNTP_READ_TIMEOUT;

    /* Do the actual read. */
    count = buffer_read(&nntp->in, nntp->in_fd);
    if (count < 0)
        return NNTP_READ_ERROR;
    else if (count == 0)
        return NNTP_READ_EOF;
    else
        return NNTP_READ_OK;
}

/*
**  Read a single line of data from an NNTP connection.  Puts the
**  nul-terminated line in the second argument; it points into the nntp input
**  buffer and will therefore be invalidated on the next read or on nntp_free,
**  but not until then.  Returns an nntp_status.
*/
enum nntp_status
nntp_read_line(struct nntp *nntp, char **line)
{
    struct buffer *in = &nntp->in;
    enum nntp_status status = NNTP_READ_OK;
    size_t offset;
    size_t start = 0;

    /* Compact the buffer if there are fewer than 128 characters left; this
       limit is chosen somewhat arbitrarily, but note that most NNTP lines
       are fairly short. */
    if (in->used + in->left + 128 >= in->size)
        buffer_compact(in);
    while (status == NNTP_READ_OK) {
        if (buffer_find_string(in, "\r\n", start, &offset)) {
            in->data[in->used + offset] = '\0';
            in->left -= offset + 2;
            *line = in->data + in->used;
            in->used += offset + 2;
            return NNTP_READ_OK;
        }

        /* Back up one character in case \r\n was split across read
           boundaries. */
        start = (in->left > 0) ? in->left - 1 : 0;
        status = nntp_read_data(nntp);
        if (in->used + in->left + 128 >= in->size)
            buffer_compact(in);
    }
    return status;
}


/*
**  Read a response to an NNTP command.  Puts the status code in the second
**  argument and the rest of the line in the third argument, using a status
**  code of 0 if no status code could be found at the beginning of the line.
**  The line will be invalidated on the next read or on nntp_free.  Returns an
**  nntp_status.
*/
enum nntp_status
nntp_read_response(struct nntp *nntp, enum nntp_code *code, char **rest)
{
    char *line;
    enum nntp_status status;

    status = nntp_read_line(nntp, &line);
    if (status != NNTP_READ_OK)
        return status;
    *code = strtol(line, rest, 10);
    if (*rest != line + 3)
        *code = 0;
    else if (isspace((unsigned char) *rest[0]))
        (*rest)++;
    return status;
}


/*
**  Read a command from an NNTP connection.  Takes a cvector as the second
**  argument and stores in it the command and arguments, split on whitespace.
**  The vectors will point into the nntp input buffer and will therefore be
**  invalidated on the next read or on nntp_free, but not until then.  Returns
**  an nntp_status.
*/
enum nntp_status
nntp_read_command(struct nntp *nntp, struct cvector *command)
{
    enum nntp_status status;
    char *line;

    status = nntp_read_line(nntp, &line);
    if (status == NNTP_READ_OK)
        cvector_split_space(line, command);
    return status;
}


/*
**  Read multiline data from an NNTP connection.  Sets the second argument to
**  a pointer to the data (still in wire format) and the third argument to its
**  length.  Returns an nntp_status code.
*/
enum nntp_status
nntp_read_multiline(struct nntp *nntp, char **data, size_t *length)
{
    struct buffer *in = &nntp->in;
    enum nntp_status status = NNTP_READ_OK;
    size_t offset;
    size_t start = 0;

    buffer_compact(in);
    while (status == NNTP_READ_OK) {
        if (buffer_find_string(in, "\r\n.\r\n", start, &offset)) {
            offset += 5;
            in->left -= offset;
            *length = offset;
            *data = in->data + in->used;
            in->used += offset;
            return NNTP_READ_OK;
        }

        /* Back up by up to four characters in case our reads split on the
           boundary of the delimiter. */
        start = (in->left < 4) ? 0 : in->left - 4;
        status = nntp_read_data(nntp);
    }
    return status;
}


/*
**  Flush the NNTP output channel and makes sure that it has no errors.
**  Returns true on success and false on failure.
*/
bool
nntp_flush(struct nntp *nntp)
{
    ssize_t status;

    if (nntp->out.left == 0)
        return true;
    status = xwrite(nntp->out_fd, nntp->out.data, nntp->out.left);
    if (status < 0)
        return false;
    nntp->out.left = 0;
    nntp->out.used = 0;
    return true;
}


/*
**  Send verbatim data to the NNTP stream.  Flush any buffered data before
**  sending and then send the data immediately without buffering (to avoid
**  making an in-memory copy of it).  The caller is responsible for making
**  sure it's properly formatted.  Returns true on success and false on
**  failure.
*/
bool
nntp_write(struct nntp *nntp, const char *buffer, size_t length)
{
    ssize_t status;

    if (!nntp_flush(nntp))
        return false;
    status = xwrite(nntp->out_fd, buffer, length);
    return (status > 0);
}


/*
**  Send a line of data to an NNTP stream, flushing after sending it.  Takes
**  the nntp struct and printf-style arguments for the rest of the line.
**  Returns true on success and false on an error.
*/
bool
nntp_send_line(struct nntp *nntp, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    buffer_append_vsprintf(&nntp->out, format, args);
    va_end(args);
    buffer_append(&nntp->out, "\r\n", 2);
    return nntp_flush(nntp);
}


/*
**  The same as nntp_send_line, but don't flush after sending the repsonse.
**  Used for accumulating multiline responses, mostly.
*/
bool
nntp_send_line_noflush(struct nntp *nntp, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    buffer_append_vsprintf(&nntp->out, format, args);
    va_end(args);
    buffer_append(&nntp->out, "\r\n", 2);
    return nntp_flush(nntp);
}


/*
**  Send a response to an NNTP command, or the opening banner of a server.
**  Takes the nntp struct, a response code, and then printf-style arguments
**  for the rest of the line.  Format may be NULL, indicating nothing should
**  be printed after the response code.  Returns true on success and false on
**  an error.
*/
bool
nntp_respond(struct nntp *nntp, enum nntp_code code, const char *format, ...)
{
    va_list args;

    if (format == NULL)
        buffer_append_sprintf(&nntp->out, "%d\r\n", code);
    else {
        buffer_append_sprintf(&nntp->out, "%d ", code);
        va_start(args, format);
        buffer_append_vsprintf(&nntp->out, format, args);
        va_end(args);
        buffer_append(&nntp->out, "\r\n", 2);
    }
    return nntp_flush(nntp);
}


/*
**  The same as nntp_respond(), but don't flush after sending the response.
**  Used for beginning multiline responses primarily.
*/
void
nntp_respond_noflush(struct nntp *nntp, enum nntp_code code,
                     const char *format, ...)
{
    va_list args;

    if (format == NULL)
        buffer_append_sprintf(&nntp->out, "%d\r\n", code);
    else {
        buffer_append_sprintf(&nntp->out, "%d ", code);
        va_start(args, format);
        buffer_append_vsprintf(&nntp->out, format, args);
        va_end(args);
        buffer_append(&nntp->out, "\r\n", 2);
    }
}
