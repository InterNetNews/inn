/* $Id$
 *
 * Some standard helpful macros.
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

#ifndef INN_MACROS_H
#define INN_MACROS_H 1

#include "inn/portable-macros.h"

/*
 * Used for iterating through arrays.  ARRAY_SIZE returns the number of
 * elements in the array (useful for a < upper bound in a for loop) and
 * ARRAY_END returns a pointer to the element past the end (ISO C99 makes it
 * legal to refer to such a pointer as long as it's never dereferenced).
 */
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define ARRAY_END(array)  (&(array)[ARRAY_SIZE(array)])

/* Used to name the elements of the array passed to pipe. */
#define PIPE_READ  0
#define PIPE_WRITE 1

/* Used for unused parameters to silence gcc warnings. */
#define UNUSED __attribute__((__unused__))

#endif /* INN_MACROS_H */
