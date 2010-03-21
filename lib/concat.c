/*  $Id$
**
**  Concatenate strings with dynamic memory allocation.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  This work is hereby placed in the public domain by its author.
**
**  Usage:
** 
**       string = concat(string1, string2, ..., (char *) 0);
**       path = concatpath(base, name);
**
**  Dynamically allocates (using xmalloc) sufficient memory to hold all of
**  the strings given and then concatenates them together into that
**  allocated memory, returning a pointer to it.  Caller is responsible for
**  freeing.  Assumes xmalloc is available.  The last argument must be a
**  null pointer (to a char *, if you actually find a platform where it
**  matters).
**
**  concatpath is similar, except that it only takes two arguments.  If the
**  second argument begins with / or ./, a copy of it is returned;
**  otherwise, the first argument, a slash, and the second argument are
**  concatenated together and returned.  This is useful for building file
**  names where names that aren't fully qualified are qualified with some
**  particular directory.
*/

#include "config.h"
#include "inn/libinn.h"

#include <stdarg.h>
#include <string.h>

/* Abbreviation for cleaner code. */
#define VA_NEXT(var, type)      ((var) = (type) va_arg(args, type))

/* ANSI C requires at least one named parameter. */
char *
concat(const char *first, ...)
{
    va_list args;
    char *result, *p;
    const char *string;
    size_t length = 0;

    /* Find the total memory required. */
    va_start(args, first);
    for (string = first; string != NULL; VA_NEXT(string, const char *))
        length += strlen(string);
    va_end(args);
    length++;

    /* Create the string.  Doing the copy ourselves avoids useless string
       traversals of result, if using strcat, or string, if using strlen to
       increment a pointer into result, at the cost of losing the native
       optimization of strcat if any. */
    result = xmalloc(length);
    p = result;
    va_start(args, first);
    for (string = first; string != NULL; VA_NEXT(string, const char *))
        while (*string != '\0')
            *p++ = *string++;
    va_end(args);
    *p = '\0';

    return result;
}


char *
concatpath(const char *base, const char *name)
{
    if (name[0] == '/' || (name[0] == '.' && name[1] == '/'))
        return xstrdup(name);
    else
        return concat(base != NULL ? base : ".", "/", name, (char *) 0);
}
