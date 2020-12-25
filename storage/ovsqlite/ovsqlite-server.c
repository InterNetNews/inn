/*  $Id$
**
**  Daemon server to access overview database based on SQLite.
**
**  Original implementation written by Bo Lindbergh (2020-12-17).
**  <2bfjdsla52kztwejndzdstsxl9athp@gmail.com>
*/

#include "config.h"
#include "inn/messages.h"
#include "ovsqlite-private.h"

#ifdef HAVE_SQLITE3

#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <syslog.h>
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#include <time.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <sys/stat.h>

#include "portable/setproctitle.h"
#include "portable/socket.h"
#ifdef HAVE_UNIX_DOMAIN_SOCKETS
# include "portable/socket-unix.h"
#endif

#include "inn/libinn.h"
#include "inn/concat.h"
#include "inn/fdflag.h"
#include "inn/xmalloc.h"
#include "inn/innconf.h"
#include "inn/confparse.h"
#include "inn/storage.h"

#include "sql-main.h"
#include "sql-init.h"

#define OVSQLITE_DB_FILE "ovsqlite.db"

#ifdef HAVE_ZLIB

#define USE_DICTIONARY 1

#include <zlib.h>

static z_stream deflation;
static z_stream inflation;

static buffer_t *flate;

static uint32_t const pack_length_bias[5] =
{
             0,
          0x80,
        0x4080,
      0x204080,
    0x10204080,
};

#ifdef USE_DICTIONARY

static char const basedict_format[] =
    "\tRe: =?UTF-8?Q? =?UTF-8?B? the The and for "
    "\tMon, \tTue, \tWed, \tThu, \tFri, \tSat, \tSun, "
    "Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec "
    "GMT\t (UTC)\tXref: %s ";

static char dictionary[0x8000];
static unsigned int basedict_len;

#endif /* USE_DICTIONARY */

#endif /* HAVE_ZLIB */

enum {
    client_flag_init    = 0x01,
    client_flag_term    = 0x02,
};

#define INITIAL_CAPACITY 0x400

typedef struct client_t {
    uint8_t flags;
    bool cutofflow;
    bool check_forgotten;
    int sock;
    uint32_t mode;
    time_t expiration_start;
    buffer_t *request;
    buffer_t *response;
} client_t;

#ifndef HAVE_UNIX_DOMAIN_SOCKETS
static ovsqlite_port port;
#endif /* ! HAVE_UNIX_DOMAIN_SOCKETS */

static char *pidfile = NULL;
static int listensock = -1;
static int maxsock = -1;
static client_t *clients = NULL;
static size_t client_capacity, client_count;
static fd_set read_fds, write_fds;
static bool volatile terminating;

static sqlite3 *connection;
static sql_main_t sql_main;

static bool use_compression;
static unsigned long pagesize;
static unsigned long cachesize;
static struct timeval transaction_time_limit = {10, 0};
static unsigned long transaction_row_limit = 10000;

static bool in_transaction;
static unsigned int transaction_rowcount;
static struct timeval next_commit;


static void timeval_normalise(
    struct timeval *t)
{
    time_t carry;

    carry = t->tv_usec/1000000;
    if (carry) {
        t->tv_sec += carry;
        t->tv_usec -= carry*1000000;
    }
    if (t->tv_sec>0 && t->tv_usec<0) {
        t->tv_sec--;
        t->tv_usec += 1000000;
    } else if (t->tv_sec<0 && t->tv_usec>0) {
        t->tv_sec++;
        t->tv_usec -= 1000000;
    }
}

static struct timeval timeval_sum(
    struct timeval a,
    struct timeval b)
{
    struct timeval result;

    timeval_normalise(&a);
    timeval_normalise(&b);
    result.tv_sec = a.tv_sec+b.tv_sec;
    result.tv_usec = a.tv_usec+b.tv_usec;
    timeval_normalise(&result);
    return result;
}

static struct timeval timeval_difference(
    struct timeval a,
    struct timeval b)
{
    struct timeval result;

    timeval_normalise(&a);
    timeval_normalise(&b);
    result.tv_sec = a.tv_sec-b.tv_sec;
    result.tv_usec = a.tv_usec-b.tv_usec;
    timeval_normalise(&result);
    return result;
}

static void catcher(
    int sig UNUSED)
{
    terminating = true;
}

static void catch_signals(void)
{
    xsignal_norestart(SIGINT, catcher);
    xsignal_norestart(SIGTERM, catcher);
    xsignal_norestart(SIGHUP, catcher);
    xsignal(SIGPIPE, SIG_IGN);
}

static void resetclear(
    sqlite3_stmt *stmt)
{
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
}

static client_t *add_client(
    int sock)
{
    client_t *result;

    if (client_count>=client_capacity) {
        size_t new_client_capacity;

        new_client_capacity = (client_count+1)*3/2;
        clients = xreallocarray(
            clients, new_client_capacity, sizeof (client_t));
        client_capacity = new_client_capacity;
    }

    result = clients+client_count;
    memset(result, 0, sizeof (client_t));
    result->sock = sock;
    result->flags = client_flag_init;
    result->request = buffer_new();
    buffer_resize(result->request, INITIAL_CAPACITY);
    result->response = buffer_new();
    buffer_resize(result->response, INITIAL_CAPACITY);

    client_count++;
    FD_SET(sock, &read_fds);
    if (sock>maxsock)
        maxsock = sock;
    return result;
}

static void del_client(
    client_t *client)
{
    size_t ix;
    int sock;

    ix = client-clients;
    if (ix>=client_count || client!=clients+ix)
        return;
    sock = client->sock;
    FD_CLR(sock, &read_fds);
    FD_CLR(sock, &write_fds);
    close(sock);
    buffer_free(client->request);
    buffer_free(client->response);
    if (ix+1<client_count)
        *client = clients[client_count-1];
    client_count--;
    if (sock==maxsock) {
        int new_maxsock;

        new_maxsock = listensock;
        for (ix = 0; ix<client_count; ix++) {
            sock = clients[ix].sock;
            if (sock>new_maxsock)
                new_maxsock = sock;
        }
        maxsock = new_maxsock;
    }
}

#ifdef HAVE_UNIX_DOMAIN_SOCKETS

static void make_unix_listener(void)
{
    char *path;
    struct sockaddr_un sa;

    listensock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listensock==-1)
        sysdie("cannot create socket");
    memset(&sa, 0, sizeof sa);
    sa.sun_family = AF_UNIX;
    path = concatpath(innconf->pathrun, OVSQLITE_SERVER_SOCKET);
    strlcpy(sa.sun_path, path, sizeof(sa.sun_path));
    unlink(sa.sun_path);
    free(path);
    if (bind(listensock, (struct sockaddr *)&sa, SUN_LEN(&sa))!=0)
        sysdie("cannot bind socket");
}

#else /* ! HAVE_UNIX_DOMAIN_SOCKETS */

