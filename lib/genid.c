/*  $Id$
**
**  Generate a message ID.
*/

#include "config.h"
#include "clibrary.h"
#include <time.h>

#include "inn/innconf.h"
#include "libinn.h"

/* Scale time back a bit, for shorter Message-ID's. */
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
