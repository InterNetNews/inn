/*  $Id$
**
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <sys/socket.h>

#include "inn/innconf.h"
#include "inn/libinn.h"
#include "nntp.h"
#include "inn/paths.h"

#ifdef HAVE_UNIX_DOMAIN_SOCKETS
# include <sys/un.h>
#endif


/*
**  Open a connection to the local InterNetNews NNTP server and optionally
**  create stdio FILE's for talking to it.  Return -1 on error.
*/
int
NNTPlocalopen(FILE **FromServerp, FILE **ToServerp, char *errbuff, size_t len)
{
#if	defined(HAVE_UNIX_DOMAIN_SOCKETS)
    int			i;
    int			j;
    int			oerrno;
    struct sockaddr_un	server;
    FILE		*F;
    char		mybuff[NNTP_STRLEN + 2];
    char		*buff;

    if (errbuff)
        buff = errbuff;
    else {
        buff = mybuff;
        len = sizeof(mybuff);
    }
    *buff = '\0';

    /* Create a socket. */
    if ((i = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	return -1;

    /* Connect to the server. */
    memset(&server, 0, sizeof server);
    server.sun_family = AF_UNIX;
    strlcpy(server.sun_path, innconf->pathrun, sizeof(server.sun_path));
    strlcat(server.sun_path, "/", sizeof(server.sun_path));
    strlcat(server.sun_path, INN_PATH_NNTPCONNECT, sizeof(server.sun_path));
    if (connect(i, (struct sockaddr *)&server, SUN_LEN(&server)) < 0) {
	oerrno = errno;
	close(i);
	errno = oerrno;
	return -1;
    }

    /* Connected -- now make sure we can post. */
    if ((F = fdopen(i, "r")) == NULL) {
	oerrno = errno;
	close(i);
	errno = oerrno;
	return -1;
    }
    if (fgets(buff, len, F) == NULL) {
	oerrno = errno;
	fclose(F);
	errno = oerrno;
	return -1;
    }
    j = atoi(buff);
    if (j != NNTP_OK_BANNER_POST && j != NNTP_OK_BANNER_NOPOST) {
	fclose(F);
	/* This seems like a reasonable error code to use... */
	errno = EPERM;
	return -1;
    }

    *FromServerp = F;
    if ((*ToServerp = fdopen(dup(i), "w")) == NULL) {
	oerrno = errno;
	fclose(F);
	errno = oerrno;
	return -1;
    }
    return 0;
#else
    return NNTPconnect("127.0.0.1", innconf->port, FromServerp, ToServerp,
                       errbuff, len);
#endif	/* defined(HAVE_UNIX_DOMAIN_SOCKETS) */
}
