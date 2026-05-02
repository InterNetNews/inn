/*
**  Overview storage method based on SQLite.
**
**  Original implementation written by Bo Lindbergh (2020-12-17).
**  <2bfjdsla52kztwejndzdstsxl9athp@gmail.com>
**
**  Various bug fixes, code and documentation improvements since then
**  in 2021-2024, 2026.
**
**  Direct reader mode added in 2026 to allow nnrpd processes to read the
**  overview database directly via SQLite, bypassing ovsqlite-server for
**  read-only access.  This improves read performance by eliminating the
**  IPC round-trip and server serialization bottleneck.
*/

#include "ovsqlite.h"
#include "inn/messages.h"
#include "ovsqlite-private.h"

#ifdef HAVE_SQLITE3

#    include <fcntl.h>

#    include "portable/socket.h"
#    ifdef HAVE_UNIX_DOMAIN_SOCKETS
#        include "portable/socket-unix.h"
#    endif

#    include "conffile.h"
#    include "inn/confparse.h"
#    include "inn/fdflag.h"
#    include "inn/innconf.h"
#    include "inn/libinn.h"
#    include "inn/newsuser.h"
#    include "inn/paths.h"

#    include "../ovinterface.h"

#    include <sqlite3.h>
#    include "sql-read.h"
#    include "sqlite-helper.h"

#    ifdef HAVE_ZLIB
#        define USE_DICTIONARY 1
#        include <zlib.h>
#    endif

#    define OVSQLITE_DB_FILE "ovsqlite.db"

typedef struct handle_t {
    uint8_t buffer[SEARCHSPACE];
    uint64_t low;
    uint64_t high;
    uint32_t count;
    uint32_t index;
    char **overview;
    time_t *arrived;
    ARTNUM *artnum;
    TOKEN *token;
    uint16_t groupname_len;
    uint8_t cols;
    bool done;
    char groupname[1];
} handle_t;

/* Server connection state (used for write mode and legacy read mode). */
static int sock = -1;
static buffer_t *request;
static buffer_t *response;

#    ifndef HAVE_UNIX_DOMAIN_SOCKETS
static ovsqlite_port port;
#    endif

/* Direct reader state (used when mode is OV_READ only). */
static bool direct_reader = false;
static sqlite3 *read_connection = NULL;
static sql_read_t sql_read;
static bool reader_use_compression = false;

#    ifdef HAVE_ZLIB

/* clang-format off */
static uint32_t const pack_length_bias[5] =
{
             0,
          0x80,
        0x4080,
      0x204080,
    0x10204080,
};
/* clang-format on */

static z_stream reader_inflation;
static buffer_t *reader_flate;

#        ifdef USE_DICTIONARY

static char reader_dictionary[0x8000];
static unsigned int reader_basedict_len;

#        endif /* USE_DICTIONARY */

static uint32_t
reader_unpack_length(z_stream *stream)
{
    uint8_t *walk;
    unsigned int c, lenlen, n;
    uint32_t length;

    if (stream->avail_in <= 0)
        return ~0U;
    walk = stream->next_in;
    c = *walk++;
    lenlen = 1;
    while (c & (1U << (8 - lenlen)))
        lenlen++;
    if (lenlen > 5 || lenlen > stream->avail_in)
        return ~0U;
    length = c & ~(~0U << (8 - lenlen));
    for (n = lenlen - 1; n > 0; n--)
        length = (length << 8) | *walk++;
    length += pack_length_bias[lenlen - 1];
    stream->next_in = walk;
    stream->avail_in -= lenlen;
    return length;
}

#        ifdef USE_DICTIONARY

static unsigned int
reader_make_dict(char const *groupname, int groupname_len, uint64_t artnum)
{
    sqlite3_snprintf(sizeof reader_dictionary - reader_basedict_len,
                     reader_dictionary + reader_basedict_len,
                     "%.*s:%llu\r\n", groupname_len, groupname, artnum);
    return reader_basedict_len
           + strlen(reader_dictionary + reader_basedict_len);
}

#        endif /* USE_DICTIONARY */

#    endif /* HAVE_ZLIB */


/*
**  Decompress an overview blob read directly from SQLite.
**  Returns the decompressed data in reader_flate, or NULL on error.
**  On success, *out_len is set to the decompressed length.
*/
#    ifdef HAVE_ZLIB
static uint8_t *
reader_decompress(uint8_t const *overview, uint32_t overview_len,
                  char const *groupname, int groupname_len, uint64_t artnum,
                  uint32_t *out_len)
{
    uint32_t raw_len;
    int status;

    reader_inflation.next_in = (uint8_t *) overview;
    reader_inflation.avail_in = overview_len;

    raw_len = reader_unpack_length(&reader_inflation);
    if (raw_len > MAX_OVDATA_SIZE)
        return NULL;
    if (raw_len > 0) {
        buffer_resize(reader_flate, raw_len);
        reader_inflation.next_out = (uint8_t *) reader_flate->data;
        reader_inflation.avail_out = raw_len;
        status = inflate(&reader_inflation, Z_FINISH);
#        ifdef USE_DICTIONARY
        if (status == Z_NEED_DICT) {
            status = inflateSetDictionary(
                &reader_inflation, (uint8_t *) reader_dictionary,
                reader_make_dict(groupname, groupname_len, artnum));
            if (status == Z_OK)
                status = inflate(&reader_inflation, Z_FINISH);
        }
#        endif
        reader_flate->left =
            (char *) reader_inflation.next_out - reader_flate->data;
        reader_inflation.next_in = NULL;
        reader_inflation.avail_in = 0;
        inflateReset(&reader_inflation);
        if (status != Z_STREAM_END || reader_inflation.avail_out > 0)
            return NULL;
        *out_len = reader_flate->left;
        return (uint8_t *) reader_flate->data;
    } else {
        /* Compression didn't save space; data stored uncompressed after
         * the zero length marker. */
        *out_len = reader_inflation.avail_in;
        return reader_inflation.next_in;
    }
}
#    endif /* HAVE_ZLIB */


