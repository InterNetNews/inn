/*
 * Concatenate strings with dynamic memory allocation.
 *
 * Usage:
 *
 *      string = concat(string1, string2, ..., (char *) 0);
 *      path = concatpath(base, name);
 *
 * Dynamically allocates (using xmalloc) sufficient memory to hold all of the
 * strings given and then concatenates them together into that allocated
 * memory, returning a pointer to it.  Caller is responsible for freeing.
 * Assumes xmalloc is available.  The last argument must be a null pointer (to
 * a char *, if you actually find a platform where it matters).
 *
 * concatpath is similar, except that it only takes two arguments.  If the
 * second argument begins with / or ./, a copy of it is returned; otherwise,
 * the first argument, a slash, and the second argument are concatenated
 * together and returned.  This is useful for building file names where names
 * that aren't fully qualified are qualified with some particular directory.
 *
 * The canonical version of this file *used to be* maintained in the
 * rra-c-util package, which can be found at
 * <http://www.eyrie.org/~eagle/software/rra-c-util/>.
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

#include "inn/concat.h"
#include "inn/xmalloc.h"
#include "inn/messages.h"

/* Abbreviation for cleaner code. */
#define VA_NEXT(var, type) ((var) = (type) va_arg(args, type))


/*
 * Concatenate all of the arguments into a newly allocated string.  ANSI C
 * requires at least one named parameter, but it's not treated any different
 * than the rest.
 */
char *
concat(const char *first, ...)
{
    va_list args;
    char *result, *p;
    const char *string;
    size_t length = 0;

    /* Find the total memory required. */
    va_start(args, first);
    for (string = first; string != NULL; VA_NEXT(string, const char *)) {
        /*
         * Prevent overflow.  We limit the size to INT_MAX so that
         * downstream conversions to int don't generate a vulnerability.
         */
        if(length >= INT_MAX - strlen(string))
            sysdie("concat input too long");
        length += strlen(string);
    }
    va_end(args);
    /* We will need a 0 terminator as well. */
    length++;

    /*
     * Create the string.  Doing the copy ourselves avoids useless string
     * traversals of result, if using strcat, or string, if using strlen to
     * increment a pointer into result, at the cost of losing the native
     * optimization of strcat if any.
     */
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


/*
 * Concatenate name with base, unless name begins with / or ./.  Return the
 * new string in newly allocated memory.
 */
char *
concatpath(const char *base, const char *name)
{
    if (name[0] == '/' || (name[0] == '.' && name[1] == '/'))
        return xstrdup(name);
    else
        return concat(base != NULL ? base : ".", "/", name, (char *) 0);
}
