/*  $Revision$
**
**  Open a connection to a remote NNTP server.
*/
#include "config.h"
#include "clibrary.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#ifdef HAVE_UNIX_DOMAIN_SOCKETS
# include <sys/un.h>
#endif

#include "libinn.h"
#include "nntp.h"
#include "paths.h"

/*
**  Open a connection to an NNTP server and create stdio FILE's for talking
**  to it.  Return -1 on error.
*/
int NNTPconnect(char *host, int port, FILE **FromServerp, FILE **ToServerp, char *errbuff)
{
    char		mybuff[NNTP_STRLEN + 2];
    char		*buff;
    int	                i;
    int 	        j;
    int			oerrno;
    FILE		*F;
#ifdef HAVE_INET6
    struct addrinfo	hints, *ressave, *addr;
    char		portbuf[16];
    struct sockaddr_storage client;
#else
    char		**ap;
    char	        *dest;
    char		*fakelist[2];
    char                *p;
    struct hostent	*hp;
    struct hostent	fakehp;
    struct in_addr	quadaddr;
    struct sockaddr_in	server, client;
#endif

    buff = errbuff ? errbuff : mybuff;
    *buff = '\0';

#ifdef HAVE_INET6
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    sprintf(portbuf, "%d", port);
    if (getaddrinfo(host, portbuf, &hints, &addr) != 0)
	return -1;

    for (ressave = addr; addr; addr = addr->ai_next) {
	if ((i = socket(addr->ai_family, addr->ai_socktype,
			addr->ai_protocol)) < 0)
	    continue; /* ignore */
	/* bind the local (source) address, if requested */
	memset(&client, 0, sizeof client);
	if (addr->ai_family == AF_INET && innconf->sourceaddress) {
	    if (inet_pton(AF_INET, innconf->sourceaddress,
			&((struct sockaddr_in *)&client)->sin_addr) < 1) {
		addr = NULL;
		break;
	    }
	    ((struct sockaddr_in *)&client)->sin_family = AF_INET;
#ifdef HAVE_SOCKADDR_LEN
	    ((struct sockaddr_in *)&client)->sin_len = sizeof( struct sockaddr_in );
#endif
	}
	if (addr->ai_family == AF_INET6 && innconf->sourceaddress6) {
	    if (inet_pton(AF_INET6, innconf->sourceaddress6,
			&((struct sockaddr_in6 *)&client)->sin6_addr) < 1) {
		addr = NULL;
		break;
	    }
	    ((struct sockaddr_in6 *)&client)->sin6_family = AF_INET6;
#ifdef HAVE_SOCKADDR_LEN
	    ((struct sockaddr_in6 *)&client)->sin6_len = sizeof( struct sockaddr_in6 );
#endif
	}
	if (client.ss_family != 0) {
	    if (bind(i, (struct sockaddr *)&client, addr->ai_addrlen) < 0) {
		addr = NULL;
		break;
	    }
	}
	/* we are ready, try to connect */
	if (connect(i, addr->ai_addr, addr->ai_addrlen) == 0)
	    break; /* success */
	oerrno = errno;
	close(i);
	errno = oerrno;
    }
    freeaddrinfo(ressave);

    if (addr == NULL) {
	/* all connect(2) calls failed or some other error has occurred */
	oerrno = errno;
	close(i);
	errno = oerrno;
	return -1;
    }
    {
#else /* HAVE_INET6 */
    if (inet_aton(host, &quadaddr)) {
	/* Host was specified as a dotted-quad internet address.  Fill in
	 * the parts of the hostent struct that we need. */
	fakehp.h_length = sizeof quadaddr;
	fakehp.h_addrtype = AF_INET;
	hp = &fakehp;
	fakelist[0] = (char *)&quadaddr;
	fakelist[1] = NULL;
	ap = fakelist;
    }
    else if ((hp = gethostbyname(host)) != NULL) {
	/* Symbolic host name. */
#if	defined(h_addr)
	ap = hp->h_addr_list;
#else
	/* Fake up an address list for old systems. */
	fakelist[0] = (char *)hp->h_addr;
	fakelist[1] = NULL;
	ap = fakelist;
#endif	/* defined(h_addr) */
    }
    else
	/* Not a host name. */
	return -1;

    /* Set up the socket address. */
    memset(&server, 0, sizeof server);
    server.sin_family = hp->h_addrtype;
#ifdef HAVE_SOCKADDR_LEN
    server.sin_len = sizeof( struct sockaddr_in );
#endif
    server.sin_port = htons(port);

    /* Source IP address to which we bind. */
    memset(&client, 0, sizeof client);
    client.sin_family = AF_INET;
#ifdef HAVE_SOCKADDR_LEN
    client.sin_len = sizeof( struct sockaddr_in );
#endif
    if (innconf->sourceaddress) {
        if (!inet_aton(innconf->sourceaddress, &client.sin_addr))
	    return -1;
    } else
	client.sin_addr.s_addr = htonl(INADDR_ANY);
  
    /* Loop through the address list, trying to connect. */
    for (; ap && *ap; ap++) {
	/* Make a socket and try to connect. */
	if ((i = socket(hp->h_addrtype, SOCK_STREAM, 0)) < 0)
	    break;
	/* Bind to the source address we want. */
	if (bind(i, (struct sockaddr *)&client, sizeof client) < 0) {
	    oerrno = errno;
	    (void)close(i);
	    errno = oerrno;
	    continue;
	}
	/* Copy the address via inline memcpy:
	 *	memcpy(&server.sin_addr, *ap, hp->h_length); */
	p = (char *)*ap;
	for (dest = (char *)&server.sin_addr, j = hp->h_length; --j >= 0; )
	    *dest++ = *p++;
	if (connect(i, (struct sockaddr *)&server, sizeof server) < 0) {
	    oerrno = errno;
	    (void)close(i);
	    errno = oerrno;
	    continue;
	}
#endif /* HAVE_INET6 */

	/* Connected -- now make sure we can post. */
	if ((F = fdopen(i, "r")) == NULL) {
	    oerrno = errno;
	    (void)close(i);
	    errno = oerrno;
	    return -1;
	}
	if (fgets(buff, sizeof mybuff, F) == NULL) {
	    oerrno = errno;
	    (void)fclose(F);
	    errno = oerrno;
	    return -1;
	}
	j = atoi(buff);
	if (j != NNTP_POSTOK_VAL && j != NNTP_NOPOSTOK_VAL) {
	    (void)fclose(F);
	    /* This seems like a reasonable error code to use... */
	    errno = EPERM;
	    return -1;
	}

	*FromServerp = F;
	if ((*ToServerp = fdopen(dup(i), "w")) == NULL) {
	    oerrno = errno;
	    (void)fclose(F);
	    errno = oerrno;
	    return -1;
	}
	return 0;
    }

    return -1;
}



#if	defined(REM_INND)

int NNTPremoteopen(int port, FILE **FromServerp, FILE **ToServerp, char *errbuff)
{
    char		*p;

    if ((p = innconf->server) == NULL) {
	if (errbuff)
	    (void)strcpy(errbuff, "What server?");
	return -1;
    }
    return NNTPconnect(p, port, FromServerp, ToServerp, errbuff);
}

#endif	/* defined(REM_INND) */



#if	defined(REM_NNTP)


/*
**  Open a connection to an NNTP server using the "clientlib" routines in
**  the NNTP distribution.  We also create stdio FILE's for talking over
**  the connection (which is easy since clientlib has them as globals.)
**  Return -1 on error.
*/
int NNTPremoteopen(int port, FromServerp, ToServerp, buff)
    FILE		**FromServerp;
    FILE		**ToServerp;
    char		*buff;
{
    extern FILE		*ser_rd_fp;
    extern FILE		*ser_wr_fp;
    extern char		*getserverbyfile();
    char		*p;
    int			i;
    static char		*pathserver = NULL;

    if (buff)
	(void)strcpy(buff, "Text unavailable");
    if (pathserver == NULL)
	pathserver = concatpath(innconf->pathetc, _PATH_SERVER);
    if ((p = getserverbyfile(pathserver)) == NULL)
	return -1;
    if ((i = server_init(p, port)) < 0)
 	return -1;
    if (i != NNTP_POSTOK_VAL && i != NNTP_NOPOSTOK_VAL) {
	errno = EPERM;
	return -1;
    }
    if (ser_rd_fp == NULL || ser_wr_fp == NULL)
	return -1;

    *FromServerp = ser_rd_fp;
    *ToServerp = ser_wr_fp;
    return 0;
}
#endif	/* defined(REM_NNTP) */
