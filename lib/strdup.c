/*  $Id$
**
**  strdup implementation for systems that don't have it.
*/
#include "config.h"

#include <stdlib.h>

#ifdef STDC_HEADERS
# include <string.h>
#else
# ifdef HAVE_MEMORY_H
#  include <memory.h>
# endif
# ifdef HAVE_STRING_H
#  include <string.h>
# endif
#endif

char *
strdup(const char *s)
{
    char *p;
    size_t len;

    len = strlen(s) + 1;
    p = malloc(len);
    if (p == NULL) return NULL;
    memcpy(p, s, len);
    return p;
}