/*
**  Helper callback for pragma queries that return a single text value.
*/
static int
pragma_callback(void *data, int ncols, char **values, char **names UNUSED)
{
    char **result = data;

    if (ncols > 0 && values[0])
        *result = xstrdup(values[0]);
    return 0;
}

/*
**  Helper to read an integer value from the misc table.
*/
static bool
read_misc_int(char const *key, int *value)
{
    int status;

    sqlite3_bind_text(sql_read.getmisc, 1, key, -1, SQLITE_STATIC);
    status = sqlite3_step(sql_read.getmisc);
    if (status != SQLITE_ROW) {
        warn("ovsqlite: cannot read '%s' from database: %s", key,
             sqlite3_errmsg(read_connection));
        sqlite3_reset(sql_read.getmisc);
        sqlite3_clear_bindings(sql_read.getmisc);
        return false;
    }
    *value = sqlite3_column_int(sql_read.getmisc, 0);
    sqlite3_reset(sql_read.getmisc);
    sqlite3_clear_bindings(sql_read.getmisc);
    return true;
}

/*
**  Open the overview database directly for read-only access.
*/
static bool
direct_open(void)
{
    char *path;
    char *confpath;
    struct config_group *top;
    int status;
    char *errmsg;
    char *journal_mode = NULL;
    bool use_wal = false;
    unsigned long reader_cachesize = 8000; /* default 8 MB */
    int version;
    int compress_flag;
    char sqltext[64];
    bool have_inflate = false;
    bool have_stmts = false;

    /* Load config.  Direct reader mode requires WAL; without WAL, writers
     * take EXCLUSIVE locks that block all other connections during commit.
     * The server path avoids this because reads and writes share a single
     * connection. */
    confpath = concatpath(innconf->pathetc, "ovsqlite.conf");
    top = config_parse_file(confpath);
    free(confpath);
    if (top) {
        config_param_boolean(top, "walmode", &use_wal);
        config_param_unsigned_number(top, "readercachesize",
                                     &reader_cachesize);
        config_free(top);
    }
    if (!use_wal)
        return false;

    /* Open database read-only.  Suppress warning for CANTOPEN since the
     * database may not exist yet during initial setup. */
    path = concatpath(innconf->pathoverview, OVSQLITE_DB_FILE);
    status = sqlite3_open_v2(path, &read_connection, SQLITE_OPEN_READONLY,
                             NULL);
    free(path);
    if (status != SQLITE_OK) {
        if (status != SQLITE_CANTOPEN)
            warn("ovsqlite: cannot open database for reading: %s",
                 sqlite3_errstr(status));
        read_connection = NULL;
        return false;
    }
    sqlite3_extended_result_codes(read_connection, 1);

    /* Verify the database is actually in WAL mode, not just the config.
     * The writer sets WAL mode; if it hasn't run yet or the database was
     * created without WAL, fall back to the server path. */
    status = sqlite3_exec(read_connection, "pragma journal_mode;",
                          pragma_callback, &journal_mode, NULL);
    if (status != SQLITE_OK || !journal_mode
        || strcmp(journal_mode, "wal") != 0) {
        free(journal_mode);
        sqlite3_close_v2(read_connection);
        read_connection = NULL;
        return false;
    }
    free(journal_mode);

    /* Prepare read-only statements. */
    status =
        sqlite_helper_init(&sql_read_helper, (sqlite3_stmt **) &sql_read,
                           read_connection, SQLITE_PREPARE_PERSISTENT,
                           &errmsg);
    if (status != SQLITE_OK) {
        warn("ovsqlite: cannot set up read session: %s", errmsg);
        sqlite3_free(errmsg);
        goto fail;
    }
    have_stmts = true;

    /* Set cache size. */
    if (reader_cachesize) {
        snprintf(sqltext, sizeof sqltext, "pragma cache_size = -%lu;",
                 reader_cachesize);
        status = sqlite3_exec(read_connection, sqltext, 0, NULL, &errmsg);
        if (status != SQLITE_OK) {
            warn("ovsqlite: cannot set reader cache size: %s", errmsg);
            sqlite3_free(errmsg);
        }
    }

    /* Validate schema version. */
    if (!read_misc_int("version", &version))
        goto fail;
    if (version != OVSQLITE_SCHEMA_VERSION) {
        warn("ovsqlite: incompatible database schema %d (expected %d)",
             version, OVSQLITE_SCHEMA_VERSION);
        goto fail;
    }

    /* Check compression setting. */
    if (!read_misc_int("compress", &compress_flag))
        goto fail;
    reader_use_compression = compress_flag;

#    ifdef HAVE_ZLIB
    if (reader_use_compression) {
        void const *dict;
        size_t size;

        reader_inflation.zalloc = Z_NULL;
        reader_inflation.zfree = Z_NULL;
        reader_inflation.opaque = Z_NULL;
        reader_inflation.next_in = Z_NULL;
        reader_inflation.avail_in = 0;
        status = inflateInit(&reader_inflation);
        if (status != Z_OK) {
            warn("ovsqlite: cannot set up decompression");
            goto fail;
        }
        have_inflate = true;

#        ifdef USE_DICTIONARY
        sqlite3_bind_text(sql_read.getmisc, 1, "basedict", -1,
                          SQLITE_STATIC);
        status = sqlite3_step(sql_read.getmisc);
        if (status != SQLITE_ROW) {
            warn("ovsqlite: cannot load compression dictionary: %s",
                 sqlite3_errmsg(read_connection));
            sqlite3_reset(sql_read.getmisc);
            sqlite3_clear_bindings(sql_read.getmisc);
            goto fail;
        }
        dict = sqlite3_column_blob(sql_read.getmisc, 0);
        size = sqlite3_column_bytes(sql_read.getmisc, 0);
        if (!dict || size >= sizeof reader_dictionary) {
            warn("ovsqlite: invalid compression dictionary in database");
            sqlite3_reset(sql_read.getmisc);
            sqlite3_clear_bindings(sql_read.getmisc);
            goto fail;
        }
        memcpy(reader_dictionary, dict, size);
        reader_basedict_len = size;
        sqlite3_reset(sql_read.getmisc);
        sqlite3_clear_bindings(sql_read.getmisc);
#        endif /* USE_DICTIONARY */

        reader_flate = buffer_new();
    }
#    else  /* ! HAVE_ZLIB */
    if (reader_use_compression) {
        warn("ovsqlite: database uses compression but INN was not built"
             " with zlib");
        goto fail;
    }
#    endif /* ! HAVE_ZLIB */

    direct_reader = true;
    return true;

fail:
#    ifdef HAVE_ZLIB
    if (have_inflate)
        inflateEnd(&reader_inflation);
#    endif
    if (have_stmts)
        sqlite_helper_term(&sql_read_helper, (sqlite3_stmt **) &sql_read);
    if (read_connection) {
        sqlite3_close_v2(read_connection);
        read_connection = NULL;
    }
    return false;
}


