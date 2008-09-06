/*  $Id$
**
**  Routines for message-IDs:  generation and checks.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <time.h>

#include "inn/innconf.h"
#include "inn/libinn.h"

/*  Scale time back a bit, for shorter message-ID's. */
#define OFFSET	673416000L

char *
GenerateMessageID(char *domain)
{
    static char		buff[SMBUF];
    static int		count;
    char		*p;
    char		sec32[10];
    char		pid32[10];
    time_t		now;

    now = time(NULL);
    Radix32(now - OFFSET, sec32);
    Radix32(getpid(), pid32);
    if ((domain != NULL && innconf->domain == NULL) ||
	(domain != NULL && innconf->domain != NULL
         && strcmp(domain, innconf->domain) != 0)) {
	p = domain;
    } else {
	if ((p = GetFQDN(domain)) == NULL)
	    return NULL;
    }
    snprintf(buff, sizeof(buff), "<%s$%s$%d@%s>", sec32, pid32, ++count, p);
    return buff;
}


/*
**  We currently only check the requirements for RFC 3977:
**
**    o  A message-ID MUST begin with "<", end with ">", and MUST NOT
**       contain the latter except at the end.
**
**    o  A message-ID MUST be between 3 and 250 octets in length.
**
**    o  A message-ID MUST NOT contain octets other than printable US-ASCII
**       characters.
*/
bool
IsValidMessageID(const char *string)
{
    int len = 0;
    const unsigned char *p;

    /* Not NULL. */
    if (string == NULL)
        return false;

    p = (const unsigned char *) string;

    /* Begins with "<". */
    if (p[0] != '<')
        return false;

    for (; *p != '\0'; p++) {
        len++;
        /* Contains ">" *only* at the end. */
        if (*p == '>') {
            p++;
            if (*p != '\0')
                return false;
            else
                break;
        }
        /* Contains only printable US-ASCII characters. */
        if (!CTYPE(isgraph, *p))
            return false;
    }

    /* Between 3 and 250 octets in length.
     * Ends with ">". */
    if (len < 3 || len > 250 || p[-1] != '>')
        return false;
    else
        return true;
}