static void make_inet_listener(void)
{
    char *path;
    struct sockaddr_in sa;
    int status;
    void const *cookie;
    int ret;
    socklen_t salen;
    int fd;

    sqlite3_bind_int(sql_main.random, 1, OVSQLITE_COOKIE_LENGTH);
    status = sqlite3_step(sql_main.random);
    if (status!=SQLITE_ROW) {
        die("SQLite error while generating random cookie: %s",
            sqlite3_errmsg(connection));
    }
    cookie = sqlite3_column_blob(sql_main.random, 0);
    if (!cookie) {
        status = sqlite3_errcode(connection);
        if (status!=SQLITE_OK) {
            die("SQLite error while generating random cookie: %s",
                sqlite3_errmsg(connection));
        } else {
            die("unexpected NULL result while generating random cookie");
        }
    }
    if (sqlite3_column_bytes(sql_main.random, 0)!=OVSQLITE_COOKIE_LENGTH)
        die("unexpected result size while generating random cookie");
    memcpy(port.cookie, cookie, OVSQLITE_COOKIE_LENGTH);
    resetclear(sql_main.random);

    listensock = socket(AF_INET, SOCK_STREAM, 0);
    if (listensock==-1)
        sysdie("cannot create socket");
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(0x7f000001UL);
    if (bind(listensock, (struct sockaddr *)&sa, sizeof sa)!=0)
        sysdie("cannot bind socket");
    salen = sizeof sa;
    if (getsockname(listensock, (struct sockaddr *)&sa, &salen)!=0)
        sysdie("cannot extract socket port number");
    port.port = sa.sin_port;

    path = concatpath(innconf->pathrun, OVSQLITE_SERVER_PORT);
    unlink(path);
    fd = open(path, O_CREAT|O_TRUNC|O_WRONLY, 0440);
    if (fd==-1)
        sysdie("cannot create port file %s", path);
    ret = write(fd, &port, sizeof port);
    if (ret==-1)
        sysdie("cannot write port file %s", path);
    if (ret!=sizeof port)
        die("cannot write port file %s: Short write", path);
    if (close(fd)!=0)
        sysdie("cannot close port file %s", path);
    free(path);
}

#endif /* ! HAVE_UNIX_DOMAIN_SOCKETS */

static void make_listener(void)
{
#ifdef HAVE_UNIX_DOMAIN_SOCKETS
    make_unix_listener();
#else
    make_inet_listener();
#endif
    if (listen(listensock, MAXLISTEN)==-1)
        sysdie("cannot listen on socket");
    fdflag_nonblocking(listensock, 1);
    FD_SET(listensock, &read_fds);
    maxsock = listensock;
}

static void make_pidfile(void)
{
    FILE *pf;

    pidfile = concatpath(innconf->pathrun, OVSQLITE_SERVER_PIDFILE);
    pf = fopen(pidfile, "w");
    if (!pf)
        sysdie("cannot create PID file");
    if (fprintf(pf, "%ld\n", (long)getpid())<0)
        sysdie("cannot write PID file");
    if (fclose(pf))
        sysdie("cannot close PID file");
}

static void load_config(void)
{
    char *path;
    struct config_group *top;
    double timelimit;

    if (strcmp(innconf->ovmethod, "ovsqlite"))
        die("ovmethod not set to ovsqlite in inn.conf");
    path = concatpath(innconf->pathetc, "ovsqlite.conf");
    top = config_parse_file(path);
    free(path);
    if (top) {
        config_param_boolean(top, "compress", &use_compression);
        config_param_unsigned_number(
            top, "pagesize", &pagesize);
        config_param_unsigned_number(
            top, "cachesize", &cachesize);
        if (config_param_real(top, "transtimelimit", &timelimit)) {
            transaction_time_limit.tv_sec = timelimit;
            transaction_time_limit.tv_usec =
                (timelimit-transaction_time_limit.tv_sec)*1E6+0.5;
            timeval_normalise(&transaction_time_limit);
        }
        config_param_unsigned_number(
            top, "transrowlimit", &transaction_row_limit);

        config_free(top);
    }
}

#ifdef HAVE_ZLIB

static void setup_compression(
    bool init)
{
    int status;

    deflation.zalloc = Z_NULL;
    deflation.zfree = Z_NULL;
    deflation.opaque = Z_NULL;
    deflation.next_in = Z_NULL;
    deflation.avail_in = 0;
    status = deflateInit(&deflation, Z_DEFAULT_COMPRESSION);
    if (status!=Z_OK) {
        if (deflation.msg) {
            die("cannot set up compression: %s", deflation.msg);
        } else {
            die("cannot set up compression");
        }
    }

    inflation.zalloc = Z_NULL;
    inflation.zfree = Z_NULL;
    inflation.opaque = Z_NULL;
    inflation.next_in = Z_NULL;
    inflation.avail_in = 0;
    status = inflateInit(&inflation);
    if (status!=Z_OK) {
        if (inflation.msg) {
            die("cannot set up decompression: %s", inflation.msg);
        } else {
            die("cannot set up decompression");
        }
    }

#ifdef USE_DICTIONARY
    if (init) {
        basedict_len = snprintf(
            dictionary, sizeof dictionary,
            basedict_format, innconf->pathhost);
        sqlite3_bind_text(sql_main.setmisc, 1, "basedict", -1, SQLITE_STATIC);
        sqlite3_bind_blob(
            sql_main.setmisc, 2, dictionary, basedict_len, SQLITE_STATIC);
        status = sqlite3_step(sql_main.setmisc);
        if (status!=SQLITE_DONE) {
            die("cannot store compression dictionary: %s",
                sqlite3_errmsg(connection));
        }
        resetclear(sql_main.setmisc);
    } else {
        void const *dict;
        size_t size;

        sqlite3_bind_text(sql_main.getmisc, 1, "basedict", -1, SQLITE_STATIC);
        status = sqlite3_step(sql_main.getmisc);
        if (status!=SQLITE_ROW) {
            die("cannot load compression dictionary: %s",
                sqlite3_errmsg(connection));
        }
        dict = sqlite3_column_blob(sql_main.getmisc, 0);
        size = sqlite3_column_bytes(sql_main.getmisc, 0);
        if (!dict || size>=sizeof dictionary)
            die("invalid compression dictionary in database");
        memcpy(dictionary, dict, size);
        basedict_len = size;
        resetclear(sql_main.getmisc);
    }
#endif

    flate = buffer_new();
}

static void pack_length(
    z_stream *stream,
    uint32_t length)
{
    unsigned int lenlen, n;
    uint8_t *walk;

    lenlen = 1;
    while (lenlen<5 && length>=pack_length_bias[lenlen])
        lenlen++;
    length -= pack_length_bias[lenlen-1];
    if (stream->avail_out<lenlen) {
        die("BUG!  pack_length called with insufficient buffer space (%u<%u)",
            stream->avail_out,lenlen);
    }
    walk = stream->next_out+lenlen;
    stream->next_out = walk;
    stream->avail_out -= lenlen;
    for (n = lenlen; n>1; n--) {
        *--walk = length;
        length >>= 8;
    }
    *--walk = length | (~0U<<(9-lenlen));
}

static uint32_t unpack_length(
    z_stream *stream)
{
    uint8_t *walk;
    unsigned int c, lenlen, n;
    uint32_t length;

    if (stream->avail_in<=0)
        die("BUG!  unpack_length called with empty buffer");
    walk = stream->next_in;
    c = *walk++;
    lenlen = 1;
    while (c & (1U<<(8-lenlen)))
        lenlen++;
    if (lenlen>5 || lenlen>stream->avail_in)
        return ~0U;
    length = c&~(~0U<<(8-lenlen));
    for (n=lenlen-1; n>0; n--)
        length = (length<<8) | *walk++;
    length += pack_length_bias[lenlen-1];
    stream->next_in = walk;
    stream->avail_in -= lenlen;
    return length;
}

#ifdef USE_DICTIONARY

static unsigned int make_dict(
    char const *groupname,
    int groupname_len,
    uint64_t artnum)
{
    sqlite3_snprintf(
        sizeof dictionary-basedict_len, dictionary+basedict_len,
        "%.*s:%llu\r\n",
        groupname_len, groupname, artnum);
    return basedict_len+strlen(dictionary+basedict_len);
}


#endif

#endif /* HAVE_ZLIB */

