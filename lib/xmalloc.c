/* $Id$
**
**  malloc routines with failure handling.
**
**  Usage:
**
**       extern xmalloc_handler_t memory_error;
**       extern const char *string;
**       char *buffer;
**
**       xmalloc_error_handler = memory_error;
**       buffer = xmalloc(1024);
**       xrealloc(buffer, 2048);
**       free(buffer);
**       buffer = xstrdup(string);
**
**  xmalloc, xrealloc, and xstrdup behave exactly like their C library
**  counterparts without the leading x except that they will never return
**  NULL.  Instead, on error, they call xmalloc_error_handler, passing it
**  the name of the function whose memory allocation failed, the amount of
**  the allocation, and the file and line number where the allocation
**  function was invoked (from __FILE__ and __LINE__).  This function may do
**  whatever it wishes, such as some action to free up memory or a call to
**  sleep to hope that system resources return.  If the handler returns, the
**  interrupted memory allocation function will try its allocation again
**  (calling the handler again if it still fails).
**
**  The default error handler, if none is set by the caller, prints an error
**  message to stderr and exits with exit status 1.  An error handler must
**  take a const char * (function name), size_t (bytes allocated), const
**  char * (file), and int (line).
**
**  xmalloc will return a pointer to a valid memory region on an xmalloc of 0
**  bytes, ensuring this by allocating space for one character instead of 0
**  bytes.
**
**  The functions defined here are actually x_malloc, x_realloc, etc.  The
**  header file defines macros named xmalloc, etc. that pass the file name
**  and line number to these functions.
*/

#include "config.h"
#include "libinn.h"

#include <stdlib.h>
#include <sys/types.h>

#if STDC_HEADERS
# include <string.h>
#elif !HAVE_MEMCPY
# define memcpy(d, s, n)        bcopy((s), (d), (n))
#endif

/* The default error handler. */
static void
xmalloc_fail(const char *function, size_t size, const char *file, int line)
{
    sysdie("failed to %s %lu bytes at %s line %d", function,
           (unsigned long) size, file, line);
}

/* Assign to this variable to choose a handler other than the default. */
xmalloc_handler_t xmalloc_error_handler = xmalloc_fail;

void *
x_malloc(size_t size, const char *file, int line)
{
    void *p;
    size_t real_size;

    real_size = (size > 0) ? size : 1;
    p = malloc(real_size);
    while (p == NULL) {
        (*xmalloc_error_handler)("malloc", size, file, line);
        p = malloc(real_size);
    }
    return p;
}

void *
x_realloc(void *p, size_t size, const char *file, int line)
{
    void *newp;

    newp = realloc(p, size);
    while (newp == NULL && size > 0) {
        (*xmalloc_error_handler)("realloc", size, file, line);
        newp = realloc(p, size);
    }
    return newp;
}

char *
x_strdup(const char *s, const char *file, int line)
{
    char *p;
    size_t len;

    len = strlen(s) + 1;
    p = malloc(len);
    while (p == NULL) {
        (*xmalloc_error_handler)("strdup", len, file, line);
        p = malloc(len);
    }
    memcpy(p, s, len);
    return p;
}
