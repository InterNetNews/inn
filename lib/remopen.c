/*  $Id$
**
**  Open a connection to a remote NNTP server.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include <errno.h>

#include "inn/innconf.h"
#include "inn/network.h"
#include "libinn.h"
#include "nntp.h"


/*
**  Open a connection to an NNTP server and create stdio FILE's for talking
**  to it.  Return -1 on error.
*/
int
NNTPconnect(const char *host, int port, FILE **FromServerp, FILE **ToServerp,
            char *errbuff)
{
    char mybuff[NNTP_STRLEN + 2];
    char *buff;
    int fd, code, oerrno;
    FILE *F = NULL;
    struct addrinfo hints, *ai;
    char portbuf[16];

    buff = errbuff ? errbuff : mybuff;
    *buff = '\0';

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = NETWORK_AF_HINT;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portbuf, sizeof(portbuf), "%d", port);
    if (getaddrinfo(host, portbuf, &hints, &ai) != 0)
        return -1;
    fd = network_connect(ai);
    oerrno = errno;
    freeaddrinfo(ai);
    errno = oerrno;
    if (fd < 0)
        return -1;

    /* Connected -- now make sure we can post.  If we can't, use EPERM as a
       reasonable error code. */
    F = fdopen(fd, "r");
    if (F == NULL)
        goto fail;
    if (fgets(buff, sizeof mybuff, F) == NULL)
        goto fail;
    code = atoi(buff);
    if (code != NNTP_POSTOK_VAL && code != NNTP_NOPOSTOK_VAL) {
        errno = EPERM;
        goto fail;
    }
    *FromServerp = F;
    *ToServerp = fdopen(dup(fd), "w");
    if (*ToServerp == NULL)
        goto fail;
    return 0;

 fail:
    oerrno = errno;
    if (F != NULL)
        fclose(F);
    else
        close(fd);
    errno = oerrno;
    return -1;
}


int
NNTPremoteopen(int port, FILE **FromServerp, FILE **ToServerp, char *errbuff)
{
    char		*p;

    if ((p = innconf->server) == NULL) {
	if (errbuff)
	    strcpy(errbuff, "What server?");
	return -1;
    }
    return NNTPconnect(p, port, FromServerp, ToServerp, errbuff);
}
