/*  $Id$
**
**  Routines for manipulating sockaddr structs
*/

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include <netdb.h>

char *sprint_sockaddr(const struct sockaddr *sa)
{
#ifdef HAVE_INET6
    static char buff[256];
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;

    *buff = '\0';
    if (sa->sa_family == AF_INET6 && IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
	struct sockaddr_in sin;
	memcpy(&sin.sin_addr, sin6->sin6_addr.s6_addr + 12,
		sizeof sin.sin_addr);
	sin.sin_port = sin6->sin6_port;
	sin.sin_family = AF_INET;
#ifdef HAVE_SOCKADDR_LEN
	sin.sin_len = sizeof(struct sockaddr_in);
#endif
	return inet_ntoa(sin.sin_addr);
    }
    getnameinfo(sa, SA_LEN(sa), buff, sizeof buff, NULL, 0, NI_NUMERICHOST);

    return buff;
#else
    return inet_ntoa(((struct sockaddr_in *)sa)->sin_addr);
#endif
}

void make_sin(struct sockaddr_in *s, const struct in_addr *src)
{
    memset(s, 0, sizeof( struct sockaddr_in ));
    s->sin_family = AF_INET;
#ifdef HAVE_SOCKADDR_LEN
    s->sin_len = sizeof( struct sockaddr_in );
#endif
    s->sin_addr = *src;
}