static void open_db(void)
{
    char *path;
    struct stat sb;
    int status;
    char *errmsg;
    bool init;
    char sqltext[64];

    path = concatpath(innconf->pathoverview, OVSQLITE_DB_FILE);
    init = stat(path, &sb)==-1;
    if (init) {
        sql_init_t sql_init;

        status = sqlite3_open_v2(
            path,
            &connection,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
            NULL);
        if (status != SQLITE_OK)
            die("cannot create database: %s", sqlite3_errstr(status));
        sqlite3_extended_result_codes(connection, 1);
        status = sqlite_helper_init(
            &sql_init_helper,
            (sqlite3_stmt **)&sql_init,
            connection,
            0,
            &errmsg);
        if (status != SQLITE_OK)
            die("cannot initialize new database: %s", errmsg);
        if (pagesize) {
            unsigned long default_pagesize;

            status = sqlite3_step(sql_init.getpagesize);
            if (status != SQLITE_ROW)
                die("cannot get pagesize: %s", sqlite3_errmsg(connection));
            default_pagesize = sqlite3_column_int64(sql_init.getpagesize, 0);
            sqlite3_reset(sql_init.getpagesize);
            if (default_pagesize!=pagesize) {
                /* can't use placeholders for pragma arguments, alas */
                snprintf(sqltext, sizeof sqltext,
                         "pragma page_size=%lu; vacuum;",
                         pagesize);
                status = sqlite3_exec(connection, sqltext, 0, NULL, &errmsg);
                if (status!=SQLITE_OK)
                    die("cannot set pagesize: %s", errmsg);
            }
        }
        sqlite_helper_term(&sql_init_helper, (sqlite3_stmt **)&sql_init);
    } else {
        status = sqlite3_open_v2(
            path,
            &connection,
            SQLITE_OPEN_READWRITE,
            NULL);
        if (status != SQLITE_OK)
            die("cannot open database: %s", sqlite3_errstr(status));
        sqlite3_extended_result_codes(connection, 1);
    }
    status = sqlite_helper_init(
        &sql_main_helper,
        (sqlite3_stmt **)&sql_main,
        connection,
        SQLITE_PREPARE_PERSISTENT,
        &errmsg);
    if (status!=SQLITE_OK)
        die("cannot set up database session: %s", errmsg);
    if (init) {
        sqlite3_bind_text(sql_main.setmisc, 1, "version", -1, SQLITE_STATIC);
        sqlite3_bind_int64(sql_main.setmisc, 2, OVSQLITE_SCHEMA_VERSION);
        status = sqlite3_step(sql_main.setmisc);
        if (status!=SQLITE_DONE)
            die("cannot initialize new database: %s",
                sqlite3_errmsg(connection));
        resetclear(sql_main.setmisc);

#ifndef HAVE_ZLIB
        if (use_compression) {
            warn("compression requested but INN was not built with zlib");
            use_compression = false;
        }
#endif /* ! HAVE_ZLIB */
        sqlite3_bind_text(sql_main.setmisc, 1, "compress", -1, SQLITE_STATIC);
        sqlite3_bind_int(sql_main.setmisc, 2, use_compression);
        status = sqlite3_step(sql_main.setmisc);
        if (status!=SQLITE_DONE)
            die("cannot initialize new database: %s",
                sqlite3_errmsg(connection));
        resetclear(sql_main.setmisc);
    } else {
        int version;

        sqlite3_bind_text(sql_main.getmisc, 1, "version", -1, SQLITE_STATIC);
        status = sqlite3_step(sql_main.getmisc);
        if (status!=SQLITE_ROW)
            die("cannot set up database session: %s",
                sqlite3_errmsg(connection));
        version = sqlite3_column_int(sql_main.getmisc, 0);
        if (version!=OVSQLITE_SCHEMA_VERSION)
            die("incompatible database schema %d", version);
        resetclear(sql_main.getmisc);

        sqlite3_bind_text(sql_main.getmisc, 1, "compress", -1, SQLITE_STATIC);
        status = sqlite3_step(sql_main.getmisc);
        if (status!=SQLITE_ROW)
            die("cannot set up database session: %s",
                sqlite3_errmsg(connection));
        use_compression = sqlite3_column_int(sql_main.getmisc, 0);
#ifndef HAVE_ZLIB
        if (use_compression)
            die("database uses compression but INN was not built with zlib");
#endif /* ! HAVE_ZLIB */
        resetclear(sql_main.getmisc);
    }
#ifdef HAVE_ZLIB
    if (use_compression)
        setup_compression(init);
#endif
    if (cachesize) {
        snprintf(
            sqltext, sizeof sqltext,
            "pragma cache_size = -%lu;",
            cachesize);
        status = sqlite3_exec(connection, sqltext, 0, NULL, &errmsg);
        if (status!=SQLITE_OK) {
            warn("cannot set cache size: %s", errmsg);
            sqlite3_free(errmsg);
        }
    }
}

static void close_db(void)
{
    sqlite3_step(sql_main.delete_journal);
    sqlite3_reset(sql_main.delete_journal);
    sqlite_helper_term(&sql_main_helper, (sqlite3_stmt **)&sql_main);
    sqlite3_close_v2(connection);
    connection = NULL;
#ifdef HAVE_ZLIB
    if (use_compression) {
        inflateEnd(&inflation);
        deflateEnd(&deflation);
        buffer_free(flate);
        flate = NULL;
    }
#endif
}

static void close_sockets(void)
{
    size_t ix;

    close(listensock);
    listensock = -1;
    for (ix = client_count; ix-->0; )
        del_client(clients+ix);
}

static unsigned int start_request(
    client_t *client)
{
    uint8_t code;

    unpack_later(client->request, 4);
    unpack_now(client->request, &code, sizeof code);
    return code;
}

static bool finish_request(
    client_t *client)
{
    return client->request->left==0;
}

static void start_response(
    client_t *client,
    unsigned int code)
{
    uint8_t code_r;

    buffer_set(client->response, NULL, 0);
    code_r = code;
    pack_later(client->response, 4);
    pack_now(client->response, &code_r, sizeof code_r);
}

static void finish_response(
    client_t *client)
{
    *(uint32_t *)(void *)client->response->data = client->response->left;
    FD_SET(client->sock, &write_fds);
}

static void simple_response(
    client_t *client,
    unsigned int code)
{
    start_response(client, code);
    finish_response(client);
    if (code>=response_fatal) {
        client->flags |= client_flag_term;
        FD_CLR(client->sock, &read_fds);
    }
}

static void begin_transaction(void)
{
    struct timeval now;

    if (in_transaction)
        return;
    gettimeofday(&now, NULL);
    next_commit = timeval_sum(now, transaction_time_limit);
    sqlite3_step(sql_main.begin);
    sqlite3_reset(sql_main.begin);
    in_transaction = true;
}

static void commit_transaction(void)
{
    if (!in_transaction)
        return;
    sqlite3_step(sql_main.commit);
    sqlite3_reset(sql_main.commit);
    transaction_rowcount = 0;
    in_transaction = false;
}

static void savepoint(void)
{
    sqlite3_step(sql_main.savepoint);
    sqlite3_reset(sql_main.savepoint);
}

static void release_savepoint(void)
{
    sqlite3_step(sql_main.release_savepoint);
    sqlite3_reset(sql_main.release_savepoint);
}

static void rollback_savepoint(void)
{
    sqlite3_step(sql_main.rollback_savepoint);
    sqlite3_reset(sql_main.rollback_savepoint);
}

static void sql_error_response(
    client_t *client,
    int status)
{
    buffer_t *respbuf;
    int32_t status_r;
    char const *errmsg;
    uint32_t errmsg_len;

    respbuf = client->response;
    status_r = status;
    errmsg = sqlite3_errmsg(connection);
    if (errmsg) {
        errmsg_len = strlen(errmsg);
    } else {
        errmsg_len = 0;
    }
    start_response(client, response_sql_error);
    pack_now(respbuf, &status_r, sizeof status_r);
    pack_now(respbuf, &errmsg_len, sizeof errmsg_len);
    pack_now(respbuf, errmsg, errmsg_len);
    finish_response(client);
}

