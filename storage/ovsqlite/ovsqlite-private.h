#ifndef OVSQLITE_PRIVATE_H
#define OVSQLITE_PRIVATE_H

#include "portable/system.h"

#ifdef HAVE_SQLITE3

#    include "inn/buffer.h"

#    define OVSQLITE_SCHEMA_VERSION   1
#    define OVSQLITE_PROTOCOL_VERSION 1

#    define OVSQLITE_SERVER_SOCKET    "ovsqlite.sock"
#    define OVSQLITE_SERVER_PIDFILE   "ovsqlite.pid"

#    ifndef HAVE_UNIX_DOMAIN_SOCKETS

#        define OVSQLITE_SERVER_PORT   "ovsqlite.port"

#        define OVSQLITE_COOKIE_LENGTH 16

typedef struct ovsqlite_port {
    uint16_t port; /* in network byte order */
    uint8_t cookie[OVSQLITE_COOKIE_LENGTH];
} ovsqlite_port;

#    endif /* ! HAVE_UNIX_DOMAIN_SOCKETS */

/* A single overview record must not exceed the client buffer size, so make
 * sure that SEARCHSPACE is always slightly greater than MAX_OVDATA_SIZE.
 * 0x20000 corresponds to a client buffer size of 131072 bytes. */
#    define SEARCHSPACE     0x20000
#    define MAX_OVDATA_SIZE 100000

/*
 * This needs to stay in sync with the dispatch array
 * in ovsqlite-server.c or things will explode.
 */

enum {
    request_hello,
    request_set_cutofflow,
    request_add_group,
    request_get_groupinfo,
    request_delete_group,
    request_list_groups,
    request_add_article,
    request_get_artinfo,
    request_delete_article,
    request_search_group,
    request_start_expire_group,
    request_expire_group,
    request_finish_expire,

    count_request_codes
};

enum {
    response_ok = 0x00,
    response_done,
    response_groupinfo,
    response_grouplist,
    response_grouplist_done,
    response_artinfo,
    response_artlist,
    response_artlist_done,

    response_error = 0x80,
    response_sequence_error,
    response_sql_error,
    response_corrupted,
    response_no_group,
    response_no_article,
    response_dup_article,
    response_old_article,

    response_fatal = 0xC0,
    response_bad_request,
    response_oversized,
    response_wrong_state,
    response_wrong_version,
    response_failed_auth
};

enum {
    search_flag_high = 0x01,

    search_flags_all = 0x01
};

enum {
    search_col_arrived = 0x01,
    search_col_expires = 0x02,
    search_col_token = 0x04,
    search_col_overview = 0x08,

    search_cols_all = 0x0F
};

typedef struct buffer buffer_t;

BEGIN_DECLS

extern bool unpack_now(buffer_t *src, void *bytes, size_t count);

extern void *unpack_later(buffer_t *src, size_t count);

extern size_t pack_now(buffer_t *dst, void const *bytes, size_t count);

extern size_t pack_later(buffer_t *dst, size_t count);

END_DECLS

#endif /* HAVE_SQLITE3 */


