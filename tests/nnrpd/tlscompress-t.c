/*
**  Test suite for reading client input in nnrpd when TLS-level compression
**  is negotiated.
**
**  TLS-level compression sets compression_layer_on together with
**  tls_compression_on, but leaves the zlib streams of COMPRESS DEFLATE
**  (RFC 8054) uninitialized.  Reading the next command must not use them.
**  Uses the real nnrpd/line.c and nnrpd/zlib.c, and a pipe as the connection
**  with the client.  Each line is read in a child process so that a crash is
**  reported as a failure.
*/

#define LIBTEST_NEW_FORMAT 1

#include "portable/system.h"

#include <sys/wait.h>

#include "tap/basic.h"

/* Define the nnrpd globals declared with EXTERN here. */
#if defined(__llvm__) || defined(__clang__)
#    pragma GCC diagnostic ignored "-Wmissing-variable-declarations"
#endif
#define MAINLINE 1
#include "../../nnrpd/nnrpd.h"
#include "../../nnrpd/tls.h"

/* Globals defined in other nnrpd files. */
#if defined(HAVE_OPENSSL) || defined(HAVE_SASL)
bool encryption_layer_on = false;
#endif
#ifdef HAVE_OPENSSL
SSL *tls_conn = NULL;
int tls_cipher_usebits = 0;
char *tls_peer_CN = NULL;
#endif
#ifdef HAVE_SASL
sasl_conn_t *sasl_conn = NULL;
int sasl_ssf = 0;
int sasl_maxout = NNTP_MAXLEN_COMMAND;
#endif


#ifndef HAVE_ZLIB

int
main(void)
{
    skip_all("zlib support not configured");
    return 0;
}

#else

static ACCESSGROUP access_group;


/*
**  Make data the whole input of the client, on standard input.
*/
static void
feed(const void *data, size_t len)
{
    int fds[2];

    if (pipe(fds) < 0)
        sysbail("cannot create pipe");
    if (dup2(fds[0], STDIN_FILENO) < 0)
        sysbail("cannot duplicate pipe");
    close(fds[0]);
    if (write(fds[1], data, len) != (ssize_t) len)
        sysbail("cannot write to pipe");
    close(fds[1]);
}


/*
**  Compress data with raw DEFLATE, as a client does after COMPRESS DEFLATE.
**  Returns the length of the compressed data.
*/
static size_t
deflate_raw(const char *data, unsigned char *out, size_t size)
{
    z_stream zs;
    size_t len;

    memset(&zs, 0, sizeof(zs));
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8,
                     Z_DEFAULT_STRATEGY)
        != Z_OK)
        bail("cannot initialize deflate");
    zs.next_in = (unsigned char *) data;
    zs.avail_in = (uInt) strlen(data);
    zs.next_out = out;
    zs.avail_out = (uInt) size;
    if (deflate(&zs, Z_SYNC_FLUSH) != Z_OK)
        bail("cannot compress");
    len = size - zs.avail_out;
    deflateEnd(&zs);
    return len;
}


/*
**  Read the next command nnrpd would process, in a child process.  Stores
**  the line read in buf (empty if none) and returns the wait status of the
**  child.
*/
static int
read_command(char *buf, size_t size)
{
    int fds[2];
    int status;
    pid_t pid;
    ssize_t n;
    size_t got = 0;

    if (pipe(fds) < 0)
        sysbail("cannot create pipe");
    fflush(stdout);
    pid = fork();
    if (pid < 0)
        sysbail("cannot fork");
    if (pid == 0) {
        const char *p = NULL;
        size_t len = 0;
        READTYPE r;

        close(fds[0]);
        r = line_read(&NNTPline, 5, &p, &len, NULL);
        if (r == RTok && write(fds[1], p, len) != (ssize_t) len)
            _exit(2);
        _exit(r == RTok ? 0 : 1);
    }
    close(fds[1]);
    while (got < size - 1) {
        n = read(fds[0], buf + got, size - 1 - got);
        if (n <= 0)
            break;
        got += (size_t) n;
    }
    buf[got] = '\0';
    close(fds[0]);
    if (waitpid(pid, &status, 0) != pid)
        sysbail("cannot wait for child");
    return status;
}


/*
**  Check that the child read the expected command and exited normally.
*/
static void
check_command(const char *what, const char *expected)
{
    char buf[256];
    int status;

    status = read_command(buf, sizeof(buf));
    if (WIFSIGNALED(status))
        diag("%s: reader killed by signal %d", what, WTERMSIG(status));
    ok(WIFEXITED(status) && WEXITSTATUS(status) == 0,
       "%s: command read without a crash", what);
    is_string(expected, buf, "%s: command is %s", what, expected);
}


int
main(void)
{
    unsigned char compressed[256];
    size_t len;

    access_group.clienttimeout = 10;
    access_group.localmaxartsize = 1000000;
    PERMaccessconf = &access_group;
    strlcpy(Client.host, "localhost", sizeof(Client.host));
    line_init(&NNTPline);

    plan(6);

    /* No compression layer. */
    compression_layer_on = false;
    tls_compression_on = false;
    line_reset(&NNTPline);
    feed("DATE\r\n", 6);
    check_command("no compression", "DATE");

    /* TLS-level compression, as set after the TLS handshake: the zlib
     * streams of COMPRESS DEFLATE are not initialized. */
    compression_layer_on = true;
    tls_compression_on = true;
    line_reset(&NNTPline);
    feed("DATE\r\n", 6);
    check_command("TLS compression", "DATE");

    /* COMPRESS DEFLATE still decompresses the input. */
    if (!zlib_init())
        bail("cannot initialize zlib streams");
    compression_layer_on = true;
    tls_compression_on = false;
    line_reset(&NNTPline);
    len = deflate_raw("DATE\r\n", compressed, sizeof(compressed));
    feed(compressed, len);
    check_command("COMPRESS DEFLATE", "DATE");

    line_free(&NNTPline);
    return 0;
}

#endif /* HAVE_ZLIB */