#define failvar \
    unsigned int code = response_ok

#define failvar_stmt \
    failvar; \
    int status = SQLITE_OK; \
    sqlite3_stmt *stmt = NULL

#define failvar_stmt_savepoint \
    failvar_stmt; \
    bool have_savepoint = false

#define fail(err) do { code = (err); goto failure; } while (false)

#define fail_stmt() goto failure_stmt

#define failure_response \
    failure: \
        simple_response(client, code); \
        break

#define failure_stmt_response \
    failure_stmt: \
        sql_error_response(client, status); \
        break

#define failhandling \
    do { \
        break; \
        failure_response; \
    } while (0)

#define failhandling_stmt \
    do { \
        break; \
        do { \
            failure_stmt_response; \
            failure_response; \
        } while (0); \
        if (stmt) \
            resetclear(stmt); \
    } while (0)

#define failhandling_stmt_savepoint \
    do { \
        break; \
        do { \
            failure_stmt_response; \
            failure_response; \
        } while (0); \
        if (stmt) \
            resetclear(stmt); \
        if (have_savepoint) { \
            rollback_savepoint(); \
            release_savepoint(); \
        } \
    } while (0)

static void do_hello(
    client_t *client)
{
    buffer_t *reqbuf;
    uint32_t version;
    uint32_t mode;
#ifndef HAVE_UNIX_DOMAIN_SOCKETS
    void *cookie;
#endif
    failvar;

    reqbuf = client->request;
    if (!unpack_now(reqbuf, &version, sizeof version))
        fail(response_bad_request);
    if (!unpack_now(reqbuf, &mode, sizeof mode))
        fail(response_bad_request);
#ifndef HAVE_UNIX_DOMAIN_SOCKETS
    cookie = unpack_later(reqbuf, OVSQLITE_COOKIE_LENGTH);
    if (!cookie)
        fail(response_bad_request);
#endif
    if (!finish_request(client))
        fail(response_bad_request);

    if (version!=OVSQLITE_PROTOCOL_VERSION)
        fail(response_wrong_version);
#ifndef HAVE_UNIX_DOMAIN_SOCKETS
    if (memcmp(cookie, port.cookie, OVSQLITE_COOKIE_LENGTH)!=0)
        fail(response_failed_auth);
#endif

    client->flags &= ~client_flag_init;
    client->mode = mode;
    simple_response(client, response_ok);
    return;

    failhandling;
}

static void do_set_cutofflow(
    client_t *client)
{
    buffer_t *reqbuf;
    uint8_t cutofflow;
    failvar;

    reqbuf = client->request;
    if (!unpack_now(reqbuf, &cutofflow, sizeof cutofflow))
        fail(response_bad_request);
    if (!finish_request(client))
        fail(response_bad_request);

    client->cutofflow = cutofflow;
    simple_response(client, response_ok);
    return;

    failhandling;
}

static void do_add_group(
    client_t *client)
{
    buffer_t *reqbuf;
    void *groupname;
    uint16_t groupname_len;
    void *flag_alias;
    uint16_t flag_alias_len;
    uint64_t low, high;
    int64_t groupid = 0;
    int changes;
    failvar_stmt;

    reqbuf = client->request;
    if (!unpack_now(reqbuf, &groupname_len, sizeof groupname_len))
        fail(response_bad_request);
    groupname = unpack_later(reqbuf, groupname_len);
    if (!groupname)
        fail(response_bad_request);
    if (!unpack_now(reqbuf, &low, sizeof low))
        fail(response_bad_request);
    if (!unpack_now(reqbuf, &high, sizeof high))
        fail(response_bad_request);
    if (!unpack_now(reqbuf, &flag_alias_len, sizeof flag_alias_len))
        fail(response_bad_request);
    flag_alias = unpack_later(reqbuf, flag_alias_len);
    if (!flag_alias)
        fail(response_bad_request);
    if (!finish_request(client))
        fail(response_bad_request);

    begin_transaction();

    stmt = sql_main.lookup_groupinfo;
    sqlite3_bind_blob(stmt, 1, groupname, groupname_len, SQLITE_STATIC);
    status = sqlite3_step(stmt);
    switch (status) {
    case SQLITE_ROW:
        groupid = sqlite3_column_int64(stmt, 0);
        break;
    case SQLITE_DONE:
        break;
    default:
        fail_stmt();
    }
    resetclear(stmt);
    stmt = NULL;

    if (!groupid) {
        stmt = sql_main.add_group;
        sqlite3_bind_blob(
            stmt, 1, groupname, groupname_len, SQLITE_STATIC);
        sqlite3_bind_blob(
            stmt, 2, flag_alias, flag_alias_len, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, low);
        sqlite3_bind_int64(stmt, 4, high);
        status = sqlite3_step(stmt);
        if (status!=SQLITE_DONE)
            fail_stmt();
        changes = sqlite3_changes(connection);
        resetclear(stmt);
        stmt = NULL;
    } else {
        stmt = sql_main.update_groupinfo_flag_alias;
        sqlite3_bind_int64(stmt, 1, groupid);
        sqlite3_bind_blob(
            stmt, 2, flag_alias, flag_alias_len, SQLITE_STATIC);
        status = sqlite3_step(stmt);
        if (status!=SQLITE_DONE)
            fail_stmt();
        changes = sqlite3_changes(connection);
        resetclear(stmt);
        stmt = NULL;
    }

    if (changes>0)
        commit_transaction();
    simple_response(client, response_ok);
    return;

    failhandling_stmt;
}

static void do_get_groupinfo(
    client_t *client)
{
    buffer_t *reqbuf, *respbuf;
    void *groupname;
    uint16_t groupname_len;
    char const *flag_alias;
    uint16_t flag_alias_len;
    uint64_t low;
    uint64_t high;
    uint64_t count;
    size_t size;
    failvar_stmt;

    reqbuf = client->request;
    if (!unpack_now(reqbuf, &groupname_len, sizeof groupname_len))
        fail(response_bad_request);
    groupname = unpack_later(reqbuf, groupname_len);
    if (!groupname)
        fail(response_bad_request);
    if (!finish_request(client))
        fail(response_bad_request);

    stmt = sql_main.get_groupinfo;
    sqlite3_bind_blob(stmt, 1, groupname, groupname_len, SQLITE_STATIC);
    status = sqlite3_step(stmt);
    switch (status) {
    case SQLITE_ROW:
        break;
    case SQLITE_DONE:
        fail(response_no_group);
    default:
        fail_stmt();
    }
    low = sqlite3_column_int64(stmt, 0);
    high = sqlite3_column_int64(stmt, 1);
    count = sqlite3_column_int64(stmt, 2);
    flag_alias = sqlite3_column_blob(stmt, 3);
    if (!flag_alias)
        fail(response_corrupted);
    size = sqlite3_column_bytes(stmt, 3);
    if (size>=0x10000)
        fail(response_corrupted);
    flag_alias_len = size;
    respbuf = client->response;
    start_response(client, response_groupinfo);
    pack_now(respbuf, &low, sizeof low);
    pack_now(respbuf, &high, sizeof high);
    pack_now(respbuf, &count, sizeof count);
    pack_now(respbuf, &flag_alias_len, sizeof flag_alias_len);
    pack_now(respbuf, flag_alias, flag_alias_len);
    finish_response(client);
    resetclear(stmt);
    stmt = NULL;
    return;

    failhandling_stmt;
}

