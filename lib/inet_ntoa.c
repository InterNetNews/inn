/*  $Id$
**
**  Replacement for a missing or broken inet_ntoa.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  This work is hereby placed in the public domain by its author.
**
**  Provides the same functionality as the standard library routine
**  inet_ntoa for those platforms that don't have it or where it doesn't
**  work right (such as on IRIX when using gcc to compile).  inet_ntoa is
**  not thread-safe since it uses static storage (inet_ntop should be used
**  instead when available).
*/

#include "config.h"
#include "clibrary.h"
#include <netinet/in.h>

/* If we're running the test suite, rename inet_ntoa to avoid conflicts with
   the system version. */
#if TESTING
# define inet_ntoa test_inet_ntoa
const char *test_inet_ntoa(const struct in_addr);
#endif

const char *
inet_ntoa(const struct in_addr in)
{
    static char buf[16];
    const unsigned char *p;

    p = (const unsigned char *) &in.s_addr;
    sprintf(buf, "%u.%u.%u.%u",
            (unsigned int) (p[0] & 0xff), (unsigned int) (p[1] & 0xff),
            (unsigned int) (p[2] & 0xff), (unsigned int) (p[3] & 0xff));
    return buf;
}
