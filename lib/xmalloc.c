/*  $Id$
**
**  malloc and realloc which call a memory failure handler on failure and
**  never returns NULL, so that the rest of the application doesn't have to
**  check malloc's return status.  The default xmemfailure exits the
**  application, but this implementation allows for one that waits for a
**  while and then returns, or performs some other emergency action and will
**  continue calling malloc as long as xmemfailure keeps returning.
*/
#include "config.h"
#include "libinn.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef STDC_HEADERS
# include <string.h>
#else
# ifdef HAVE_MEMORY_H
#  include <memory.h>
# endif
# ifdef HAVE_STRING_H
#  include <string.h>
# endif
#endif

/* Assume this will be provided if the system doesn't have it. */
#ifndef HAVE_STRERROR
extern char *strerror();
#endif

void *
xmalloc(size_t size, const char *file, int line)
{
    void *p;

    p = malloc(size);
    while (p == NULL) {
        (*xmemfailure)("malloc", size, file, line);
        p = malloc(size);
    }
    return p;
}

void *
xrealloc(void *p, size_t size, const char *file, int line)
{
    void *newp;

    newp = realloc(p, size);
    while (newp == NULL) {
        (*xmemfailure)("remalloc", size, file, line);
        newp = realloc(p, size);
    }
    return newp;
}

char *
xstrdup(const char *s, const char *file, int line)
{
    char *p;

    p = strdup(s);
    while (p == NULL) {
        (*xmemfailure)("strdup", strlen(s), file, line);
        p = strdup(s);
    }
    return p;
}

static int
xmemerr(const char *what, size_t size, const char *file, int line)
{
    fprintf(stderr, "%s:%d Can\'t %s %lu bytes: %s",
            file, line, what, (unsigned long) size, strerror(errno));
    exit(1);
}

/* Set the default error handler. */
int (*xmemfailure)(const char *, size_t, const char *, int) = xmemerr;