/*
**  Close the direct reader connection.
*/
static void
direct_close(void)
{
    if (!read_connection)
        return;
#    ifdef HAVE_ZLIB
    if (reader_use_compression) {
        inflateEnd(&reader_inflation);
        buffer_free(reader_flate);
        reader_flate = NULL;
    }
#    endif
    sqlite_helper_term(&sql_read_helper, (sqlite3_stmt **) &sql_read);
    sqlite3_close_v2(read_connection);
    read_connection = NULL;
    direct_reader = false;
}


static bool
server_connect(void)
{
    char *path;
    int ret;

#    ifdef HAVE_UNIX_DOMAIN_SOCKETS

    struct sockaddr_un sa;

    sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock == -1) {
        syswarn("ovsqlite: socket");
        return false;
    }
    memset(&sa, 0, sizeof sa);
    sa.sun_family = AF_UNIX;
    path = concatpath(innconf->pathrun, OVSQLITE_SERVER_SOCKET);
    strlcpy(sa.sun_path, path, sizeof(sa.sun_path));
    free(path);

    ret = connect(sock, (struct sockaddr *) &sa, SUN_LEN(&sa));

#    else  /* ! HAVE_UNIX_DOMAIN_SOCKETS */

    struct sockaddr_in sa;
    int fd;
    ssize_t got;

    path = concatpath(innconf->pathrun, OVSQLITE_SERVER_PORT);
    fd = open(path, O_RDONLY);
    if (fd == -1) {
        syswarn("ovsqlite: cannot open port file %s", path);
        return false;
    }
    free(path);
    got = read(fd, &port, sizeof port);
    if (got == -1) {
        syswarn("ovsqlite: cannot read port file");
        close(fd);
        return false;
    }
    close(fd);
    if (got < sizeof port) {
        warn("ovsqlite: unexpected EOF while reading port file");
        return false;
    }
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        syswarn("ovsqlite: socket");
        return false;
    }
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = port.port;
    sa.sin_addr.s_addr = htonl(0x7f000001UL);
    ret = connect(sock, (struct sockaddr *) &sa, sizeof sa);
#    endif /* ! HAVE_UNIX_DOMAIN_SOCKETS */

    if (ret == -1) {
        syswarn("ovsqlite: connect");
        close(sock);
        sock = -1;
        return false;
    }

    request = buffer_new();
    buffer_resize(request, 0x400);
    response = buffer_new();
    buffer_resize(response, 0x400);

    return true;
}

static void
start_request(unsigned int code)
{
    uint8_t code_r;

    buffer_set(request, NULL, 0);
    code_r = code;
    pack_later(request, 4);
    pack_now(request, &code_r, sizeof code_r);
}

static void
finish_request(void)
{
    *(uint32_t *) (void *) request->data = request->left;
}

static unsigned int
start_response(void)
{
    uint8_t code;

    unpack_later(response, 4);
    unpack_now(response, &code, sizeof code);
    return code;
}

static bool
finish_response(void)
{
    return response->left == 0;
}

static bool
write_request(void)
{
    char *data;
    size_t left;

    data = request->data + request->used;
    left = request->left;
    while (left > 0) {
        ssize_t got;

        got = write(sock, data, left);
        if (got == -1) {
            if (errno == EINTR)
                continue;
            syswarn("ovsqlite: cannot write request");
            close(sock);
            sock = -1;
            return false;
        }
        data += got;
        request->used += got;
        request->left = left -= got;
    }
    return true;
}

static bool
read_response(void)
{
    char *data;
    size_t size, response_size;

    buffer_set(response, NULL, 0);
    data = response->data;
    size = 0;
    response_size = 0;
    for (;;) {
        size_t wanted;
        ssize_t got;

        if (response_size) {
            wanted = response_size - size;
        } else {
            wanted = 5 - size;
        }
        got = read(sock, data, wanted);
        if (got == -1) {
            if (errno == EINTR)
                continue;
            syswarn("ovsqlite: cannot read response");
            close(sock);
            sock = -1;
            return false;
        }
        if (got == 0) {
            warn("ovsqlite: unexpected EOF while reading response");
            close(sock);
            sock = -1;
            return false;
        }
        response->left = size += got;
        data += got;
        if ((size_t) got == wanted) {
            if (response_size) {
                break;
            } else {
                response_size = *(uint32_t *) (void *) response->data;
                if (response_size < 5 || response_size > 0x100000) {
                    warn("ovsqlite: invalid response size");
                    close(sock);
                    sock = -1;
                    return false;
                }
                if (size >= response_size)
                    break;
                buffer_resize(response, response_size);
                data = response->data + size;
            }
        }
    }
    return true;
}

