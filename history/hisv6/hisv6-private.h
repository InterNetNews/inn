#ifndef HISV6_PRIVATE_H
#define HISV6_PRIVATE_H

#include "config.h"
#include <stdio.h>
#include <time.h>
#include <syslog.h>
#include <sys/stat.h>
#include "inn/history.h"
#include "inn/storage.h"
#include "inn/libinn.h"

/* Used by lots of stuff that parses history file entries.  Should be moved
   into a header specifically for history parsing. */
#define HISV6_BADCHAR             '_'
#define HISV6_FIELDSEP            '\t'
#define HISV6_NOEXP               '-'
#define HISV6_SUBFIELDSEP         '~'

/* maximum length of a history line:
   34 - hash
    1 - \t
   20 - arrived
    1 - ~
   20 - expires
    1 - ~
   20 - posted
    1 - tab
   38 - token
    1 - \n */
#define HISV6_MAXLINE 137

/* minimum length of a history line:
   34 - hash
    1 - \t
    1 - arrived
    1 - \n */
#define HISV6_MINLINE 37

struct hisv6 {
    char *histpath;
    FILE *writefp;
    off_t offset;               /* Offset into writefp. */
    unsigned long nextcheck;
    struct history *history;
    time_t statinterval;
    size_t synccount;
    size_t dirty;
    ssize_t npairs;
    int readfd;
    int flags;
    struct stat st;
};

/* values in the bitmap returned from hisv6_splitline */
#define HISV6_HAVE_HASH (1<<0)
#define HISV6_HAVE_ARRIVED (1<<1)
#define HISV6_HAVE_POSTED (1<<2)
#define HISV6_HAVE_EXPIRES (1<<3)
#define HISV6_HAVE_TOKEN (1<<4)

/* structure used to hold the callback and cookie so we don't try
 * passing too many parameters into the callers callback */
struct hisv6_walkstate {
    union {
	bool (*expire)(void *, time_t, time_t, time_t, TOKEN *);
	bool (*walk)(void *, time_t, time_t, time_t, const TOKEN *);
    } cb;
    void *cookie;
    bool paused;
    bool ignore;
    /* the next two fields are only used during expire... they should
     * probably be linked off of cookie, but I've been lazy */
    struct hisv6 *new;
    time_t threshold;
};

/* maximum length of the string from hisv6_errloc */
#define HISV6_MAX_LOCATION 22

#endif
