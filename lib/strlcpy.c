/*  $Id$
**
**  Replacement for a missing strlcpy.
**
**  Written by Russ Allbery <eagle@eyrie.org>
**  This work is hereby placed in the public domain by its author.
**
**  Provides the same functionality as the *BSD function strlcpy, originally
**  developed by Todd Miller and Theo de Raadt.  strlcpy works similarly to
**  strncpy, except saner and simpler.  The result is always nul-terminated
**  even if the source string is longer than the destination string, and the
**  total space required is returned.  The destination string is not
**  nul-filled like strncpy does, just nul-terminated.
*/

#include "config.h"
#include "clibrary.h"

/* If we're running the test suite, rename strlcpy to avoid conflicts with
   the system version. */
#if TESTING
# define strlcpy test_strlcpy
size_t test_strlcpy(char *, const char *, size_t);
#endif

size_t
strlcpy(char *dst, const char *src, size_t size)
{
    size_t length, copy;

    length = strlen(src);
    if (size > 0) {
        copy = (length >= size) ? size - 1 : length;
        memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return length;
}
