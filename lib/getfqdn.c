/*  $Id$
**
*/

#include "config.h"
#include "clibrary.h"
#include <netdb.h>

#include "inn/libinn.h"
#include "inn/paths.h"


/*
**  Get the fully-qualified domain name for this host.
*/
char *GetFQDN(char *domain)
{
    static char		buff[SMBUF];
    struct hostent	*hp;
    char		*p;
    char		**ap;
#if	0
    /* See comments below. */
    char		temp[SMBUF + 2];
#endif	/* 0 */

    /* Return any old results. */
    if (buff[0])
	return buff;

    /* Try gethostname. */
    if (gethostname(buff, (int)sizeof buff) < 0)
	return NULL;
    if (strchr(buff, '.') != NULL)
	return buff;

    /* See if DNS (or /etc/hosts) gives us a full domain name. */
    if ((hp = gethostbyname(buff)) == NULL)
	return NULL;
#if	0
    /* This code is a "feature" that allows multiple domains (NIS or
     * DNS, I'm not sure) to work with a single INN server.  However,
     * it turns out to cause more problems for people, and they have to
     * use hacks like __switch_gethostbyname, etc.  So if you need this,
     * turn it on, but don't complain to me. */
    if (strchr(hp->h_name, '.') == NULL) {
	/* Try to force DNS lookup if NIS/whatever gets in the way. */
        strlcpy(temp, buff, sizeof(temp));
        strlcat(temp, ".", sizeof(temp));
	hp = gethostbyname(temp);
    }
#endif	/* 0 */

    /* First, see if the main name is a FQDN.  It should be. */
    if (hp != NULL && strchr(hp->h_name, '.') != NULL) {
	if (strlen(hp->h_name) < sizeof buff - 1) {
	    strlcpy(buff, hp->h_name, sizeof(buff));
            return buff;
        }
	/* Doesn't fit; make sure we don't return bad data next time. */
	buff[0] = '\0';
	return hp->h_name;
    }

    /* Second, see if any aliases are. */
    if ((ap = hp->h_aliases) != NULL)
	while ((p = *ap++) != NULL)
	    if (strchr(p, '.') != NULL) {
		/* Deja-vous all over again. */
		if (strlen(p) < sizeof buff - 1) {
		    strlcpy(buff, p, sizeof(buff));
                    return buff;
                }
		buff[0] = '\0';
		return p ;
	    }

    /* Give up:  Get the domain config param and append it. */
    if ((p = domain) == NULL || *p == '\0')
	return NULL;
    if (strlen(buff) + 1 + strlen(p) > sizeof buff - 1)
	/* Doesn't fit. */
	return NULL;
    strlcat(buff, ".", sizeof(buff));
    strlcat(buff, p, sizeof(buff));
    return buff;
}