static bool
server_handshake(uint32_t mode)
{
    uint32_t version;
    unsigned int code;

    version = OVSQLITE_PROTOCOL_VERSION;
    start_request(request_hello);
    pack_now(request, &version, sizeof version);
    pack_now(request, &mode, sizeof mode);
#    ifndef HAVE_UNIX_DOMAIN_SOCKETS
    pack_now(request, port.cookie, OVSQLITE_COOKIE_LENGTH);
#    endif /* ! HAVE_UNIX_DOMAIN_SOCKETS */
    finish_request();
    if (!write_request())
        return false;

    if (!read_response())
        return false;
    code = start_response();
    if (code != response_ok) {
        close(sock);
        sock = -1;
        warn("ovsqlite: server handshake failed (%u)", code);
        return false;
    }
    if (!finish_response()) {
        close(sock);
        sock = -1;
        warn("ovsqlite: protocol failure");
        return false;
    }
    return true;
}

bool
ovsqlite_open(int mode)
{
    if (direct_reader || sock != -1) {
        warn("ovsqlite_open called more than once");
        return false;
    }

    /* Read-only mode: try direct database access (requires WAL mode).
     * Falls back to the server path if WAL is not enabled. */
    if (mode == OV_READ) {
        if (direct_open())
            return true;
        notice("ovsqlite: direct reader not available, using server");
    }

    /* Read-write mode, or WAL not enabled: connect to ovsqlite-server. */
    if (!server_connect())
        return false;
    if (!server_handshake(mode))
        return false;
    return true;
}

bool
ovsqlite_groupstats(const char *group, int *low, int *high, int *count,
                    int *flag)
{
    sqlite3_stmt *stmt;
    int status;
    char const *flag_blob;
    uint16_t groupname_len;
    unsigned int code;
    uint64_t r_low;
    uint64_t r_high;
    uint64_t r_count;
    uint16_t flag_alias_len;
    uint8_t *flag_alias;

    if (direct_reader) {
        stmt = sql_read.get_groupinfo;
        sqlite3_bind_blob(stmt, 1, group, strlen(group), SQLITE_STATIC);
        status = sqlite3_step(stmt);
        if (status != SQLITE_ROW) {
            if (status != SQLITE_DONE)
                warn("ovsqlite: groupstats query error: %s",
                     sqlite3_errmsg(read_connection));
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            return false;
        }
        if (low)
            *low = sqlite3_column_int64(stmt, 0);
        if (high)
            *high = sqlite3_column_int64(stmt, 1);
        if (count)
            *count = sqlite3_column_int64(stmt, 2);
        if (flag) {
            flag_blob = sqlite3_column_blob(stmt, 3);
            if (flag_blob)
                *flag = *flag_blob;
        }
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        return true;
    }

    /* Server path. */
    if (sock == -1) {
        warn("ovsqlite: not connected to server");
        return false;
    }
    groupname_len = strlen(group);
        start_request(request_get_groupinfo);
        pack_now(request, &groupname_len, sizeof groupname_len);
        pack_now(request, group, groupname_len);
        finish_request();
        if (!write_request())
            return false;

        if (!read_response())
            return false;
        code = start_response();
        if (code != response_groupinfo)
            return false;
        if (!unpack_now(response, &r_low, sizeof r_low))
            return false;
        if (!unpack_now(response, &r_high, sizeof r_high))
            return false;
        if (!unpack_now(response, &r_count, sizeof r_count))
            return false;
        if (!unpack_now(response, &flag_alias_len, sizeof flag_alias_len))
            return false;
        flag_alias = unpack_later(response, flag_alias_len);
        if (!flag_alias)
            return false;
        if (!finish_response())
            return false;
        if (low)
            *low = r_low;
        if (high)
            *high = r_high;
        if (count)
            *count = r_count;
        if (flag)
            *flag = *flag_alias;
        return true;
}

bool
ovsqlite_groupadd(const char *group, ARTNUM low, ARTNUM high, char *flag)
{
    uint16_t groupname_len;
    uint16_t flag_alias_len;
    uint64_t r_low;
    uint64_t r_high;
    unsigned int code;

    if (direct_reader) {
        warn("ovsqlite: groupadd not available in direct reader mode");
        return false;
    }
    if (sock == -1) {
        warn("ovsqlite: not connected to server");
        return false;
    }
    groupname_len = strlen(group);
    r_low = low;
    r_high = high;
    flag_alias_len = strcspn(flag, "\n");
    start_request(request_add_group);
    pack_now(request, &groupname_len, sizeof groupname_len);
    pack_now(request, group, groupname_len);
    pack_now(request, &r_low, sizeof r_low);
    pack_now(request, &r_high, sizeof r_high);
    pack_now(request, &flag_alias_len, sizeof flag_alias_len);
    pack_now(request, flag, flag_alias_len);
    finish_request();
    if (!write_request())
        return false;

    if (!read_response())
        return false;
    code = start_response();
    if (code != response_ok)
        return false;
    if (!finish_response())
        return false;
    return true;
}

bool
ovsqlite_groupdel(const char *group)
{
    uint16_t groupname_len;
    unsigned int code;

    if (direct_reader) {
        warn("ovsqlite: groupdel not available in direct reader mode");
        return false;
    }
    if (sock == -1) {
        warn("ovsqlite: not connected to server");
        return false;
    }
    groupname_len = strlen(group);
    start_request(request_delete_group);
    pack_now(request, &groupname_len, sizeof groupname_len);
    pack_now(request, group, groupname_len);
    finish_request();
    if (!write_request())
        return false;

    if (!read_response())
        return false;
    code = start_response();
    if (code != response_ok)
        return false;
    if (!finish_response())
        return false;
    return true;
}

