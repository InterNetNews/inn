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

END_DECLS

#endif /* INN_MMAP_H */
