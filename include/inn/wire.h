/*  $Id$
**
**  Wire format article utilities
**
**  Written by Alex Kiernan (alex.kiernan@thus.net)
**
**  These routines manipulate wire format articles, in particular they
**  should be safe in the presence of embedded NULs and UTF-8
**  characters.
*/

#ifndef INN_WIRE_H
#define INN_WIRE_H 1

#include <inn/defines.h>

BEGIN_DECLS

/* Given a pointer to the start of an article, locate the first octet
   of the body (which may be the octet beyond the end of the buffer if
   your article is bodyless) */
char *wire_findbody(const char *, size_t);

END_DECLS

#endif /* INN_WIRE_H */