static void do_delete_group(
    client_t *client)
{
    buffer_t *reqbuf;
    void *groupname;
    uint16_t groupname_len;
    failvar_stmt;

    reqbuf = client->request;
    if (!unpack_now(reqbuf, &groupname_len, sizeof groupname_len))
        fail(response_bad_request);
    groupname = unpack_later(reqbuf, groupname_len);
    if (!groupname)
        fail(response_bad_request);
    if (!finish_request(client))
        fail(response_bad_request);

    stmt = sql_main.set_group_deleted;
    sqlite3_bind_blob(stmt, 1, groupname, groupname_len, SQLITE_STATIC);
    status = sqlite3_step(stmt);
    if (status!=SQLITE_DONE)
        fail_stmt();
    if (sqlite3_changes(connection)<=0)
        fail(response_no_group);
    resetclear(stmt);
    stmt = NULL;
    simple_response(client, response_ok);
    commit_transaction();
    return;

    failhandling_stmt;
}

static void do_list_groups(
    client_t *client)
{
    buffer_t *reqbuf, *respbuf;
    uint32_t space;
    int64_t groupid;
    size_t off_code, off_id, off_count;
    uint32_t groupcount;
    failvar_stmt;

    reqbuf = client->request;
    if (!unpack_now(reqbuf, &space, sizeof space))
        fail(response_bad_request);
    if (!unpack_now(reqbuf, &groupid, sizeof groupid))
        fail(response_bad_request);
    if (!finish_request(client))
        fail(response_bad_request);
    if (space>=0x100000)
        fail(response_bad_request);

    respbuf = client->response;
    start_response(client, response_grouplist);
    off_code = respbuf->left-1;
    off_id = pack_later(respbuf, sizeof groupid);
    off_count = pack_later(respbuf, sizeof groupcount);
    groupcount = 0;

    stmt = sql_main.list_groups;
    sqlite3_bind_int64(stmt, 1, groupid);
    for (;;) {
        size_t saveleft = respbuf->left;
        size_t size;
        char const *groupname;
        uint16_t groupname_len;
        uint64_t low, high, count;
        char const *flag_alias;
        uint16_t flag_alias_len;

        status = sqlite3_step(stmt);
        switch (status) {
        case SQLITE_ROW:
            break;
        case SQLITE_DONE:
            respbuf->data[off_code] = response_grouplist_done;
            goto done;
        default:
            if (groupcount>0) {
                goto done;
            } else {
                fail_stmt();
            }
        }

        groupname = sqlite3_column_blob(stmt, 1);
        size = sqlite3_column_bytes(stmt, 1);
        if (!groupname || size>=0x10000)
            goto corrupted;
        groupname_len = size;
        if (pack_now(respbuf, &groupname_len, sizeof groupname_len)>space)
            goto flush;
        if (pack_now(respbuf, groupname, groupname_len)>space)
            goto flush;

        low = sqlite3_column_int64(stmt, 2);
        if (pack_now(respbuf, &low, sizeof low)>space)
            goto flush;

        high = sqlite3_column_int64(stmt, 3);
        if (pack_now(respbuf, &high, sizeof high)>space)
            goto flush;

        count = sqlite3_column_int64(stmt, 4);
        if (pack_now(respbuf, &count, sizeof count)>space)
            goto flush;

        flag_alias = sqlite3_column_blob(stmt, 5);
        size = sqlite3_column_bytes(stmt, 5);
        if (!flag_alias || size>=0x10000)
            goto corrupted;
        flag_alias_len = size;

        if (pack_now(respbuf, &flag_alias_len, sizeof flag_alias_len)>space)
            goto flush;
        if (pack_now(respbuf, flag_alias, flag_alias_len)>space)
            goto flush;

        groupid = sqlite3_column_int64(stmt, 0);
        groupcount++;
        continue;

    corrupted:
        if (groupcount<=0)
            fail(response_corrupted);
    flush:
        respbuf->left = saveleft;
        break;
    }

done:
    resetclear(stmt);
    stmt = NULL;
    memcpy(respbuf->data+off_id, &groupid, sizeof groupid);
    memcpy(respbuf->data+off_count, &groupcount, sizeof groupcount);
    finish_response(client);
    return;

    failhandling_stmt;
}

static void do_add_article(
    client_t *client)
{
    buffer_t *reqbuf;
    void *groupname;
    uint16_t groupname_len;
    uint64_t artnum;
    int64_t arrived;
    int64_t expires;
    TOKEN token;
    uint8_t *overview;
    uint32_t overview_len;
    int64_t groupid;
    uint64_t low;
    failvar_stmt_savepoint;

    reqbuf = client->request;
    if (!unpack_now(reqbuf, &groupname_len, sizeof groupname_len))
        fail(response_bad_request);
    groupname = unpack_later(reqbuf, groupname_len);
    if (!groupname)
        fail(response_bad_request);
    if (!unpack_now(reqbuf, &artnum, sizeof artnum))
        fail(response_bad_request);
    if (!unpack_now(reqbuf, &arrived, sizeof arrived))
        fail(response_bad_request);
    if (!unpack_now(reqbuf, &expires, sizeof expires))
        fail(response_bad_request);
    if (!unpack_now(reqbuf, &token, sizeof token))
        fail(response_bad_request);
    if (!unpack_now(reqbuf, &overview_len, sizeof overview_len))
        fail(response_bad_request);
    overview = unpack_later(reqbuf, overview_len);
    if (!overview)
        fail(response_bad_request);
    if (!finish_request(client))
        fail(response_bad_request);

    begin_transaction();

    stmt = sql_main.lookup_groupinfo;
    sqlite3_bind_blob(stmt, 1, groupname, groupname_len, SQLITE_STATIC);
    status = sqlite3_step(stmt);
    switch (status) {
    case SQLITE_ROW:
        break;
    case SQLITE_DONE:
        fail(response_no_group);
    default:
        fail_stmt();
    }
    groupid = sqlite3_column_int64(stmt, 0);
    low = sqlite3_column_int64(stmt, 1);
    if (client->cutofflow && artnum<low)
        fail(response_old_article);
    resetclear(stmt);
    stmt = NULL;

#ifdef HAVE_ZLIB
    /*
     * How to be excessively clever and make the corner cases
     * work for you instead of against you, part 1.
     * a) deflation.avail_out is set to the uncompressed size.
     * b) The uncompressed size is stored first,
     *    consuming some of the deflation buffer.
     * c) When deflate returns Z_STREAM_END, we know that everything
     *    went well _and_ that compression saved at least one byte,
     *    overhead included.
     */
    if (use_compression && overview_len>5) {
        buffer_resize(flate, overview_len);
        deflation.next_out = (uint8_t *)flate->data;
        deflation.avail_out = overview_len;
        pack_length(&deflation, overview_len);
        deflation.next_in = overview;
        deflation.avail_in = overview_len;
#ifdef USE_DICTIONARY
        status = deflateSetDictionary(
            &deflation,
            (uint8_t *)dictionary,
            make_dict(groupname, groupname_len, artnum));
        if (status==Z_OK)
#endif
            status = deflate(&deflation, Z_FINISH);
        flate->left = (char *)deflation.next_out-flate->data;
        if (status==Z_STREAM_END) {
            overview = (uint8_t *)flate->data;
            overview_len = flate->left;
        } else {
            /* This is safe; it overwrites the last byte of the overview
               length, which we have already unpacked. */
            *--overview = 0;
            overview_len++;
        }
        deflation.next_in = NULL;
        deflation.avail_in = 0;
        deflateReset(&deflation);
    }
#endif

    savepoint();
    have_savepoint = true;

    stmt = sql_main.add_article;
    sqlite3_bind_int64(stmt, 1, groupid);
    sqlite3_bind_int64(stmt, 2, artnum);
    sqlite3_bind_int64(stmt, 3, arrived);
    sqlite3_bind_int64(stmt, 4, expires);
    sqlite3_bind_blob(stmt, 5, &token, sizeof token, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 6, overview, overview_len, SQLITE_STATIC);
    status = sqlite3_step(stmt);
    switch (status) {
    case SQLITE_DONE:
        break;
    case SQLITE_CONSTRAINT_PRIMARYKEY:
        fail(response_dup_article);
    default:
        fail_stmt();
    }
    resetclear(stmt);
    stmt = NULL;

    stmt = sql_main.update_groupinfo_add;
    sqlite3_bind_int64(stmt, 1, groupid);
    sqlite3_bind_int64(stmt, 2, artnum);
    status = sqlite3_step(stmt);
    if (status!=SQLITE_DONE)
        fail_stmt();
    resetclear(stmt);
    stmt = NULL;

    release_savepoint();
    have_savepoint = false;
    transaction_rowcount++;
    simple_response(client, response_ok);
    return;

    failhandling_stmt_savepoint;
}

