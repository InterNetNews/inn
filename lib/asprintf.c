/*  $Id$
**
**  Replacement for a missing asprintf and vasprintf.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  This work is hereby placed in the public domain by its author.
**
**  Provides the same functionality as the standard GNU library routines
**  asprintf and vasprintf for those platforms that don't have them.
*/

#include "config.h"
#include "clibrary.h"

/* If we're running the test suite, rename the functions to avoid conflicts
   with the system versions. */
#if TESTING
# define asprintf test_asprintf
# define vasprintf test_vasprintf
int test_asprintf(char **, const char *, ...);
int test_vasprintf(char **, const char *, va_list);
#endif

int
asprintf(char **strp, const char *fmt, ...)
{
    va_list args;
    int status;

    va_start(args, fmt);
    status = vasprintf(strp, fmt, args);
    va_end(args);
    return status;
}

int
vasprintf(char **strp, const char *fmt, va_list args)
{
    va_list args_copy;
    int status, needed;

    va_copy(args_copy, args);
    needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (needed < 0) {
        *strp = NULL;
        return needed;
    }
    *strp = malloc(needed + 1);
    if (*strp == NULL)
        return -1;
    status = vsnprintf(*strp, needed + 1, fmt, args);
    if (status >= 0)
        return status;
    else {
        free(*strp);
        *strp = NULL;
        return status;
    }
}
