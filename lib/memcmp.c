/*  $Id$
**
**  Replacement for a missing or broken memcmp.
**
**  Written by Russ Allbery <eagle@eyrie.org>
**  This work is hereby placed in the public domain by its author.
**
**  Provides the same functionality as the standard library routine memcmp
**  for those platforms that don't have it or where it doesn't work right
**  (such as on SunOS where it can't deal with eight-bit characters).
*/

#include "config.h"
#include <sys/types.h>

/* If we're running the test suite, rename memcmp to avoid conflicts with
   the system version. */
#if TESTING
# undef memcmp
# define memcmp test_memcmp
int test_memcmp(const void *, const void *, size_t);
#endif

int
memcmp(const void *s1, const void *s2, size_t n)
{
    size_t i;
    const unsigned char *p1, *p2;

    /* It's technically illegal to call memcmp with NULL pointers, but we
       may as well check anyway. */
    if (!s1)
        return !s2 ? 0 : -1;
    if (!s2)
        return 1;

    p1 = (const unsigned char *) s1;
    p2 = (const unsigned char *) s2;
    for (i = 0; i < n; i++, p1++, p2++)
        if (*p1 != *p2)
	    return (int) *p1 - (int) *p2;
    return 0;
}