static void do_get_artinfo(
    client_t *client)
{
    buffer_t *reqbuf, *respbuf;
    void *groupname;
    uint16_t groupname_len;
    uint64_t artnum;
    TOKEN const *token;
    failvar_stmt;

    reqbuf = client->request;
    if (!unpack_now(reqbuf, &groupname_len, sizeof groupname_len))
        fail(response_bad_request);
    groupname = unpack_later(reqbuf, groupname_len);
    if (!groupname)
        fail(response_bad_request);
    if (!unpack_now(reqbuf, &artnum, sizeof artnum))
        fail(response_bad_request);
    if (!finish_request(client))
        fail(response_bad_request);

    stmt = sql_main.get_artinfo;
    sqlite3_bind_blob(stmt, 1, groupname, groupname_len, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, artnum);
    status = sqlite3_step(stmt);
    switch (status) {
    case SQLITE_ROW:
        break;
    case SQLITE_DONE:
        fail(response_no_article);
    default:
        fail_stmt();
    }
    token = sqlite3_column_blob(stmt, 0);
    if (!token)
        fail(response_corrupted);
    if (sqlite3_column_bytes(stmt, 0) != sizeof (TOKEN))
        fail(response_corrupted);
    respbuf = client->response;
    start_response(client, response_artinfo);
    pack_now(respbuf, token, sizeof (TOKEN));
    finish_response(client);
    resetclear(stmt);
    stmt = NULL;
    return;

    failhandling_stmt;
}

static void do_delete_article(
    client_t *client)
{
    buffer_t *reqbuf;
    void *groupname;
    uint16_t groupname_len;
    uint64_t artnum;
    int64_t groupid;
    uint64_t low;
    failvar_stmt_savepoint;

    reqbuf = client->request;
    if (!unpack_now(reqbuf, &groupname_len, sizeof groupname_len))
        fail(response_bad_request);
    groupname = unpack_later(reqbuf, groupname_len);
    if (!groupname)
        fail(response_bad_request);
    if (!unpack_now(reqbuf, &artnum, sizeof artnum))
        fail(response_bad_request);
    if (!finish_request(client))
        fail(response_bad_request);

    begin_transaction();

    stmt = sql_main.lookup_groupinfo;
    sqlite3_bind_blob(stmt, 1, groupname, groupname_len, SQLITE_STATIC);
    status = sqlite3_step(stmt);
    switch (status) {
    case SQLITE_ROW:
        break;
    case SQLITE_DONE:
        fail(response_no_group);
    default:
        fail_stmt();
    }
    groupid = sqlite3_column_int64(stmt, 0);
    low = sqlite3_column_int64(stmt, 1);
    resetclear(stmt);
    stmt = NULL;

    savepoint();
    have_savepoint = true;

    stmt = sql_main.delete_article;
    sqlite3_bind_int64(stmt, 1, groupid);
    sqlite3_bind_int64(stmt, 2, artnum);
    status = sqlite3_step(stmt);
    if (status!=SQLITE_DONE)
        fail_stmt();
    if (sqlite3_changes(connection)<=0)
        fail(response_no_article);
    resetclear(stmt);
    stmt = NULL;

    if (artnum==low) {
        stmt = sql_main.update_groupinfo_delete_low;
    } else {
        stmt = sql_main.update_groupinfo_delete_middle;
    }
    sqlite3_bind_int64(stmt, 1, groupid);
    sqlite3_bind_int(stmt, 2, 1);
    status = sqlite3_step(stmt);
    if (status!=SQLITE_DONE)
        fail_stmt();
    resetclear(stmt);
    stmt = NULL;

    release_savepoint();
    have_savepoint = false;
    transaction_rowcount++;
    simple_response(client, response_ok);
    return;

    failhandling_stmt_savepoint;
}

static void do_search_group(
    client_t *client)
{
    buffer_t *reqbuf, *respbuf;
    uint32_t space;
    uint8_t flags, cols;
    void *groupname;
    uint16_t groupname_len;
    uint64_t low;
    uint64_t high;
    size_t off_count, off_code;
    uint32_t count;
    failvar_stmt;

    reqbuf = client->request;
    if (!unpack_now(reqbuf, &space, sizeof space))
        fail(response_bad_request);
    if (!unpack_now(reqbuf, &flags, sizeof flags))
        fail(response_bad_request);
    if (!unpack_now(reqbuf, &cols, sizeof cols))
        fail(response_bad_request);
    if (!unpack_now(reqbuf, &groupname_len, sizeof groupname_len))
        fail(response_bad_request);
    groupname = unpack_later(reqbuf, groupname_len);
    if (!groupname)
        fail(response_bad_request);
    if (!unpack_now(reqbuf, &low, sizeof low))
        fail(response_bad_request);
    if (flags & search_flag_high) {
        if (!unpack_now(reqbuf, &high, sizeof high))
            fail(response_bad_request);
    }
    if (!finish_request(client))
        fail(response_bad_request);
    if (space>=0x100000
            || flags & ~search_flags_all
            || cols & ~search_cols_all)
        fail(response_bad_request);

    respbuf = client->response;
    start_response(client, response_artlist);
    off_code = respbuf->left-1;
    pack_now(respbuf, &cols, sizeof cols);
    off_count = pack_later(respbuf, sizeof count);
    count = 0;

    if (flags & search_flag_high) {
        if (cols & search_col_overview) {
            stmt = sql_main.list_articles_high_overview;
        } else {
            stmt = sql_main.list_articles_high;
        }
    } else {
        if (cols & search_col_overview) {
            stmt = sql_main.list_articles_overview;
        } else {
            stmt = sql_main.list_articles;
        }
    }
    sqlite3_bind_blob(stmt, 1, groupname, groupname_len, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, low);
    if (flags&search_flag_high)
        sqlite3_bind_int64(stmt, 3, high);
    for (;;) {
        size_t saveleft = respbuf->left;
        uint64_t artnum;
        size_t size;

        status = sqlite3_step(stmt);
        switch (status) {
        case SQLITE_ROW:
            break;
        case SQLITE_DONE:
            respbuf->data[off_code] = response_artlist_done;
            goto done;
        default:
            if (count>0) {
                goto done;
            } else {
                fail_stmt();
            }
        }

        artnum = sqlite3_column_int64(stmt, 0);
        if (pack_now(respbuf, &artnum, sizeof artnum)>space)
            goto flush;

        if (cols & search_col_arrived) {
            int64_t arrived;

            arrived = sqlite3_column_int64(stmt, 1);
            if (pack_now(respbuf, &arrived, sizeof arrived)>space)
                goto flush;
        }

        if (cols & search_col_expires) {
            int64_t expires;

            expires = sqlite3_column_int64(stmt, 2);
            if (pack_now(respbuf, &expires, sizeof expires)>space)
                goto flush;
        }

        if (cols & search_col_token) {
            TOKEN const *token;

            token = sqlite3_column_blob(stmt, 3);
            size = sqlite3_column_bytes(stmt, 3);
            if (!token || size!=sizeof (TOKEN))
                goto corrupted;
            if (pack_now(respbuf, token, sizeof (TOKEN))>space)
                goto flush;
        }

        if (cols & search_col_overview) {
            uint8_t const *overview;
            uint32_t overview_len;

            overview = sqlite3_column_blob(stmt, 4);
            size = sqlite3_column_bytes(stmt, 4);
            if (!overview || size>100000)
                goto corrupted;
            overview_len = size;
#ifdef HAVE_ZLIB
            /*
             * How to be excessively clever and make the corner cases
             * work for you instead of against you, part 2.
             * a) deflation.avail_out is set to the expected uncompressed size.
             * b) When inflate returns Z_STREAM_END, we know that everything
             *    went well _and_ that the uncompressed data isn't larger
             *    than expected.
             * c) We still need to check that the uncompressed data isn't
             *    smaller than expected.
             */
            if (use_compression) {
                uint32_t raw_len;

                inflation.next_in = (uint8_t *)overview;
                inflation.avail_in = overview_len;

                raw_len = unpack_length(&inflation);
                if (raw_len>100000)
                    goto corrupted;
                if (raw_len>0) {
                    buffer_resize(flate, raw_len);
                    inflation.next_out = (uint8_t *)flate->data;
                    inflation.avail_out = raw_len;
                    status = inflate(&inflation, Z_FINISH);
#ifdef USE_DICTIONARY
                    if (status==Z_NEED_DICT) {
                        status = inflateSetDictionary(
                            &inflation,
                            (uint8_t *)dictionary,
                            make_dict(groupname, groupname_len, artnum));
                        if (status==Z_OK)
                            status = inflate(&inflation, Z_FINISH);
                    }
#endif
                    flate->left = (char *)inflation.next_out-flate->data;
                    inflation.next_in = NULL;
                    inflation.avail_in = 0;
                    inflateReset(&inflation);
                    if (status!=Z_STREAM_END || inflation.avail_out>0)
                        goto corrupted;
                    overview = (uint8_t *)flate->data;
                    overview_len = flate->left;
                } else {
                    overview++;
                    overview_len--;
                }
            }
#endif
            if (pack_now(respbuf, &overview_len, sizeof overview_len)>space)
                goto flush;
            if (pack_now(respbuf, overview, overview_len)>space)
                goto flush;
        }

        count++;
        continue;

    corrupted:
        if (count<=0)
            fail(response_corrupted);
    flush:
        respbuf->left = saveleft;
        break;
    }

done:
    resetclear(stmt);
    stmt = NULL;
    memcpy(respbuf->data+off_count, &count, sizeof count);
    finish_response(client);
    return;

    failhandling_stmt;
}

