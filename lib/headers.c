/*  $Id$
**
**  Routines for headers:  manipulation and checks.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>

#include "inn/libinn.h"


/*
** We currently only check the requirements for RFC 3977:
**
**   o  The name [of a header] consists of one or more printable
**      US-ASCII characters other than colon.
*/
bool
IsValidHeaderName(const char *string)
{
    const unsigned char *p;

    /* Not NULL. */
    if (string == NULL)
        return false;

    p = (const unsigned char *) string;
   
    /* Not empty. */
    if (*p == '\0')
        return false;

    for (; *p != '\0'; p++) {
        /* Contains only printable US-ASCII characters other
         * than colon. */
        if (!CTYPE(isgraph, *p) || *p == ':')
            return false;
    } 

    return true;
}
