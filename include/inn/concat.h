/*
 * Prototypes for string concatenation with dynamic memory allocation.
 *
 * The canonical version of this file *used to be* maintained in the
 * rra-c-util package, which can be found at
 * <https://www.eyrie.org/~eagle/software/rra-c-util/>.
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

#ifndef INN_CONCAT_H
#define INN_CONCAT_H 1

#include <inn/defines.h>
#include "inn/portable-macros.h"

BEGIN_DECLS

/* Concatenate NULL-terminated strings into a newly allocated string. */
char *concat(const char *first, ...)
    __attribute__((__malloc__, __nonnull__(1)));

/*
 * Given a base path and a file name, create a newly allocated path string.
 * The name will be appended to base with a / between them.  Exceptionally, if
 * name begins with a slash, it will be strdup'd and returned as-is.
 */
char *concatpath(const char *base, const char *name)
    __attribute__((__malloc__, __nonnull__(2)));

END_DECLS

#endif /* INN_CONCAT_H */
