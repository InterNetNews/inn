/*  $Id$
**
**  Wire format article utilities
**
**  Written by Alex Kiernan (alex.kiernan@thus.net)
**
**  These routines manipulate wire format articles, in particular they
**  should be safe in the presence of embedded NULs
** 
*/

#include "config.h"
#include "clibrary.h"
#include <assert.h>

#include "inn/wire.h"

/*
**  Given a pointer to the start of an article, locate the first octet
**  of the body (which may be the octet beyond the end of the buffer
**  if your article is bodiless)
*/
char *
wire_findbody(const char *article,
	      size_t len)
{
    char *p;
    const char *end;

    end = article + len;
    assert(article <= end);
    for(p = (char *)article; (p + 4) <= end; ++p) {
	p = memchr(p, '\r', end - p);
	if (p == NULL)
	    break;
	if ((p + 4 <= end) && memcmp(p, "\r\n\r\n", 4) == 0) {
	    p += 4;
	    assert(p <= end);
	    return p;
	}
    }
    return NULL;
}
