/*
**  Test suite for oversized articles on an innd NNTP channel.
**
**  Articles larger than maxartsize are sent by a configured peer through a
**  socket pair to the real channel reader.  The reply, the notice, the
**  article log line, the history entry and the rejected size must be those
**  of the whole article, the stream must stay in sync, and the input buffer
**  must not grow with the size of the article.  The article log line must
**  also name the peer correctly when incoming.conf is reloaded while the
**  article is being received.
*/

#include "portable/system.h"

#include "portable/socket.h"
#include <poll.h>
#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif

#include "inn/buffer.h"
#include "inn/fdflag.h"
#include "inn/history.h"
#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "tap/basic.h"
#include "tap/messages.h"

#include "../../innd/innd.h"
#include "innperl.h"

#define MAXARTSIZE 8192
#define WRITESIZE  1024
#define BUFLIMIT   (4 * MAXARTSIZE)

/* Largest number of bytes used in, and largest size of, the input buffer. */
static size_t peakused;
static size_t peaksize;

/* Let the channel read and process everything sent so far. */
static void
process(CHANNEL *cp)
{
    struct pollfd pfd;

    pfd.fd = cp->fd;
    pfd.events = POLLIN;
    while (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
        (*cp->Reader)(cp);
        if (cp->In.used > peakused)
            peakused = cp->In.used;
        if (cp->In.size > peaksize)
            peaksize = cp->In.size;
    }
}

/* Send length bytes to the channel, WRITESIZE bytes per write. */
static void
send_data(CHANNEL *cp, int fd, const char *data, size_t length)
{
    size_t n;

    while (length > 0) {
        n = length < WRITESIZE ? length : WRITESIZE;
        if (write(fd, data, n) != (ssize_t) n)
            sysbail("cannot write to channel");
        process(cp);
        data += n;
        length -= n;
    }
}

static void
send_string(CHANNEL *cp, int fd, const char *data)
{
    send_data(cp, fd, data, strlen(data));
}

/* Return the replies of the channel not read yet. */
static char *
read_replies(int fd)
{
    static char buff[1024];
    ssize_t n;

    n = read(fd, buff, sizeof(buff) - 1);
    if (n < 0)
        n = 0;
    buff[n] = '\0';
    return buff;
}

/* Return the article log written from offset start on. */
static char *
read_log(long start)
{
    char *text;
    long end;

    fflush(Log);
    end = ftell(Log);
    if (end < 0)
        sysbail("cannot get log position");
    text = xcalloc(end - start + 1, 1);
    if (fseek(Log, start, SEEK_SET) < 0)
        sysbail("cannot seek in log");
    if (fread(text, 1, end - start, Log) != (size_t) (end - start))
        sysbail("cannot read log");
    if (fseek(Log, 0, SEEK_END) < 0)
        sysbail("cannot seek in log");
    return text;
}

static long
log_position(void)
{
    long pos;

    fflush(Log);
    pos = ftell(Log);
    if (pos < 0)
        sysbail("cannot get log position");
    return pos;
}

/* Fill size bytes (a multiple of 64) with lines of 64 bytes, each starting
   with lead and ending in CRLF. */
static char *
make_lines(size_t size, const char *lead, char fill)
{
    char *text;
    size_t i, n;

    n = strlen(lead);
    text = xmalloc(size + 1);
    for (i = 0; i < size; i += 64) {
        memcpy(text + i, lead, n);
        memset(text + i + n, fill, 62 - n);
        memcpy(text + i + 62, "\r\n", 2);
    }
    text[size] = '\0';
    return text;
}

/* Build the header of a test article, with extra header lines. */
static char *
make_header(const char *msgid, const char *extra)
{
    char *header;

    xasprintf(&header,
              "Path: peer.example.invalid!not-for-mail\r\n"
              "From: test@example.invalid\r\n"
              "Newsgroups: example.test\r\n"
              "Date: Wed, 25 Nov 2026 00:00:00 +0000\r\n"
              "Subject: test\r\n"
              "Message-ID: %s\r\n"
              "%s"
              "\r\n",
              msgid, extra);
    return header;
}

/* Write incoming.conf in dir with one peer, as ctlinnd reload would find it,
   and load it. */
static void
load_peer(const char *dir, const char *label, const char *hostname)
{
    char *path;
    FILE *f;

    path = concatpath(dir, "incoming.conf");
    f = fopen(path, "w");
    if (f == NULL)
        sysbail("cannot create %s", path);
    fprintf(f, "peer %s {\n    hostname: \"%s\"\n}\n", label, hostname);
    if (fclose(f) != 0)
        sysbail("cannot write %s", path);
    free(path);
    RCreadlist();
}

