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

/* Figure out what page an address is in and flush those pages
 */
void mapcntl(void *, size_t, int);

/* Some platforms only support two arguments to msync.  On those platforms,
   make the third argument to mapcntl always be zero, getting rid of whatever
   the caller tried to pass.  This avoids undefined symbols for MS_ASYNC and
   friends on platforms with two-argument msync functions. */
#ifndef INN_HAVE_MSYNC_3_ARG
# define mapcntl(p, l, f) mapcntl((p), (l), 0)
#endif

END_DECLS

#endif /* INN_MMAP_H */
