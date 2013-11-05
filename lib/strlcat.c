/* $Id$
 *
 * Replacement for a missing strlcat.
 *
 * Provides the same functionality as the *BSD function strlcat, originally
 * developed by Todd Miller and Theo de Raadt.  strlcat works similarly to
 * strncat, except simpler.  The result is always nul-terminated even if the
 * source string is longer than the space remaining in the destination string,
 * and the total space required is returned.  The third argument is the total
 * space available in the destination buffer, not just the amount of space
 * remaining.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <rra@stanford.edu>
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

/*
 * If we're running the test suite, rename strlcat to avoid conflicts with
 * the system version.
 */
#if TESTING
# define strlcat test_strlcat
size_t test_strlcat(char *, const char *, size_t);
#endif

size_t
strlcat(char *dst, const char *src, size_t size)
{
    size_t used, length, copy;

    used = strlen(dst);
    length = strlen(src);
    if (size > 0 && used < size - 1) {
        copy = (length >= size - used) ? size - used - 1 : length;
        memcpy(dst + used, src, copy);
        dst[used + copy] = '\0';
    }
    return used + length;
}
