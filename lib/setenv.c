/*  $Id$
**
**  Replacement for a missing setenv.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  This work is hereby placed in the public domain by its author.
**
**  Provides the same functionality as the standard library routine setenv
**  for those platforms that don't have it.
*/

#include "config.h"
#include "clibrary.h"

/* If we're running the test suite, rename setenv to avoid conflicts with
   the system version. */
#if TESTING
# define setenv test_setenv
int test_setenv(const char *, const char *, int);
#endif

int
setenv(const char *name, const char *value, int overwrite)
{
    char *envstring;
    size_t size;

    if (!overwrite && getenv(name) != NULL)
        return 0;

    /* Allocate memory for the environment string.  We intentionally don't
       use concat here, or the xmalloc family of allocation routines, since
       the intention is to provide a replacement for the standard library
       function which sets errno and returns in the event of a memory
       allocation failure. */
    size = strlen(name) + 1 + strlen(value) + 1;
    envstring = malloc(size);
    if (envstring == NULL)
        return -1;

    /* Build the environment string and add it to the environment using
       putenv.  Systems without putenv lose, but XPG4 requires it. */
    strlcpy(envstring, name, size);
    strlcat(envstring, "=", size);
    strlcat(envstring, value, size);
    return putenv(envstring);

    /* Note that the memory allocated is not freed.  This is intentional;
       many implementations of putenv assume that the string passed to
       putenv will never be freed and don't make a copy of it.  Repeated use
       of this function will therefore leak memory, since most
       implementations of putenv also don't free strings removed from the
       environment (due to being overwritten). */
}