bool
ovsqlite_add(const char *group, ARTNUM artnum, TOKEN token, char *data,
             int len, time_t arrived, time_t expires)
{
    uint16_t groupname_len;
    uint64_t r_artnum;
    uint32_t overview_len;
    uint64_t r_arrived;
    uint64_t r_expires;
    unsigned int code;

    if (direct_reader) {
        warn("ovsqlite: add not available in direct reader mode");
        return false;
    }
    if (sock == -1) {
        warn("ovsqlite: not connected to server");
        return false;
    }

    groupname_len = strlen(group);
    r_artnum = artnum;
    overview_len = len;
    r_arrived = arrived;
    r_expires = expires;

    if (overview_len > MAX_OVDATA_SIZE) {
        warn("Too large overview data of %u bytes (most certainly spam)",
             overview_len);
        return false;
    }

    start_request(request_add_article);
    pack_now(request, &groupname_len, sizeof groupname_len);
    pack_now(request, group, groupname_len);
    pack_now(request, &r_artnum, sizeof r_artnum);
    pack_now(request, &r_arrived, sizeof r_arrived);
    pack_now(request, &r_expires, sizeof r_expires);
    pack_now(request, &token, sizeof token);
    pack_now(request, &overview_len, sizeof overview_len);
    pack_now(request, data, overview_len);
    finish_request();
    if (!write_request())
        return false;

    if (!read_response())
        return false;
    code = start_response();
    if (!finish_response())
        return false;
    switch (code) {
    case response_ok:
    case response_no_group:
        /* Handle unknown newsgroups as a success.
         * For instance for crossposts to newsgroups not present or no longer
         * present in active. */
        return true;
    default:
        return false;
    }
}

bool
ovsqlite_cancel(const char *group, ARTNUM artnum)
{
    uint16_t groupname_len;
    uint64_t r_artnum;
    unsigned int code;

    if (direct_reader) {
        warn("ovsqlite: cancel not available in direct reader mode");
        return false;
    }
    if (sock == -1) {
        warn("ovsqlite: not connected to server");
        return false;
    }
    groupname_len = strlen(group);
    r_artnum = artnum;
    start_request(request_delete_article);
    pack_now(request, &groupname_len, sizeof groupname_len);
    pack_now(request, group, groupname_len);
    pack_now(request, &r_artnum, sizeof r_artnum);
    finish_request();
    if (!write_request())
        return false;

    if (!read_response())
        return false;
    code = start_response();
    if (code != response_ok)
        return false;
    if (!finish_response())
        return false;
    return true;
}

void *
ovsqlite_opensearch(const char *group, int low, int high)
{
    handle_t *rh;
    uint16_t groupname_len;

    if (!direct_reader && sock == -1) {
        warn("ovsqlite: not connected to server");
        return NULL;
    }
    groupname_len = strlen(group);
    rh = xmalloc(offsetof(handle_t, groupname) + groupname_len);
    rh->low = low;
    rh->high = high;
    rh->count = 0;
    rh->index = 0;
    rh->groupname_len = groupname_len;
    rh->cols = 0;
    rh->done = false;
    memcpy(rh->groupname, group, groupname_len);
    return rh;
}

/*
**  Fill the search buffer directly from SQLite for direct reader mode.
**
**  Collects rows into temporary storage, then packs into the handle_t
**  buffer in the same layout that fill_search_buffer produces:
**    [overview ptrs (count+1)]  [arrived]  [artnums]  [tokens]  [overview data]
**  The consumer (ovsqlite_search) computes overview length as
**    overview[ix+1] - overview[ix]
**  so overview[count] must be a valid sentinel pointer.
*/
static bool
fill_search_buffer_direct(handle_t *rh)
{
    unsigned int cols;
    size_t per_row;
    uint32_t max_rows, count, ix;
    sqlite3_stmt *stmt;
    int status;
    uint8_t *store;

    /* Temporary storage for collected rows. */
    ARTNUM *tmp_artnum;
    time_t *tmp_arrived;
    TOKEN *tmp_token;
    uint8_t *tmp_overview;
    uint32_t *tmp_ov_offset;
    uint32_t *tmp_ov_len;
    size_t ov_total;

    rh->count = 0;
    rh->index = 0;
    cols = rh->cols;

    /* Calculate per-row metadata size in the final buffer. */
    per_row = sizeof(ARTNUM);
    if (cols & search_col_arrived)
        per_row += sizeof(time_t);
    if (cols & search_col_token)
        per_row += sizeof(TOKEN);
    if (cols & search_col_overview)
        per_row += sizeof(char *);

    /* Rough upper bound on rows that can fit. */
    max_rows = SEARCHSPACE / per_row;
    if (max_rows > 4096)
        max_rows = 4096;

    /* Allocate temporary arrays. */
    tmp_artnum = xmalloc(max_rows * sizeof(ARTNUM));
    tmp_arrived = (cols & search_col_arrived)
                      ? xmalloc(max_rows * sizeof(time_t))
                      : NULL;
    tmp_token = (cols & search_col_token)
                    ? xmalloc(max_rows * sizeof(TOKEN))
                    : NULL;
    tmp_overview = (cols & search_col_overview)
                       ? xmalloc(SEARCHSPACE)
                       : NULL;
    tmp_ov_offset = (cols & search_col_overview)
                        ? xmalloc(max_rows * sizeof(uint32_t))
                        : NULL;
    tmp_ov_len = (cols & search_col_overview)
                     ? xmalloc(max_rows * sizeof(uint32_t))
                     : NULL;

    /* Select the appropriate prepared statement. */
    stmt = (cols & search_col_overview) ? sql_read.list_articles_high_overview
                                        : sql_read.list_articles_high;
    sqlite3_bind_blob(stmt, 1, rh->groupname, rh->groupname_len,
                      SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, rh->low);
    sqlite3_bind_int64(stmt, 3, rh->high);

    /* Collect rows. */
    count = 0;
    ov_total = 0;
    while (count < max_rows) {
        uint64_t artnum;
        size_t meta_needed;

        status = sqlite3_step(stmt);
        if (status == SQLITE_DONE) {
            rh->done = true;
            break;
        }
        if (status != SQLITE_ROW) {
            warn("ovsqlite: search query error: %s",
                 sqlite3_errmsg(read_connection));
            break;
        }

        artnum = sqlite3_column_int64(stmt, 0);

        if (cols & search_col_overview) {
            uint8_t const *overview;
            uint32_t overview_len;
            size_t size;

            overview = sqlite3_column_blob(stmt, 4);
            size = sqlite3_column_bytes(stmt, 4);
            if (!overview || size > MAX_OVDATA_SIZE)
                continue;
            overview_len = size;

#    ifdef HAVE_ZLIB
            if (reader_use_compression && overview_len > 0) {
                uint8_t *dec;
                uint32_t raw_len;

                dec = reader_decompress(overview, overview_len, rh->groupname,
                                        rh->groupname_len, artnum, &raw_len);
                if (!dec)
                    continue;
                overview = dec;
                overview_len = raw_len;
            }
#    endif

            /* Check if adding this row would overflow the buffer.
             * Account for the sentinel overview pointer. */
            meta_needed = (count + 2) * sizeof(char *) /* overview ptrs */
                          + (count + 1) * sizeof(ARTNUM);
            if (cols & search_col_arrived)
                meta_needed += (count + 1) * sizeof(time_t);
            if (cols & search_col_token)
                meta_needed += (count + 1) * sizeof(TOKEN);
            if (meta_needed + ov_total + overview_len > SEARCHSPACE)
                break;

            tmp_ov_offset[count] = ov_total;
            tmp_ov_len[count] = overview_len;
            memcpy(tmp_overview + ov_total, overview, overview_len);
            ov_total += overview_len;
        } else {
            /* No overview: just check metadata fits. */
            if ((count + 1) * per_row > SEARCHSPACE)
                break;
        }

        tmp_artnum[count] = artnum;
        if (cols & search_col_arrived)
            tmp_arrived[count] = sqlite3_column_int64(stmt, 1);
        if (cols & search_col_token) {
            TOKEN const *tk = sqlite3_column_blob(stmt, 3);

            if (tk && sqlite3_column_bytes(stmt, 3) == sizeof(TOKEN))
                tmp_token[count] = *tk;
            else
                memset(&tmp_token[count], 0, sizeof(TOKEN));
        }

        count++;
    }

    if (count > 0)
        rh->low = tmp_artnum[count - 1] + 1;
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);

    /* Pack into rh->buffer in the canonical layout. */
    store = rh->buffer;
    rh->overview = (char **) (void *) store;
    if (cols & search_col_overview)
        store += (count + 1) * sizeof(char *);
    rh->arrived = (time_t *) (void *) store;
    if (cols & search_col_arrived)
        store += count * sizeof(time_t);
    rh->artnum = (ARTNUM *) (void *) store;
    store += count * sizeof(ARTNUM);
    rh->token = (TOKEN *) store;
    if (cols & search_col_token)
        store += count * sizeof(TOKEN);

    for (ix = 0; ix < count; ix++) {
        rh->artnum[ix] = tmp_artnum[ix];
        if (cols & search_col_arrived)
            rh->arrived[ix] = tmp_arrived[ix];
        if (cols & search_col_token)
            rh->token[ix] = tmp_token[ix];
    }

    if (cols & search_col_overview) {
        /* Copy overview data contiguously after the metadata arrays
         * and set up pointers. */
        memcpy(store, tmp_overview, ov_total);
        for (ix = 0; ix < count; ix++)
            rh->overview[ix] = (char *) store + tmp_ov_offset[ix];
        rh->overview[count] = (char *) store + ov_total;
    }

    /* Clean up temporary storage. */
    free(tmp_artnum);
    free(tmp_arrived);
    free(tmp_token);
    free(tmp_overview);
    free(tmp_ov_offset);
    free(tmp_ov_len);

    rh->count = count;
    return true;
}

