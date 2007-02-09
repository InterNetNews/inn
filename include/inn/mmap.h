/*  $Id$
**
**  MMap manipulation routines
**
**  Written by Alex Kiernan (alex.kiernan@thus.net)
**
**  These routines work with mmap()ed memory
*/

#ifndef INN_MMAP_H
#define INN_MMAP_H 1

#include <inn/defines.h>

BEGIN_DECLS

/* Figure out what page an address is in and flush those pages.  This is the
   internal function, which we wrap with a define below. */
void inn__mapcntl(void *, size_t, int);

/* Some platforms only support two arguments to msync.  On those platforms,
   make the third argument to mapcntl always be zero, getting rid of whatever
   the caller tried to pass.  This avoids undefined symbols for MS_ASYNC and
   friends on platforms with two-argument msync functions. */
#ifdef INN_HAVE_MSYNC_3_ARG
# define inn_mapcntl inn__mapcntl
#else
# define inn_mapcntl(p, l, f) inn__mapcntl((p), (l), 0)
#endif

END_DECLS

#endif /* INN_MMAP_H */
