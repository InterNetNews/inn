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

#include "config.h"

/* Our declarations of sys_nerr and sys_errlist may conflict with the ones
   provided by stdio.h from glibc.  Defining __STRICT_ANSI__ ensures that
   glibc won't attempt to provide any declarations.  (The conflicts are just
   whether or not to const, so there are no negative effects from using our
   declarations. */
#ifndef __STRICT_ANSI__
# define __STRICT_ANSI__ 1
#endif

extern const int sys_nerr;
extern const char *sys_errlist[];

#include <errno.h>
#include <stdio.h>

/* If we're running the test suite, rename strerror to avoid conflicts with
   the system version. */
#if TESTING
# define strerror test_strerror
const char *test_strerror(int);
#endif

const char *
strerror(int error)
{
    static char buff[32];
    int oerrno;

    if (error >= 0 && error < sys_nerr)
        return sys_errlist[error];
    oerrno = errno;
    snprintf(buff, sizeof(buff), "Error code %d", error);
    errno = oerrno;
    return buff;
}