/* Check the notice and the article log line of a rejected article. */
static void
check_logs(int n, const char *peer, const char *msgid, int code, size_t wire,
           long logpos)
{
    char *expected, *text;

    xasprintf(&expected, "internal rejecting huge article (%lu > %d)",
              (unsigned long) wire, MAXARTSIZE);
    ok(n, errors != NULL && strstr(errors, expected) != NULL);
    if (errors == NULL || strstr(errors, expected) == NULL)
        diag("notices: %s", errors != NULL ? errors : "(none)");
    free(expected);

    xasprintf(&expected,
              " %s %s %d Article of %lu bytes exceeds local limit of %d "
              "bytes\n",
              peer, msgid, code, (unsigned long) wire, MAXARTSIZE);
    text = read_log(logpos);
    ok(n + 1, strstr(text, expected) != NULL);
    if (strstr(text, expected) == NULL)
        diag("article log: %s", text);
    free(expected);
    free(text);
}

/* Check the size counted as rejected. */
static void
check_size(int n, const CHANNEL *cp, unsigned long long before, size_t wire)
{
    ok(n, cp->RejectSize - before == wire);
    if (cp->RejectSize - before != wire)
        diag("rejected size %llu, article %lu", cp->RejectSize - before,
             (unsigned long) wire);
}

/* Check the input buffer peak and report it. */
static void
check_peak(int n, const char *what)
{
    diag("%s: input buffer peak %lu bytes used, %lu allocated", what,
         (unsigned long) peakused, (unsigned long) peaksize);
    ok(n, peakused < BUFLIMIT);
}

