/*  $Id$
**
**  Concatenate strings with dynamic memory allocation.
**
**  Usage:
**
**      string = concat(string1, string2, ..., (char *) 0);
**
**  Dynamically allocates (using xmalloc()) sufficient memory to hold all of
**  the strings given and then concatenates them together into that
**  allocated memory, returning a pointer to it.  Caller is responsible for
**  freeing.  Assumes xmalloc() is available.  The last argument must be a
**  null pointer (to a char *, if you actually find a platform where it
**  matters).
**
**  Written by Russ Allbery <rra@stanford.edu>
**  Copyright abandoned 1999 by author.  This work is in the public domain.
*/

#include "config.h"
#include "libinn.h"

#ifdef STDC_HEADERS
# include <string.h>
#endif

/* If we're testing, use our own xmalloc. */
#ifdef TEST
  void *myxmalloc (size_t length) { return malloc(length); }
# define xmalloc myxmalloc
#endif

/* varargs implementation based on Solaris 2.6 man page. */
#if defined(STDC_HEADERS) || defined(HAVE_STDARG_H)
# include <stdarg.h>
# define VA_PARAM(type, param)  (type param, ...)
# define VA_START(param)        (va_start(args, param))
#else
# ifdef HAVE_VARARGS_H
#  include <varargs.h>
#  define VA_PARAM(type, param) (param, va_alist) type param; va_dcl
#  define VA_START(param)       (va_start(args))
# else
#  error "No variadic argument mechanism available."
# endif
#endif

/* These are the same between stdargs and varargs. */
#define VA_DECL                 va_list args
#define VA_NEXT(var, type)      ((var) = (type) va_arg(args, type))
#define VA_END                  va_end(args)

/* ANSI C requires at least one named parameter. */
void *
concat VA_PARAM(const char *, first)
{
    VA_DECL;
    char *result, *p;
    const char *string;
    size_t length = 0;

    /* Find the total memory required. */
    VA_START(first);
    for (string = first; string != 0; VA_NEXT(string, const char *))
        length += strlen(string);
    VA_END;
    length++;

    /* Create the string.  Doing the copy ourselves avoids useless string
       traversals of result, if using strcat(), or string, if using strlen
       to increment a pointer into result, at the cost of losing the
       native optimization of strcat() if any. */
    result = (char *) xmalloc(length);
    p = result;
    VA_START(first);
    for (string = first; string != 0; VA_NEXT(string, const char *))
        while (*string != '\0')
            *p++ = *string++;
    VA_END;
    *p = '\0';

    return result;
}


/* Some simple testing code to create memory leaks. */
#ifdef TEST

#include <stdio.h>

int
main ()
{
#ifdef VAR_STDARGS
    printf("Using stdargs:\n\n");
#else
    printf("Using varargs:\n\n");
#endif
    printf("a\t%s\n",     concat("a",                          (char *) 0));
    printf("ab\t%s\n",    concat("a", "b",                     (char *) 0));
    printf("ab\t%s\n",    concat("ab", "",                     (char *) 0));
    printf("ab\t%s\n",    concat("", "ab",                     (char *) 0));
    printf("\t%s\n",      concat("",                           (char *) 0));
    printf("abcde\t%s\n", concat("ab", "c", "", "de",          (char *) 0));
    printf("abcde\t%s\n", concat("abc", "de", (char *) 0, "f", (char *) 0));
    return 0;
}

#endif /* TEST */
