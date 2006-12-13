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
#include "inn/libinn.h"
#include "nntp.h"


/*
**  Open a connection to an NNTP server and create stdio FILE's for talking
**  to it.  Return -1 on error.
*/
int
NNTPconnect(const char *host, int port, FILE **FromServerp, FILE **ToServerp,
            char *errbuff, size_t len)
{
    char mybuff[NNTP_STRLEN + 2];
    char *buff;
    int fd, code, oerrno;
    FILE *F = NULL;

    if (errbuff)
        buff = errbuff;
    else {
        buff = mybuff;
        len = sizeof(mybuff);
    }
    *buff = '\0';

    fd = network_connect_host(host, port, NULL);
    if (fd < 0)
        return -1;

    /* Connected -- now make sure we can post.  If we can't, use EPERM as a
       reasonable error code. */
    F = fdopen(fd, "r");
    if (F == NULL)
        goto fail;
    if (fgets(buff, len, F) == NULL)
        goto fail;
    code = atoi(buff);
    if (code != NNTP_OK_BANNER_POST && code != NNTP_OK_BANNER_NOPOST) {
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
NNTPremoteopen(int port, FILE **FromServerp, FILE **ToServerp, char *errbuff,
               size_t len)
{
    char		*p;

    if ((p = innconf->server) == NULL) {
	if (errbuff)
	    strlcpy(errbuff, "What server?", len);
	return -1;
    }
    return NNTPconnect(p, port, FromServerp, ToServerp, errbuff, len);
}