static bool
fill_search_buffer(handle_t *rh)
{
    unsigned int cols;
    uint32_t space;
    uint8_t flags;
    unsigned int code;
    uint32_t count, ix;
    uint8_t *store;
    size_t wiresize, storesize, storespace;
    uint8_t resp_cols;

    rh->count = 0;
    rh->index = 0;
    storespace = SEARCHSPACE;
    wiresize = 8;
    storesize = sizeof(ARTNUM);
    cols = rh->cols;
    if (cols & search_col_arrived) {
        wiresize += 8;
        storesize += sizeof(time_t);
    }
    if (cols & search_col_token) {
        wiresize += sizeof(TOKEN);
        storesize += sizeof(TOKEN);
    }
    if (cols & search_col_overview) {
        wiresize += 4;
        storesize += sizeof(char *);
        storespace -= sizeof(char *);
    }
    space = storespace / storesize * wiresize + 10;
    flags = search_flag_high;

    start_request(request_search_group);
    pack_now(request, &space, sizeof space);
    pack_now(request, &flags, sizeof flags);
    pack_now(request, &rh->cols, sizeof rh->cols);
    pack_now(request, &rh->groupname_len, sizeof rh->groupname_len);
    pack_now(request, rh->groupname, rh->groupname_len);
    pack_now(request, &rh->low, sizeof rh->low);
    pack_now(request, &rh->high, sizeof rh->high);
    finish_request();
    if (!write_request())
        return false;

    if (!read_response())
        return false;
    code = start_response();
    switch (code) {
    case response_artlist:
        break;
    case response_artlist_done:
        rh->done = true;
        break;
    default:
        return false;
    }
    if (!unpack_now(response, &resp_cols, sizeof resp_cols))
        return false;
    if (resp_cols != cols)
        return false;
    if (!unpack_now(response, &count, sizeof count))
        return false;
    store = rh->buffer;
    rh->overview = (char **) (void *) store;
    if (cols & search_col_overview)
        store += (count + 1) * sizeof(char **);
    rh->arrived = (time_t *) (void *) store;
    if (cols & search_col_arrived)
        store += count * sizeof(time_t);
    rh->artnum = (ARTNUM *) (void *) store;
    store += count * sizeof(ARTNUM);
    rh->token = (TOKEN *) store;
    if (cols & search_col_token)
        store += count * sizeof(TOKEN);
    if (store > rh->buffer + SEARCHSPACE) {
        warn("ovsqlite: server returned excessive result count");
        return false;
    }
    for (ix = 0; ix < count; ix++) {
        uint64_t artnum;

        if (!unpack_now(response, &artnum, sizeof artnum))
            return false;
        rh->artnum[ix] = artnum;
        if (cols & search_col_arrived) {
            uint64_t arrived;

            if (!unpack_now(response, &arrived, sizeof arrived))
                return false;
            rh->arrived[ix] = arrived;
        }
        if (cols & search_col_token) {
            if (!unpack_now(response, rh->token + ix, sizeof(TOKEN)))
                return false;
        }
        if (cols & search_col_overview) {
            uint32_t overview_len;

            if (!unpack_now(response, &overview_len, sizeof overview_len))
                return false;
            if (store + overview_len > rh->buffer + SEARCHSPACE)
                return false;
            if (!unpack_now(response, store, overview_len))
                return false;
            rh->overview[ix] = (char *) store;
            store += overview_len;
        }
    }
    if (!finish_response())
        return false;
    if (cols & search_col_overview)
        rh->overview[count] = (char *) store;
    rh->count = count;
    return true;
}

