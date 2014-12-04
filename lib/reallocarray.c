/* $Id$
 *
 * Replacement for a missing reallocarray.
 *
 * Provides the same functionality as the OpenBSD library function
 * reallocarray for those systems that don't have it.  This function is the
 * same as realloc, but takes the size arguments in the same form as calloc
 * and checks for overflow so that the caller doesn't need to.
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

#include <errno.h>

/*
 * If we're running the test suite, rename reallocarray to avoid conflicts
 * with the system version.  #undef it first because some systems may define
 * it to another name.
 */
#if TESTING
# undef reallocarray
# define reallocarray test_reallocarray
void *test_reallocarray(void *, size_t, size_t);
#endif

/*
 * nmemb * size cannot overflow if both are smaller than sqrt(SIZE_MAX).  We
 * can calculate that value statically by using 2^(sizeof(size_t) * 8) as the
 * value of SIZE_MAX and then taking the square root, which gives
 * 2^(sizeof(size_t) * 4).  Compute the exponentiation with shift.
 */
#define CHECK_THRESHOLD (1UL << (sizeof(size_t) * 4))

void *
reallocarray(void *ptr, size_t nmemb, size_t size)
{
    if (nmemb >= CHECK_THRESHOLD || size >= CHECK_THRESHOLD)
        if (nmemb > 0 && SIZE_MAX / nmemb <= size) {
            errno = ENOMEM;
            return NULL;
        }
    return realloc(ptr, nmemb * size);
}
