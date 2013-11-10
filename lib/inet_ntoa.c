/* $Id$
 *
 * Replacement for a missing or broken inet_ntoa.
 *
 * Provides the same functionality as the standard library routine inet_ntoa
 * for those platforms that don't have it or where it doesn't work right (such
 * as on IRIX when using gcc to compile).  inet_ntoa is not thread-safe since
 * it uses static storage (inet_ntop should be used instead when available).
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 *
 * The authors hereby relinquish any claim to any copyright that they may have
 * in this work, whether granted under contract or by operation of law or
 * international treaty, and hereby commit to the public, at large, that they
 * shall not, at any time in the future, seek to enforce any copyright in this
 * work against any person or entity, or prevent any person or entity from
 * copying, publishing, distributing or creating derivative works of this
 * work.
 */

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"

/*
 * If we're running the test suite, rename inet_ntoa to avoid conflicts with
 * the system version.
 */
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
