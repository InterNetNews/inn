/*  $Id$
**
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <sys/socket.h>

#include "inn/innconf.h"
#include "libinn.h"
#include "nntp.h"
#include "paths.h"

#if HAVE_UNIX_DOMAIN_SOCKETS
# include <sys/un.h>
#endif


/*
**  Open a connection to the local InterNetNews NNTP server and optionally
**  create stdio FILE's for talking to it.  Return -1 on error.
*/
int
NNTPlocalopen(FILE **FromServerp, FILE **ToServerp, char *errbuff)
{
#if	defined(HAVE_UNIX_DOMAIN_SOCKETS)
    int			i;
    int			j;
    int			oerrno;
    struct sockaddr_un	server;
    FILE		*F;
    char		mybuff[NNTP_STRLEN + 2];
    char		*buff;

    buff = errbuff ? errbuff : mybuff;
    *buff = '\0';

    /* Create a socket. */
    if ((i = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	return -1;

    /* Connect to the server. */
    memset(&server, 0, sizeof server);
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, innconf->pathrun);
    strcat(server.sun_path, "/");
    strcat(server.sun_path, _PATH_NNTPCONNECT);
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
    if (fgets(buff, sizeof mybuff, F) == NULL) {
	oerrno = errno;
	fclose(F);
	errno = oerrno;
	return -1;
    }
    j = atoi(buff);
    if (j != NNTP_POSTOK_VAL && j != NNTP_NOPOSTOK_VAL) {
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
                       errbuff);
#endif	/* defined(HAVE_UNIX_DOMAIN_SOCKETS) */
}
