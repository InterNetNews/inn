/*
**  Discover the fully-qualified domain name of the local host.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"

#include "inn/libinn.h"
#include "inn/xmalloc.h"


/*
**  Return the fully-qualified domain name of the local system in
**  newly-allocated memory, or NULL if it cannot be discovered.  The caller is
**  responsible for freeing.  If the host's domain cannot be found in DNS, use
**  the domain argument as a fallback.
*/
char *
inn_getfqdn(const char *domain)
{
    char hostname[BUFSIZ];
    struct addrinfo hints, *res;
    char *fqdn, *canon;

    /* If gethostname fails, there's nothing we can do. */
    if (gethostname(hostname, sizeof(hostname)) < 0)
        return NULL;

    /* If the local hostname is already fully qualified, just return it. */
    if (strchr(hostname, '.') != NULL)
        return xstrdup(hostname);

    /* Attempt to canonicalize with DNS. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_CANONNAME;
    if (getaddrinfo(hostname, NULL, &hints, &res) == 0) {
        canon = res->ai_canonname;
        if (canon != NULL && strchr(canon, '.') != NULL) {
            fqdn = xstrdup(canon);
            freeaddrinfo(res);
            return fqdn;
        }
        freeaddrinfo(res);
    }

    /* Fall back on canonicalizing with a provided domain. */
    if (domain == NULL || domain[0] == '\0')
        return NULL;
    xasprintf(&fqdn, "%s.%s", hostname, domain);
    return fqdn;
}