static void do_start_expire_group(
    client_t *client)
{
    buffer_t *reqbuf;
    void *groupname;
    uint16_t groupname_len;
    failvar_stmt;

    reqbuf = client->request;
    if (!unpack_now(reqbuf, &groupname_len, sizeof groupname_len))
        fail(response_bad_request);
    groupname = unpack_later(reqbuf, groupname_len);
    if (!groupname)
        fail(response_bad_request);
    if (!finish_request(client))
        fail(response_bad_request);

    if (!client->expiration_start) {
        time(&client->expiration_start);
        client->check_forgotten = true;
    }
    stmt = sql_main.start_expire_group;
    sqlite3_bind_blob(stmt, 1, groupname, groupname_len, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, client->expiration_start);
    status = sqlite3_step(stmt);
    if (status!=SQLITE_DONE)
        fail_stmt();
    if (sqlite3_changes(connection)<=0)
        fail(response_no_group);
    resetclear(stmt);
    stmt = NULL;
    simple_response(client, response_ok);
    commit_transaction();
    return;

    failhandling_stmt;
}

static void do_expire_group(
    client_t *client)
{
    buffer_t *reqbuf;
    bool hit_low = false;
    void *groupname;
    uint16_t groupname_len;
    uint32_t count, artix;
    int64_t groupid;
    uint64_t low;
    int changes;
    failvar_stmt_savepoint;

    reqbuf = client->request;
    if (!unpack_now(reqbuf, &groupname_len, sizeof groupname_len))
        fail(response_bad_request);
    groupname = unpack_later(reqbuf, groupname_len);
    if (!groupname)
        fail(response_bad_request);
    if (!unpack_now(reqbuf, &count, sizeof count))
        fail(response_bad_request);

    if (count<=0) {
        if (!finish_request(client))
            fail(response_bad_request);
        simple_response(client, response_ok);
        return;
    }

    begin_transaction();

    stmt = sql_main.lookup_groupinfo;
    sqlite3_bind_blob(stmt, 1, groupname, groupname_len, SQLITE_STATIC);
    status = sqlite3_step(stmt);
    switch (status) {
    case SQLITE_ROW:
        break;
    case SQLITE_DONE:
        fail(response_no_group);
    default:
        fail_stmt();
    }
    groupid = sqlite3_column_int64(stmt, 0);
    low = sqlite3_column_int64(stmt, 1);
    resetclear(stmt);
    stmt = NULL;

    savepoint();
    have_savepoint = true;
    for (artix=0; artix<count; artix++) {
        uint64_t artnum;

        if (!unpack_now(reqbuf, &artnum, sizeof artnum))
            fail(response_bad_request);
        stmt = sql_main.add_expireart;
        sqlite3_bind_int64(stmt, 1, artnum);
        status = sqlite3_step(stmt);
        if (status!=SQLITE_DONE)
            fail_stmt();
        resetclear(stmt);
        stmt = NULL;
        if (artnum==low)
            hit_low = true;
    }
    if (!finish_request(client))
        fail(response_bad_request);

    stmt = sql_main.expire_articles;
    sqlite3_bind_int64(stmt, 1, groupid);
    status = sqlite3_step(stmt);
    if (status!=SQLITE_DONE)
        fail_stmt();
    changes = sqlite3_changes(connection);
    resetclear(stmt);
    stmt = NULL;

    sqlite3_step(sql_main.clear_expireart);
    sqlite3_reset(sql_main.clear_expireart);

    if (changes>0) {
        if (hit_low) {
            stmt = sql_main.update_groupinfo_delete_low;
        } else {
            stmt = sql_main.update_groupinfo_delete_middle;
        }
        sqlite3_bind_int64(stmt, 1, groupid);
        sqlite3_bind_int(stmt, 2, changes);
        status = sqlite3_step(stmt);
        if (status!=SQLITE_DONE)
            fail_stmt();
        resetclear(stmt);
        stmt = NULL;
        transaction_rowcount += changes;
    }

    release_savepoint();
    have_savepoint = false;
    simple_response(client, response_ok);
    return;

    failhandling_stmt_savepoint;
}

