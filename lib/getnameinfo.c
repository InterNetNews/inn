/* $Id$
 *
 * Replacement for a missing getnameinfo.
 *
 * This is an implementation of getnameinfo for systems that don't have one so
 * that networking code can use a consistant interface without #ifdef.  It is
 * a fairly minimal implementation, with the following limitations:
 *
 *   - IPv4 support only.  IPv6 is not supported.
 *   - NI_NOFQDN is ignored.
 *   - Not thread-safe due to gethostbyaddr, getservbyport, and inet_ntoa.
 *
 * The last two issues could probably be easily remedied, but haven't been
 * needed so far.  Adding IPv6 support isn't worth it; systems with IPv6
 * support should already support getnameinfo natively.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 *
 * The authors hereby relinquish any claim to any copyright that they may have
 * in this work, whether granted under contract or by operation of law or
 * international treaty, and hereby commit to the public, at large, that they
 * shall not, at any time in the future, seek to enforce any copyright in this
 * work against any person or entity, or prevent any person or entity from
 * copying, publishing, distributing or creating derivative works of this
 * work.
 */

#include "config.h"
#include "portable/macros.h"
#include "portable/socket.h"
#include "clibrary.h"

#include <errno.h>

/*
 * If we're running the test suite, rename inet_ntoa to avoid conflicts with
 * the system version.  Note that we don't rename the structures and
 * constants, but that should be okay (except possibly for gai_strerror).
 */
#if TESTING
# undef getnameinfo
# define getnameinfo test_getnameinfo
int test_getnameinfo(const struct sockaddr *, socklen_t, char *, socklen_t,
                     char *, socklen_t, int);

/* Linux doesn't provide EAI_OVERFLOW, so make up our own for testing. */
# ifndef EAI_OVERFLOW
#  define EAI_OVERFLOW 10
# endif
#endif

/* Used for unused parameters to silence gcc warnings. */
#define UNUSED  __attribute__((__unused__))


/*
 * Check to see if a name is fully qualified by seeing if it contains a
 * period.  If it does, try to copy it into the provided node buffer and set
 * status accordingly, returning true.  If not, return false.
 */
static bool
try_name(const char *name, char *node, socklen_t nodelen, int *status)
{
    size_t namelen;

    if (strchr(name, '.') == NULL)
        return false;
    namelen = strlen(name);
    if (namelen + 1 > (size_t) nodelen)
        *status = EAI_OVERFLOW;
    else {
        memcpy(node, name, namelen + 1);
        *status = 0;
    }
    return true;
}


/*
 * Look up an address (or convert it to ASCII form) and put it in the provided
 * buffer, depending on what is requested by flags.
 */
static int
lookup_name(const struct in_addr *addr, char *node, socklen_t nodelen,
            int flags)
{
    struct hostent *host;
    char **alias;
    int status;
    char *name;
    size_t namelen;

    /* Do the name lookup first unless told not to. */
    if (!(flags & NI_NUMERICHOST)) {
        host = gethostbyaddr((const void *) addr, sizeof(struct in_addr),
                             AF_INET);
        if (host == NULL) {
            if (flags & NI_NAMEREQD)
                return EAI_NONAME;
        } else {
            if (try_name(host->h_name, node, nodelen, &status))
                return status;
            for (alias = host->h_aliases; *alias != NULL; alias++)
                if (try_name(*alias, node, nodelen, &status))
                    return status;
        }

        /*
         * We found some results, but none of them were fully-qualified, so
         * act as if we found nothing and either fail or fall through.
         */
        if (flags & NI_NAMEREQD)
            return EAI_NONAME;
    }

    /* Just convert the address to ASCII. */
    name = inet_ntoa(*addr);
    namelen = strlen(name);
    if (namelen + 1 > (size_t) nodelen)
        return EAI_OVERFLOW;
    memcpy(node, name, namelen + 1);
    return 0;
}


/*
 * Look up a service (or convert it to ASCII form) and put it in the provided
 * buffer, depending on what is requested by flags.
 */
static int
lookup_service(unsigned short port, char *service, socklen_t servicelen,
               int flags)
{
    struct servent *srv;
    const char *protocol;
    int status;
    size_t namelen;

    /* Do the name lookup first unless told not to. */
    if (!(flags & NI_NUMERICSERV)) {
        protocol = (flags & NI_DGRAM) ? "udp" : "tcp";
        srv = getservbyport(htons(port), protocol);
        if (srv != NULL) {
            namelen = strlen(srv->s_name);
            if (namelen + 1 > (size_t) servicelen)
                return EAI_OVERFLOW;
            memcpy(service, srv->s_name, namelen + 1);
            return 0;
        }
    }

    /* Just convert the port number to ASCII. */
    status = snprintf(service, servicelen, "%hu", port);
    if (status < 0 || (socklen_t) status > servicelen)
        return EAI_OVERFLOW;
    return 0;
}


/*
 * The getnameinfo implementation.
 */
int
getnameinfo(const struct sockaddr *sa, socklen_t salen UNUSED, char *node,
            socklen_t nodelen, char *service, socklen_t servicelen, int flags)
{
    const struct sockaddr_in *sin;
    int status;
    unsigned short port;

    if ((node == NULL || nodelen <= 0) && (service == NULL || servicelen <= 0))
        return EAI_NONAME;

    /* We only support AF_INET. */
    if (sa->sa_family != AF_INET)
        return EAI_FAMILY;
    sin = (const struct sockaddr_in *) (const void *) sa;

    /* Name lookup. */
    if (node != NULL && nodelen > 0) {
        status = lookup_name(&sin->sin_addr, node, nodelen, flags);
        if (status != 0)
            return status;
    }

    /* Service lookup. */
    if (service != NULL && servicelen > 0) {
        port = ntohs(sin->sin_port);
        return lookup_service(port, service, servicelen, flags);
    } else
        return 0;
}
