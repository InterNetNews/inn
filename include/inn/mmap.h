/*  $Id$
**
**  Manipulation routines for memory-mapped pages.
**
**  Written by Alex Kiernan (alex.kiernan@thus.net)
*/

#ifndef INN_MMAP_H
#define INN_MMAP_H 1

#include <inn/defines.h>

BEGIN_DECLS

/* msync the page containing a section of memory. */
void msync_page(void *, size_t, int flags);

/* Some platforms only support two arguments to msync.  On those platforms,
   make the third argument to msync_page always be zero, getting rid of
   whatever the caller tried to pass.  This avoids undefined symbols for
   MS_ASYNC and friends on platforms with two-argument msync functions. */
#ifndef INN_HAVE_MSYNC_3_ARG
# define msync_page(p, l, f) msync_page((p), (l), 0)
#endif

END_DECLS

#endif /* INN_MMAP_H */