bool
ovsqlite_search(void *handle, ARTNUM *artnum, char **data, int *len,
                TOKEN *token, time_t *arrived)
{
    handle_t *rh;
    unsigned int cols;
    unsigned int ix;

    if (!direct_reader && sock == -1) {
        warn("ovsqlite: not connected to server");
        return false;
    }
    rh = handle;
    if (!rh)
        return false;
    ix = rh->index;
    if (rh->done && ix >= rh->count)
        return false;
    cols = 0;
    if (arrived)
        cols |= search_col_arrived;
    if (token)
        cols |= search_col_token;
    if (data || len)
        cols |= search_col_overview;
    if (cols & ~rh->cols || ix >= rh->count) {
        rh->cols = cols;
        if (direct_reader) {
            if (!fill_search_buffer_direct(rh))
                return false;
        } else {
            if (!fill_search_buffer(rh))
                return false;
        }
        ix = rh->index;
        if (ix >= rh->count)
            return false;
    }
    if (artnum)
        *artnum = rh->artnum[ix];
    if (data)
        *data = rh->overview[ix];
    if (len)
        *len = rh->overview[ix + 1] - rh->overview[ix];
    if (token)
        *token = rh->token[ix];
    if (arrived)
        *arrived = rh->arrived[ix];
    rh->low = rh->artnum[ix] + 1;
    rh->index = ix + 1;
    return true;
}

void
ovsqlite_closesearch(void *handle)
{
    if (!direct_reader && sock == -1)
        warn("ovsqlite: not connected to server");
    if (!handle)
        return;
    free(handle);
}

bool
ovsqlite_getartinfo(const char *group, ARTNUM artnum, TOKEN *token)
{
    sqlite3_stmt *stmt;
    TOKEN const *db_token;
    int status;
    uint16_t groupname_len;
    uint64_t r_artnum;
    unsigned int code;

    if (direct_reader) {
        stmt = sql_read.get_artinfo;
        sqlite3_bind_blob(stmt, 1, group, strlen(group), SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, artnum);
        status = sqlite3_step(stmt);
        if (status != SQLITE_ROW) {
            if (status != SQLITE_DONE)
                warn("ovsqlite: getartinfo query error: %s",
                     sqlite3_errmsg(read_connection));
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            return false;
        }
        db_token = sqlite3_column_blob(stmt, 0);
        if (!db_token || sqlite3_column_bytes(stmt, 0) != sizeof(TOKEN)) {
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            return false;
        }
        *token = *db_token;
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        return true;
    }

    /* Server path. */
    if (sock == -1) {
        warn("ovsqlite: not connected to server");
        return false;
    }
    groupname_len = strlen(group);
    r_artnum = artnum;
    start_request(request_get_artinfo);
    pack_now(request, &groupname_len, sizeof groupname_len);
    pack_now(request, group, groupname_len);
    pack_now(request, &r_artnum, sizeof r_artnum);
    finish_request();
    if (!write_request())
        return false;

    if (!read_response())
        return false;
    code = start_response();
    if (code != response_artinfo)
        return false;
    if (!unpack_now(response, token, sizeof(TOKEN)))
        return false;
    if (!finish_response())
        return false;
    return true;
}

static bool
expire_one(char const *group, int *low, struct history *h)
{
    unsigned int code;
    uint32_t space = SEARCHSPACE;
    uint16_t groupname_len = strlen(group);
    uint8_t flags = 0;
    uint8_t cols;
    uint64_t r_low = 1;
    bool done = false;

    start_request(request_start_expire_group);
    pack_now(request, &groupname_len, sizeof groupname_len);
    pack_now(request, group, groupname_len);
    finish_request();
    if (!write_request())
        return false;

    if (!read_response())
        return false;
    code = start_response();
    switch (code) {
    case response_ok:
        break;
    default:
        return false;
    }

    cols = search_col_token | search_col_overview;
    if (innconf->groupbaseexpiry)
        cols |= search_col_arrived | search_col_expires;

    do {
        size_t off_count;
        uint32_t count, ix;
        uint32_t delcount;
        uint8_t resp_cols;

        start_request(request_search_group);
        pack_now(request, &space, sizeof space);
        pack_now(request, &flags, sizeof flags);
        pack_now(request, &cols, sizeof cols);
        pack_now(request, &groupname_len, sizeof groupname_len);
        pack_now(request, group, groupname_len);
        pack_now(request, &r_low, sizeof r_low);
        finish_request();
        if (!write_request())
            return false;

        if (!read_response())
            return false;
        code = start_response();
        switch (code) {
        case response_artlist:
            break;
        case response_artlist_done:
            done = true;
            break;
        default:
            return false;
        }
        if (!unpack_now(response, &resp_cols, sizeof resp_cols))
            return false;
        if (resp_cols != cols)
            return false;
        if (!unpack_now(response, &count, sizeof count))
            return false;

        start_request(request_expire_group);
        pack_now(request, &groupname_len, sizeof groupname_len);
        pack_now(request, group, groupname_len);
        delcount = 0;
        off_count = pack_later(request, sizeof delcount);

        for (ix = 0; ix < count; ix++) {
            uint64_t artnum;
            uint64_t arrived = 0;
            uint64_t expires = 0;
            TOKEN token;
            char *overview;
            uint32_t overview_len;
            bool delete = false;
            ARTHANDLE *ah = NULL;

            if (!unpack_now(response, &artnum, sizeof artnum))
                return false;
            if (innconf->groupbaseexpiry) {
                if (!unpack_now(response, &arrived, sizeof arrived))
                    return false;
                if (!unpack_now(response, &expires, sizeof expires))
                    return false;
            }
            if (!unpack_now(response, &token, sizeof token))
                return false;
            if (!unpack_now(response, &overview_len, sizeof overview_len))
                return false;
            overview = unpack_later(response, overview_len);
            if (!overview)
                return false;

            if (!SMprobe(EXPENSIVESTAT, &token, NULL) || OVstatall) {
                ah = SMretrieve(token, RETR_STAT);
                if (ah) {
                    SMfreearticle(ah);
                } else {
                    delete = true;
                }
            } else {
                delete = !OVhisthasmsgid(h, overview);
            }
            if (!delete && innconf->groupbaseexpiry) {
                delete = OVgroupbasedexpire(token, group, overview,
                                            overview_len, arrived, expires);
            }
            if (delete) {
                pack_now(request, &artnum, sizeof artnum);
                delcount++;
            }
            r_low = artnum + 1;
        }
        if (!finish_response())
            return false;

        if (delcount > 0) {
            memcpy(request->data + off_count, &delcount, sizeof delcount);
            finish_request();
            if (!write_request())
                return false;

            if (!read_response())
                return false;
            code = start_response();
            if (code != response_ok)
                return false;
            if (!finish_response())
                return false;
        }
    } while (!done);
    if (low != NULL)
        ovsqlite_groupstats(group, low, NULL, NULL, NULL);
    return true;
}