/****************************************************************************

ovsqlite-server protocol version 1

The protocol is binary and uses no alignment padding anywhere.
All integer values are in native byte order.
This description uses "u8", "u16", etc. for unsigned integer values
and "s8", "s16", etc. for signed integer values.
Repeat counts are given in brackets.
Braces specify a group of fields to be repeated.

Each request starts with a u32 specifying the total length in bytes
(including the length itself) and a u8 containing the request code.

Each response starts with a u32 specifying the total length in bytes
(including the length itself) and a u8 containing the response code.

The server sends exactly one response for each received request.

Success responses use codes less than 0x80.
Error responses use codes from 0x80 and up.
Fatal error responses use codes from 0xC0 and up.
The server closes the connection after sending a fatal error response.


=== request formats ===

request_hello
    u32 length
    u8 code
    u32 version
    u32 mode
    u8 cookie[16]   (conditional)

This must be sent as the first (and only the first) request.
The version field specifies the expected protocol version.
The mode field contains the read/write flags.
The cookie field is present only when running on a system without
Unix-domain sockets and contains the 16-byte authentication cookie.


request_setcutofflow
    u32 length
    u8 code
    u8 cutofflow

Used by makehistory to pass along its -I option (defining whether
overview data for articles numbered lower than the lowest article
number in active should be stored).


request_add_group
    u32 length
    u8 code
    u16 groupname_len
    u8 groupname[groupname_len]
    u64 low
    u64 high
    u16 flag_alias_len
    u8 flag_alias[flag_alias_len]

Adds a new group or changes the flag of an existing group.


request_get_groupinfo
    u32 length
    u8 code
    u16 groupname_len
    u8 groupname[groupname_len]

Returns response_groupinfo on success.


request_delete_group
    u32 length
    u8 code
    u16 groupname_len
    u8 groupname[groupname_len]


request_list_groups
    u32 length
    u8 code
    u32 space
    s64 groupid

The space field specifies the largest response size wanted.
The groupid field specifies where to resume iteration and should be set
to 0 the first time around.
Returns response_grouplist or response_grouplist_done on success.


request_add_article
    u32 length
    u8 code
    u16 groupname_len
    u8 groupname[groupname_len]
    u64 artnum
    s64 arrived
    s64 expires
    u8 token[18]
    u32 overview_len
    u8 overview[overview_len]


request_get_artinfo
    u32 length
    u8 code
    u16 groupname_len
    u8 groupname[groupname_len]
    u64 artnum

Returns response_artinfo on success.


request_delete_article
    u32 length
    u8 code
    u16 groupname_len
    u8 groupname[groupname_len]
    u64 artnum


request_search_group
    u32 length
    u8 code
    u32 space
    u8 flags
    u8 cols
    u16 groupname_len
    u8 groupname[groupname_len]
    u64 low
    u64 high    (optional: search_flag_high)

The space field specifies the largest response size wanted.
The flags field specifies what optional fields are present
The cols field specifies what optional columns should be returned.


request_start_expire_group
    u32 length
    u8 code
    u16 groupname_len
    u8 groupname[groupname_len]

Sets the expire timestamp on the group to show that it hasn't been forgotten.


request_expire_group
    u32 length
    u8 code
    u16 groupname_len
    u8 groupname[groupname_len]
    u32 count
    u64 artnum[count]

Deletes multiple articles from a group.


request_finish_expire
    u32 length
    u8 code

Performs clean-up of deleted groups.  In order not to monopolise the server,
it only does a limited amount of work each time.  The client should repeat
the request until it receives a response_done.


=== response formats ===

response_ok
    u32 length
    u8 code

The generic success response.


response_done
    u32 length
    u8 code

Returned from request_finish_expire when there is no more work to be done.


response_groupinfo
    u32 length
    u8 code
    u64 low
    u64 high
    u64 count
    u16 flag_alias_len
    u8 flag_alias[flag_alias_len]

Returned from request_get_groupinfo.


response_grouplist
response_grouplist_done
    u32 length
    u8 code
    s64 groupid
    u32 count
    {
        u16 groupname_len
        u8 groupname[groupname_len]
        u64 low
        u64 high
        u64 count
        u16 flag_alias_len
        u8 flag_alias[flag_alias_len]
    } [count]

Returned from request_list_groups.
The groupid field should be copied to the next request_list_groups
to get the next batch of groups.
The code response_grouplist_done means that this is the final batch
of groups and further requests are pointless.


response_artinfo
    u32 length
    u8 code
    u8 token[18]

Returned from request_get_artinfo.


response_artlist
response_artlist_done
    u32 length
    u8 code
    u8 cols
    u32 count
    {
        u64 artnum
        s64 arrived                 (optional: search_col_arrived)
        s64 expires                 (optional: search_col_expires)
        u8 token[18]                (optional: search_col_token)
        u32 overview_len            (optional: search_col_overview)
        u8 overview[overview_len]   (optional: search_col_overview)
    } [count]

Returned from request_search_group.
The cols field specifies what optional fields are included.  It is copied
from the request.
The code response_artlist_done means that this is the last batch
of articles and further requests are pointless.


response_error
    u32 length
    u8 code

A generic error response with no further information.


response_sequence_error
    u32 length
    u8 code

Returned when the client sends requests in the wrong order, for example
request_finish_expire not preceded by at least one request_start_expire_group.


response_sql_error
    u32 length
    u8 code
    s32 status
    u32 errmsg_len
    u8 errmsg[errmsg_len]

Returned when an unexpected database error occurs.
The status and errmsg fields come directly from SQLite.


response_corrupted
    u32 length
    u8 code

Returned when incorrect or inconsistent data is found in the database.


response_no_group
    u32 length
    u8 code

Returned when a request contains an unknown group name.


response_no_article
    u32 length
    u8 code

Returned when a request contains an unknown article number.


response_dup_article
    u32 length
    u8 code

Returned from request_add_article when an article with the specified
group name and article number already exists.


response_old_article
    u32 length
    u8 code

Returned from request_add_article when the article number is less
than the group lowmark and cutofflow has been set to true.


response_fatal
    u32 length
    u8 code

A generic fatal error response with no further information.


response_bad_request
    u32 length
    u8 code

Returned from a request with an unknown code or the wrong format.


response_oversized
    u32 length
    u8 code

Returned from a request with an unreasonably large size.


response_wrong_state
    u32 length
    u8 code

Returned when the first request isn't a request_hello
or when a request_hello isn't the first request.


response_wrong_version
    u32 length
    u8 code

Returned from a request_hello with the wrong version.


response_failed_auth
    u32 length
    u8 code

Returned from a request_hello with the wrong cookie.


****************************************************************************/

#endif /* ! OVSQLITE_PRIVATE_H */
