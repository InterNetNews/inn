/*  $Id$
**
**  Domain authenticator.
**
**  Compares the domain of the client connection to the first argument given
**  on the command line, and returns the host portion of the connecting host
**  as the user if it matches.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/messages.h"
#include "libinn.h"

int
main(int argc, char *argv[])
{
    char buf[2048];
    char *host, *p;
    size_t length;

    if (argc != 2)
        die("Usage: domain <domain>");
    message_program_name = "domain";

    /* Read the connection information from stdin. */
    host = NULL;
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        length = strlen(buf);
        if (buf[length - 1] != '\n')
            die("input line too long");
        buf[length - 1] = '\0';
        if (buf[length - 2] == '\r')
            buf[length - 2] = '\0';

        if (strncmp(buf, "ClientHost: ", strlen("ClientHost: ")) == 0)
            host = xstrdup(buf + strlen("ClientHost: "));
    }
    if (host == NULL)
        die("did not get ClientHost data from nnrpd");

    /* Check the host against the provided domain.  Allow the domain to be
       specified both with and without a leading period; if without, make sure
       that there is a period right before where it matches in the host. */
    p = strstr(host, argv[1]);
    if (p == host)
        die("host %s matches the domain exactly", host);
    if (p == NULL || (argv[1][0] != '.' && p != host && *(p - 1) != '.'))
        die("host %s didn't match domain %s", host, argv[1]);

    /* Peel off the portion of the host before where the provided domain
       matches and return it as the user. */
    if (argv[1][0] != '.')
        p--;
    *p = '\0';
    printf("User:%s", host);
    return 0;
}
