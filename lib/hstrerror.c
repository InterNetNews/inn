/*  $Id$
**
**  Replacement for a missing hstrerror.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  This work is hereby placed in the public domain by its author.
**
**  Provides hstrerror (strerror, but for h_errno from the resolver
**  libraries) on those platforms that don't have it (most non-BSD).  This
**  function is thread-safe unless called with an unknown h_errno.
*/

#include "config.h"
#include "clibrary.h"
#include <netdb.h>

static const char * const errors[] = {
    "No resolver error",                /* 0 NETDB_SUCCESS */
    "Unknown host",                     /* 1 HOST_NOT_FOUND */
    "Host name lookup failure",         /* 2 TRY_AGAIN */
    "Unknown server error",             /* 3 NO_RECOVERY */
    "No address associated with name",  /* 4 NO_ADDRESS / NO_DATA */
};
static int nerrors = (sizeof errors / sizeof errors[0]);

/* If we're running the test suite, rename hstrerror to avoid conflicts with
   the system version. */
#if TESTING
# define hstrerror test_hstrerror
const char *test_hstrerror(int);
#endif

const char *
hstrerror(int error)
{
    static char buf[32];

    if (error == -1) return "Internal resolver error";
    if (error >= 0 && error < nerrors) return errors[error];

    /* Paranoia.  If an int is very large (like 128 bytes) one could
       overflow the buffer here, so refuse to process particularly large
       values of error. */
    if (error > 999999 || error < -99999) return "";
    sprintf(buf, "Resolver error %d", error);
    return buf;
}
