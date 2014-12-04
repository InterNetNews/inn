/* $Id$
 *
 * Replacement for a missing setenv.
 *
 * Provides the same functionality as the standard library routine setenv for
 * those platforms that don't have it.
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

/*
 * If we're running the test suite, rename setenv to avoid conflicts with
 * the system version.
 */
#if TESTING
# undef setenv
# define setenv test_setenv
int test_setenv(const char *, const char *, int);
#endif

int
setenv(const char *name, const char *value, int overwrite)
{
    char *envstring;

    /* Do nothing if not overwriting and the variable is already set. */
    if (!overwrite && getenv(name) != NULL)
        return 0;

    /*
     * Build the environment string and add it to the environment using
     * putenv.  Systems without putenv lose, but XPG4 requires it.
     *
     * We intentionally don't use the xmalloc family of allocation routines
     * here, since the intention is to provide a replacement for the standard
     * library function that sets errno and returns in the event of a memory
     * allocation failure.
     */
    if (asprintf(&envstring, "%s=%s", name, value) < 0)
        return -1;
    return putenv(envstring);

    /*
     * Note that the memory allocated is not freed.  This is intentional; many
     * implementations of putenv assume that the string passed to putenv will
     * never be freed and don't make a copy of it.  Repeated use of this
     * function will therefore leak memory, since most implementations of
     * putenv also don't free strings removed from the environment (due to
     * being overwritten).
     */
}