static bool
expire_finish(void)
{
    bool done = false;

    do {
        unsigned int code;

        start_request(request_finish_expire);
        finish_request();
        if (!write_request())
            return false;

        if (!read_response())
            return false;
        code = start_response();
        switch (code) {
        case response_ok:
            break;
        case response_done:
            done = true;
            break;
        default:
            return false;
        }
        if (!finish_response())
            return false;
    } while (!done);
    return true;
}

bool
ovsqlite_expiregroup(char const *group, int *low, struct history *h)
{
    if (direct_reader) {
        warn("ovsqlite: expiregroup not available in direct reader mode");
        return false;
    }
    if (sock == -1) {
        warn("ovsqlite: not connected to server");
        return false;
    }
    if (group) {
        return expire_one(group, low, h);
    } else {
        return expire_finish();
    }
}

static bool
set_cutofflow(uint8_t cutofflow)
{
    unsigned int code;

    start_request(request_set_cutofflow);
    pack_now(request, &cutofflow, sizeof cutofflow);
    finish_request();
    if (!write_request())
        return false;

    if (!read_response())
        return false;
    code = start_response();
    if (code != response_ok)
        return false;
    if (!finish_response())
        return false;
    return true;
}

bool
ovsqlite_ctl(OVCTLTYPE type, void *val)
{
    if (!direct_reader && sock == -1) {
        warn("ovsqlite: not connected to server");
        return false;
    }
    switch (type) {
    case OVSPACE:
        *(float *) val = -1.0f;
        return true;
    case OVSORT:
        *(OVSORTTYPE *) val = OVNEWSGROUP;
        return true;
    case OVCUTOFFLOW:
        if (direct_reader)
            return true; /* no-op for readers */
        return set_cutofflow(*(bool *) val);
    case OVSTATICSEARCH:
        *(int *) val = true;
        return true;
    case OVCACHEKEEP:
    case OVCACHEFREE:
        *(bool *) val = false;
        return true;
    default:
        return false;
    }
}

void
ovsqlite_close(void)
{
    if (direct_reader) {
        direct_close();
        return;
    }
    if (sock == -1) {
        warn("ovsqlite: not connected to server");
        return;
    }
    close(sock);
    sock = -1;
    buffer_free(request);
    request = NULL;
    buffer_free(response);
    response = NULL;
}

#else /* ! HAVE_SQLITE3 */

bool
ovsqlite_open(int mode UNUSED)
{
    warn("ovsqlite: SQLite support not enabled");
    return false;
}

bool
ovsqlite_groupstats(const char *group UNUSED, int *lo UNUSED, int *hi UNUSED,
                    int *count UNUSED, int *flag UNUSED)
{
    return false;
}

bool
ovsqlite_groupadd(const char *group UNUSED, ARTNUM lo UNUSED, ARTNUM hi UNUSED,
                  char *flag UNUSED)
{
    return false;
}

bool
ovsqlite_groupdel(const char *group UNUSED)
{
    return false;
}

bool
ovsqlite_add(const char *group UNUSED, ARTNUM artnum UNUSED,
             TOKEN token UNUSED, char *data UNUSED, int len UNUSED,
             time_t arrived UNUSED, time_t expires UNUSED)
{
    return false;
}

bool
ovsqlite_cancel(const char *group UNUSED, ARTNUM artnum UNUSED)
{
    return false;
}

void *
ovsqlite_opensearch(const char *group UNUSED, int low UNUSED, int high UNUSED)
{
    return NULL;
}

bool
ovsqlite_search(void *handle UNUSED, ARTNUM *artnum UNUSED, char **data UNUSED,
                int *len UNUSED, TOKEN *token UNUSED, time_t *arrived UNUSED)
{
    return false;
}

void
ovsqlite_closesearch(void *handle UNUSED)
{
}

bool
ovsqlite_getartinfo(const char *group UNUSED, ARTNUM artnum UNUSED,
                    TOKEN *token UNUSED)
{
    return false;
}

bool
ovsqlite_expiregroup(const char *group UNUSED, int *lo UNUSED,
                     struct history *h UNUSED)
{
    return false;
}

bool
ovsqlite_ctl(OVCTLTYPE type UNUSED, void *val UNUSED)
{
    return false;
}

void
ovsqlite_close(void)
{
}

#endif /* ! HAVE_SQLITE3 */
