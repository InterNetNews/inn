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
#include "portable/time.h"
#include <ctype.h>
#include <errno.h>

#include "inn/buffer.h"
#include "inn/innconf.h"
#include "inn/network.h"
#include "inn/nntp.h"
#include "inn/vector.h"
#include "libinn.h"

/* State for an NNTP connection. */
struct nntp {
    int in_fd;
    int out_fd;
    struct buffer in;
    struct buffer out;
    size_t maxsize;
};


/*
**  Allocate a new nntp struct and return it.  Takes the file descriptor to
**  read from, the file descriptor to write to, and the maximum multiline size
**  allowed.
*/
struct nntp *
nntp_new(int in, int out, size_t maxsize)
{
    struct nntp *nntp;

    nntp = xmalloc(sizeof(struct nntp));
    nntp->in_fd = in;
    nntp->out_fd = out;
    nntp->maxsize = maxsize;
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
**  Takes the server name, the port to connect to, and the maximum buffer size
**  to use.  On any failure to connect to the remote host, returns NULL.
*/
struct nntp *
nntp_connect(const char *host, unsigned short port, size_t maxsize)
{
    struct addrinfo hints, *ai;
    char portbuf[16];
    int fd, oerrno;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = NETWORK_AF_HINT;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portbuf, sizeof(portbuf), "%d", port);
    if (getaddrinfo(host, portbuf, &hints, &ai) != 0)
        return NULL;
    fd = network_connect(ai);
    oerrno = errno;
    freeaddrinfo(ai);
    errno = oerrno;
    if (fd < 0)
        return NULL;
    return nntp_new(fd, fd, maxsize);
}


/*
**  Read data from an NNTP connection, as much as is available, putting it
**  into the reader buffer.  Return an nntp_status code.
*/
static enum nntp_status
nntp_read_data(struct nntp *nntp, time_t timeout)
{
    ssize_t count;
    int status;

    /* Resize the buffer if we're out of space, but don't allow the buffer to
       grow longer than maxsize. */
    if (nntp->in.size == 0)
        buffer_resize(&nntp->in, 1024);
    if (nntp->in.used + nntp->in.left == nntp->in.size) {
        size_t size;

        if (nntp->in.size >= nntp->maxsize)
            return NNTP_READ_LONG;
        size = nntp->in.size * 2;
        if (size > nntp->maxsize)
            size = nntp->maxsize;
        buffer_resize(&nntp->in, size);
    }

    /* Wait for activity. */
    do {
        fd_set mask;
        struct timeval tv;

        FD_ZERO(&mask);
        FD_SET(nntp->in_fd, &mask);
        tv.tv_sec = timeout;
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
**  Read a single line of data from an NNTP connection with the given
**  timeout.  Puts the nul-terminated line in the third argument; it points
**  into the nntp input buffer and will therefore be invalidated on the next
**  read or on nntp_free, but not until then.  Returns an nntp_status.
*/
enum nntp_status
nntp_read_line(struct nntp *nntp, time_t timeout, char **line)
{
    struct buffer *in = &nntp->in;
    enum nntp_status status = NNTP_READ_OK;
    size_t offset;
    size_t start = 0;

    if (in->used + in->left == in->size)
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
        status = nntp_read_data(nntp, timeout);
    }
    return status;
}


/*
**  Read a response to an NNTP command.  Puts the status code in the third
**  argument and the rest of the line in the fourth argument, using a status
**  code of 000 if no status code could be found at the beginning of the
**  line.  The line will be invalidated on the next read or on nntp_free.
**  Returns an nntp_status.
*/
enum nntp_status
nntp_read_response(struct nntp *nntp, time_t timeout, enum nntp_code *code,
                   char **rest)
{
    char *line;
    enum nntp_status status;

    status = nntp_read_line(nntp, timeout, &line);
    if (status != NNTP_READ_OK)
        return status;
    *code = strtol(line, rest, 10);
    if (*rest != line + 3)
        *code = 0;
    else if (CTYPE(isspace, *rest[0]))
        (*rest)++;
    return status;
}


/*
**  Read a command from an NNTP connection with the given timeout.  Takes a
**  cvector as the third argument and stores in it the command and arguments,
**  split on whitespace.  The vectors will point into the nntp input buffer
**  and will therefore be invalidated on the next read or on nntp_free, but
**  not until then.  Returns an nntp_status.
*/
enum nntp_status
nntp_read_command(struct nntp *nntp, time_t timeout, struct cvector *command)
{
    enum nntp_status status;
    char *line;

    status = nntp_read_line(nntp, timeout, &line);
    if (status == NNTP_READ_OK)
        cvector_split_space(nntp->in.data + nntp->in.used, command);
    return status;
}


/*
**  Read multiline data from an NNTP connection.  Takes the nntp struct and a
**  timeout (in seconds) and sets the third argument to a pointer to the data
**  (still in wire format) and the fourth argument to its length.  Returns an
**  nntp_status code.
*/
enum nntp_status
nntp_read_multiline(struct nntp *nntp, time_t timeout, char **data,
                    size_t *length)
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
        status = nntp_read_data(nntp, timeout);
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
**  Send a line of data to an NNTP stream, flushing after sending it.  Takes
**  the nntp struct and printf-style arguments for the rest of the line.
**  Returns true on success and false on an error.
*/
bool
nntp_send_line(struct nntp *nntp, const char *format, ...)
{
    va_list args;
    bool done;

    va_start(args, format);
    done = buffer_vsprintf(&nntp->out, true, format, args);
    va_end(args);
    if (!done) {
        va_start(args, format);
        buffer_vsprintf(&nntp->out, true, format, args);
        va_end(args);
    }
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
    bool done;

    if (format == NULL)
        buffer_sprintf(&nntp->out, true, "%d\r\n", code);
    else {
        buffer_sprintf(&nntp->out, true, "%d ", code);
        va_start(args, format);
        done = buffer_vsprintf(&nntp->out, true, format, args);
        va_end(args);
        if (!done) {
            va_start(args, format);
            buffer_vsprintf(&nntp->out, true, format, args);
            va_end(args);
        }
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
    bool done;

    if (format == NULL)
        buffer_sprintf(&nntp->out, true, "%d\r\n", code);
    else {
        buffer_sprintf(&nntp->out, true, "%d ", code);
        va_start(args, format);
        done = buffer_vsprintf(&nntp->out, true, format, args);
        va_end(args);
        if (!done) {
            va_start(args, format);
            buffer_vsprintf(&nntp->out, true, format, args);
            va_end(args);
        }
        buffer_append(&nntp->out, "\r\n", 2);
    }
}