int
main(void)
{
    char tmpdir[] = "maxartsize-XXXXXX";
    char *histpath, *header, *body, *pad, *expected;
    int fds[2];
    CHANNEL *cp;
    size_t i, wire, bodysize;
    unsigned long long rejectsize;
    long logpos;
    struct sockaddr_in *sin;

    test_init(24);

    /* Before innconf_read, which points TMPDIR to pathtmp. */
    Log = tmpfile();
    if (Log == NULL)
        sysbail("cannot create temporary log");

    if (access("../data/etc/inn.conf", F_OK) < 0)
        if (access("data/etc/inn.conf", F_OK) == 0)
            if (chdir("innd") != 0)
                sysbail("cannot cd to innd");
    if (!innconf_read("../data/etc/inn.conf"))
        bail("cannot read inn.conf");
    innconf->maxartsize = MAXARTSIZE;
    innconf->logipaddr = false;
    message_handlers_notice(0);
    CHANsetup(64);
    gettimeofday(&Now, NULL);
    WIPsetup();
    buffer_set(&Path, "example.com!others", strlen("example.com!others") + 1);
    Path.used += strlen("example.com!");
    ARTsetup();
#if DO_PERL
    /* The message-ID filter hook needs a Perl interpreter, as in innd. */
    PERLsetup(NULL, NULL, "filter_art");
#endif

    if (mkdtemp(tmpdir) == NULL)
        sysbail("cannot create temporary directory");
    histpath = concatpath(tmpdir, "history");
    History = HISopen(histpath, "hisv6", HIS_CREAT | HIS_RDWR);
    if (History == NULL)
        bail("cannot create history %s", histpath);

    /* A configured peer that needs no authentication. */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0)
        sysbail("cannot create socket pair");
    if (!fdflag_nonblocking(fds[1], true))
        sysbail("cannot set socket nonblocking");
    cp = NCcreate(fds[0], false, true);
    if (cp == NULL)
        bail("cannot create NNTP channel");

    /* IHAVE with a body of 256 KiB. */
    bodysize = 256 * 1024;
    body = make_lines(bodysize, "", 'a');
    header = make_header("<ihave@example.invalid>", "");
    wire = strlen(header) + bodysize + 3;
    rejectsize = cp->RejectSize;
    logpos = log_position();
    peakused = peaksize = 0;
    errors_capture();
    send_string(cp, fds[1], "IHAVE <ihave@example.invalid>\r\n");
    send_string(cp, fds[1], header);
    send_data(cp, fds[1], body, bodysize);
    send_string(cp, fds[1], ".\r\n");
    errors_uncapture();
    message_handlers_notice(0);
    xasprintf(&expected,
              "335 Send it\r\n"
              "437 Article of %lu bytes exceeds local limit of %d bytes\r\n",
              (unsigned long) wire, MAXARTSIZE);
    ok_string(1, expected, read_replies(fds[1]));
    free(expected);
    check_logs(2, "peer.example.invalid", "<ihave@example.invalid>", 437, wire,
               logpos);
    ok(4, HIScheck(History, "<ihave@example.invalid>"));
    check_size(5, cp, rejectsize, wire);
    check_peak(6, "IHAVE 256 KiB");
    ok_int(7, CSgetcmd, cp->State);
    free(header);
    free(body);

    /* Streaming with a body of 4 MiB.  The terminator arrives one byte per
       read, and its last byte comes in the same write as the next CHECK. */
    cp->Streaming = true;
    bodysize = 4 * 1024 * 1024;
    body = make_lines(bodysize, "", 'a');
    header = make_header("<stream@example.invalid>", "");
    wire = strlen(header) + bodysize + 3;
    rejectsize = cp->RejectSize;
    logpos = log_position();
    peakused = peaksize = 0;
    errors_capture();
    send_string(cp, fds[1], "TAKETHIS <stream@example.invalid>\r\n");
    send_string(cp, fds[1], header);
    send_data(cp, fds[1], body, bodysize - 2);
    send_string(cp, fds[1], "\r");
    send_string(cp, fds[1], "\n");
    send_string(cp, fds[1], ".");
    send_string(cp, fds[1], "\r");
    send_string(cp, fds[1], "\nCHECK <next@example.invalid>\r\n");
    errors_uncapture();
    message_handlers_notice(0);
    ok_string(8,
              "439 <stream@example.invalid>\r\n"
              "238 <next@example.invalid> Send it\r\n",
              read_replies(fds[1]));
    check_logs(9, "peer.example.invalid", "<stream@example.invalid>", 439,
               wire, logpos);
    ok(11, HIScheck(History, "<stream@example.invalid>"));
    check_size(12, cp, rejectsize, wire);
    check_peak(13, "TAKETHIS 4 MiB");
    ok_int(14, CSgetcmd, cp->State);
    free(header);
    free(body);
    cp->Streaming = false;

    /* IHAVE whose header alone is larger than maxartsize: 64 KiB of folded
       lines after the Message-ID header field. */
    pad = make_lines(64 * 1024, "\t", 'b');
    memcpy(pad, "X-Pad: ", 7);
    header = make_header("<header@example.invalid>", pad);
    body = make_lines(1024, "", 'a');
    wire = strlen(header) + 1024 + 3;
    rejectsize = cp->RejectSize;
    logpos = log_position();
    peakused = peaksize = 0;
    errors_capture();
    send_string(cp, fds[1], "IHAVE <header@example.invalid>\r\n");
    send_string(cp, fds[1], header);
    send_string(cp, fds[1], body);
    send_string(cp, fds[1], ".\r\n");
    errors_uncapture();
    message_handlers_notice(0);
    xasprintf(&expected,
              "335 Send it\r\n"
              "437 Article of %lu bytes exceeds local limit of %d bytes\r\n",
              (unsigned long) wire, MAXARTSIZE);
    ok_string(15, expected, read_replies(fds[1]));
    free(expected);
    check_logs(16, "peer.example.invalid", "<header@example.invalid>", 437,
               wire, logpos);
    ok(18, HIScheck(History, "<header@example.invalid>"));
    check_size(19, cp, rejectsize, wire);
    check_peak(20, "IHAVE 64 KiB header");
    ok_int(21, CSgetcmd, cp->State);
    free(header);
    free(body);
    free(pad);

    /* IHAVE with a body of 64 KiB from a peer listed in incoming.conf, with
       logipaddr true (the default).  incoming.conf is reloaded, without that
       peer, while the body is being received.  The article log line then
       names the peer by its address, as innd does when the reload happens
       before the article. */
    innconf->logipaddr = true;
    innconf->pathetc = xstrdup(tmpdir);
    load_peer(tmpdir, "feeder", "127.0.0.1");
    sin = (struct sockaddr_in *) (void *) &cp->Address;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bodysize = 64 * 1024;
    body = make_lines(bodysize, "", 'a');
    header = make_header("<reload@example.invalid>", "");
    wire = strlen(header) + bodysize + 3;
    rejectsize = cp->RejectSize;
    logpos = log_position();
    errors_capture();
    send_string(cp, fds[1], "IHAVE <reload@example.invalid>\r\n");
    send_string(cp, fds[1], header);
    send_data(cp, fds[1], body, bodysize / 2);
    load_peer(tmpdir, "other", "127.0.0.2");
    send_data(cp, fds[1], body + bodysize / 2, bodysize / 2);
    send_string(cp, fds[1], ".\r\n");
    errors_uncapture();
    message_handlers_notice(0);
    xasprintf(&expected,
              "335 Send it\r\n"
              "437 Article of %lu bytes exceeds local limit of %d bytes\r\n",
              (unsigned long) wire, MAXARTSIZE);
    ok_string(22, expected, read_replies(fds[1]));
    free(expected);
    check_logs(23, "127.0.0.1", "<reload@example.invalid>", 437, wire, logpos);
    free(header);
    free(body);

    close(fds[1]);
    CHANclose(cp, CHANname(cp));
    HISclose(History);
    History = NULL;
    {
        static const char *const suffixes[] = {"", ".dir", ".index", ".hash",
                                               ".pag"};
        char *path;

        for (i = 0; i < ARRAY_SIZE(suffixes); i++) {
            path = concat(histpath, suffixes[i], (char *) 0);
            unlink(path);
            free(path);
        }
        path = concatpath(tmpdir, "incoming.conf");
        unlink(path);
        free(path);
        rmdir(tmpdir);
    }
    free(histpath);
    free(errors);
    return 0;
}
