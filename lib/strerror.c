/*  $Id$
**
**  Replacement for a missing strerror.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  This work is hereby placed in the public domain by its author.
**
**  Provides the same functionality as the standard library routine strerror
**  for those platforms that don't have it (e.g. Ultrix).  Assume that we
**  have sys_nerr and sys_errlist available to use instead.  Calling
**  strerror should be thread-safe unless it is called for an unknown errno.
*/

#include <errno.h>
#include <stdio.h>

/* If we're running the test suite, rename strerror to avoid conflicts with
   the system version. */
#if TESTING
# define strerror test_strerror
#endif

const char *
strerror(int error)
{
    extern const int sys_nerr;
    extern const char *sys_errlist[];
    static char buff[32];
    int oerrno;

    if (error >= 0 && error < sys_nerr) return sys_errlist[error];

    /* Paranoia.  If an int is very large (like 128 bytes) one could
       overflow the buffer here, so refuse to process particularly large
       values of error. */
    if (error > 999999 || error < -99999) return "";
    oerrno = errno;
    sprintf(buff, "Error code %d", error);
    errno = oerrno;
    return buff;
}