static void do_finish_expire(
    client_t *client)
{
    int changes;
    int64_t groupid = 0;
    failvar_stmt_savepoint;

    if (!finish_request(client))
        fail(response_bad_request);

    if (!client->expiration_start)
        fail(response_sequence_error);

    if (client->check_forgotten) {
        stmt = sql_main.set_forgotten_deleted;
        sqlite3_bind_int64(stmt, 1, client->expiration_start);
        status = sqlite3_step(stmt);
        if (status!=SQLITE_DONE)
            fail_stmt();
        changes = sqlite3_changes(connection);
        resetclear(stmt);
        stmt = NULL;

        if (changes>0)
            commit_transaction();
        client->check_forgotten = false;
    }

    stmt = sql_main.delete_empty_groups;
    status = sqlite3_step(stmt);
    if (status!=SQLITE_DONE)
        fail_stmt();
    changes = sqlite3_changes(connection);
    sqlite3_reset(stmt);
    stmt = NULL;
    if (changes>0)
        commit_transaction();

    begin_transaction();

    stmt = sql_main.get_deleted_groupid;
    status = sqlite3_step(stmt);
    switch (status) {
    case SQLITE_ROW:
        groupid = sqlite3_column_int64(stmt, 0);
        break;
    case SQLITE_DONE:
        break;
    default:
        fail_stmt();
    }
    sqlite3_reset(stmt);
    stmt = NULL;

    if (!groupid) {
        simple_response(client, response_done);
        client->expiration_start = 0;
        return;
    }

    savepoint();
    have_savepoint = true;

    stmt = sql_main.fill_expireart;
    sqlite3_bind_int64(stmt, 1, groupid);
    sqlite3_bind_int(stmt, 2, transaction_row_limit/2+1);
    status = sqlite3_step(stmt);
    if (status!=SQLITE_DONE)
        fail_stmt();
    resetclear(stmt);
    stmt = NULL;

    stmt = sql_main.expire_articles;
    sqlite3_bind_int64(stmt, 1, groupid);
    status = sqlite3_step(stmt);
    if (status!=SQLITE_DONE)
        fail_stmt();
    changes = sqlite3_changes(connection);
    resetclear(stmt);
    stmt = NULL;

    sqlite3_step(sql_main.clear_expireart);
    sqlite3_reset(sql_main.clear_expireart);

    transaction_rowcount += changes;
    release_savepoint();
    have_savepoint = false;
    simple_response(client, response_ok);
    return;

    failhandling_stmt_savepoint;
}

/*
 * This needs to stay in sync with the request code enum
 * in ovsqlite-private.h or things will explode.
 */

static void (*dispatch[count_request_codes])(
    client_t *) =
{
    do_hello,
    do_set_cutofflow,
    do_add_group,
    do_get_groupinfo,
    do_delete_group,
    do_list_groups,
    do_add_article,
    do_get_artinfo,
    do_delete_article,
    do_search_group,
    do_start_expire_group,
    do_expire_group,
    do_finish_expire
};

#if defined(EWOULDBLOCK)
  #if defined(EAGAIN) && EAGAIN!=EWOULDBLOCK
    #define case_NONBLOCK case EWOULDBLOCK: case EAGAIN:
  #else
    #define case_NONBLOCK case EWOULDBLOCK:
  #endif
#elif defined(EAGAIN)
  #define case_NONBLOCK case EAGAIN:
#else
  #define case_NONBLOCK
#endif

static void handle_write(
    client_t *client)
{
    int sock;
    buffer_t *response;
    char *data;
    size_t used, left;
    ssize_t got;

    sock = client->sock;
    response = client->response;
    used = response->used;
    left = response->left;
    data = response->data+used;
    for (;;) {
        got = write(sock, data, left);
        if (got>=0 || errno!=EINTR)
            break;
    }
    if (got==-1) {
        switch (errno) {
        case_NONBLOCK
            break;
        default:
            del_client(client);
        }
        return;
    }
    response->used = used+got;
    response->left = left -= got;
    if (left>0)
        return;
    if (client->flags & client_flag_term) {
        del_client(client);
    } else {
        buffer_set(client->request, NULL, 0);
        FD_CLR(client->sock, &write_fds);
        FD_SET(client->sock, &read_fds);
    }
}

static void handle_read(
    client_t *client)
{
    unsigned int code;

    for (;;) {
        bool have_size;
        size_t left, want;
        ssize_t got;
        size_t request_size;

        left = client->request->left;
        have_size = left>=4;
        if (have_size) {
            request_size = *(uint32_t *)(void *)client->request->data;
            want = request_size-left;
        } else {
            want = 5-left;
        }
        got = read(client->sock, client->request->data+left, want);
        if (got==-1) {
            switch (errno) {
            case_NONBLOCK
                break;
            case EINTR:
                continue;
            default:
                del_client(client);
            }
            return;
        }
        if (got==0) {
            del_client(client);
            return;
        }
        client->request->left = left += got;
        if ((size_t)got<want)
            return;
        if (have_size)
            break;
        request_size = *(uint32_t *)(void *)client->request->data;
        if (request_size<5) {
            simple_response(client, response_bad_request);
            return;
        }
        if (request_size>=0x100000) {
            simple_response(client, response_oversized);
            return;
        }
        if (left>=request_size)
            break;
        buffer_resize(client->request, request_size);
    }
    FD_CLR(client->sock, &read_fds);
    code = start_request(client);
    if (code>=count_request_codes) {
        simple_response(client, response_bad_request);
        return;
    }
    if ((code>request_hello) != !(client->flags&client_flag_init)) {
        simple_response(client, response_wrong_state);
        return;
    }
    (*dispatch[code])(client);
    return;
}

static void handle_accept(void)
{
#ifdef HAVE_UNIX_DOMAIN_SOCKETS
    struct sockaddr_un sa;
#else /* ! HAVE_UNIX_DOMAIN_SOCKETS */
    struct sockaddr_in sa;
#endif /* ! HAVE_UNIX_DOMAIN_SOCKETS */
    socklen_t salen;
    int sock;

    salen = sizeof sa;
    sock = accept(listensock, (struct sockaddr *)&sa, &salen);
    if (sock==-1)
        return;
    add_client(sock);
}

static void mainloop(void)
{
    while (!terminating) {
        fd_set read_fds_out, write_fds_out;
        int n;
        struct timeval delta, *nap;
        client_t *client;

        if (in_transaction) {
            struct timeval now;

            gettimeofday(&now, NULL);
            delta = timeval_difference(next_commit, now);
            if (delta.tv_sec<0 || delta.tv_usec<0
                    || transaction_rowcount>=transaction_row_limit) {
                commit_transaction();
                continue;
            }
            nap = &delta;
        } else {
            nap = NULL;
        }
        read_fds_out = read_fds;
        write_fds_out = write_fds;
        n = select(maxsock+1, &read_fds_out, &write_fds_out, NULL, nap);
        if (n<=0)
            continue;
        if (FD_ISSET(listensock, &read_fds_out)) {
            n--;
            handle_accept();
        }
        for (client = clients+client_count; n>0 && client>clients; ) {
            client--;
            if (FD_ISSET(client->sock, &read_fds_out)) {
                n--;
                handle_read(client);
            } else if (FD_ISSET(client->sock, &write_fds_out)) {
                n--;
                handle_write(client);
            }
        }
    }
    commit_transaction();
}

static void usage(void)
{
    fputs("Usage: ovsqlite-server [ -d ]\n", stderr);
    exit(1);
}

int main(
    int argc,
    char **argv)
{
    bool debug = false;

    setproctitle_init(argc, argv);
    message_program_name = "ovsqlite-server";
    for (;;) {
        int c;

        c = getopt(argc, argv, "d");
        if (c==-1)
            break;
        switch (c) {
        case 'd':
            debug = true;
            break;
        default:
            usage();
        }
    }
    if (debug) {
        message_handlers_warn(1, message_log_stderr);
        message_handlers_die(1, message_log_stderr);
    } else {
        openlog("ovsqlite-server", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
        message_handlers_warn(1, message_log_syslog_err);
        message_handlers_die(1, message_log_syslog_err);
    }
    if (!innconf_read(NULL))
        exit(1);
    load_config();
    if (!debug)
        daemonize(innconf->pathtmp);
    catch_signals();
    make_pidfile();
    open_db();
    make_listener();
    innconf_free(innconf);
    innconf = NULL;
    if (setfdlimit(FD_SETSIZE)==-1)
        syswarn("cannot set file descriptor limit");
    mainloop();
    close_sockets();
    close_db();
    if (pidfile)
        unlink(pidfile);
}

#else /* ! HAVE_SQLITE3 */

int main(void)
{
    die("SQLite support not compiled");
}

#endif /* ! HAVE_SQLITE3 */
