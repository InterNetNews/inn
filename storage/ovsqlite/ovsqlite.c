/*
**  Overview storage method based on SQLite.
**
**  Original implementation written by Bo Lindbergh (2020-12-17).
**  <2bfjdsla52kztwejndzdstsxl9athp@gmail.com>
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
#    include "inn/fdflag.h"
#    include "inn/innconf.h"
#    include "inn/libinn.h"
#    include "inn/newsuser.h"
#    include "inn/paths.h"

#    include "../ovinterface.h"

#    define SEARCHSPACE 0x20000

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

static int sock = -1;
static buffer_t *request;
static buffer_t *response;

#    ifndef HAVE_UNIX_DOMAIN_SOCKETS
static ovsqlite_port port;
#    endif

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

        got = write(sock, request->data, request->left);
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
    if (sock != -1) {
        warn("ovsqlite_open called more than once");
        return false;
    }
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
    uint16_t groupname_len;
    unsigned int code;
    uint64_t r_low;
    uint64_t r_high;
    uint64_t r_count;
    uint16_t flag_alias_len;
    uint8_t *flag_alias;

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

    if (sock == -1) {
        warn("ovsqlite: not connected to server");
        return false;
    }
    groupname_len = strlen(group);
    r_artnum = artnum;
    overview_len = len;
    r_arrived = arrived;
    r_expires = expires;
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

    if (sock == -1) {
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

    if (sock == -1) {
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
        if (!fill_search_buffer(rh))
            return false;
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
    if (sock == -1)
        warn("ovsqlite: not connected to server");
    if (!handle)
        return;
    free(handle);
}

bool
ovsqlite_getartinfo(const char *group, ARTNUM artnum, TOKEN *token)
{
    uint16_t groupname_len;
    uint64_t r_artnum;
    unsigned int code;

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
    ARTNUM new_low = 0;
    ARTNUM new_high = 0;
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
            if (!delete &&innconf->groupbaseexpiry) {
                delete = OVgroupbasedexpire(token, group, overview,
                                            overview_len, arrived, expires);
            }
            if (delete) {
                pack_now(request, &artnum, sizeof artnum);
                delcount++;
            } else {
                if (!new_low)
                    new_low = artnum;
            }
            new_high = artnum;
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
    if (!new_low)
        new_low = new_high + 1;
    if (low)
        *low = new_low;
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
    if (sock == -1) {
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
    if (sock == -1) {
        warn("ovsqlite: not connected to server");
        return;
    }
    close(sock);
    sock = -1;
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
