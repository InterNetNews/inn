/*  
**
**  Common code for authenticators and resolvers.
**
*/

#include "libauth.h"
#include <sys/socket.h>
#ifdef HAVE_INET6
#include <netdb.h>
#endif


int
get_auth(char* uname, char* pass)
{
    char buff[SMBUF];

    uname[0] = '\0';
    pass[0] = '\0';
    /* make sure that strlen(buff) is always less than sizeof(buff) */
    buff[sizeof(buff)-1] = '\0';
    /* get the username and password from stdin */
    while (fgets(buff, sizeof(buff)-1, stdin) != (char*) 0) {
        /* strip '\r\n' */
        buff[strlen(buff)-1] = '\0';
        if (strlen(buff) && (buff[strlen(buff)-1] == '\r'))
            buff[strlen(buff)-1] = '\0';

        if (!strncmp(buff, NAMESTR, strlen(NAMESTR)))
            strcpy(uname, buff+sizeof(NAMESTR)-1);
        if (!strncmp(buff, PASSSTR, strlen(PASSSTR)))
            strcpy(pass, buff+sizeof(PASSSTR)-1);
    }
    if (uname[0] == '\0' || pass[0] == '\0' )
        return(3);

    return(0);
}



char
get_res(struct sockaddr* loc,
	struct sockaddr* cli)
{
    char result = 0;
    char *c;
    char buf[2048];
    char cip[2048], sip[2048], cport[2048], sport[2048];
#ifdef HAVE_INET6
    struct addrinfo *res, hints;
    int ret;
#else
    struct sockaddr_in *loc_sin = (struct sockaddr_in *)loc;
    struct sockaddr_in *cli_sin = (struct sockaddr_in *)cli;
#endif

    cip[0] = cport[0] = sip[0] = sport[0] = '\0';

    /* read the connection info from stdin */

    while(fgets(buf, sizeof(buf), stdin) != (char*) 0) {
	if( ( c = strchr( buf, '\n' ) ) ) *c = '\0';
	if( ( c = strchr( buf, '\r' ) ) ) *c = '\0';
	if (!strncmp(buf, IPNAME, strlen(IPNAME))) {
	    strcpy( cip, buf + strlen( IPNAME ) );
	} else if (!strncmp(buf, PORTNAME, strlen(PORTNAME))) {
	    strcpy( cport, buf + strlen( PORTNAME ) );
	} else if (!strncmp(buf, LOCIP, strlen(LOCIP))) {
	    strcpy( sip, buf + strlen( LOCIP ) );
	} else if (!strncmp(buf, LOCPORT, strlen(LOCPORT))) {
	    strcpy( sport, buf + strlen( LOCPORT ) );
	}
    }

#ifdef HAVE_INET6
    memset( &hints, 0, sizeof( hints ) );
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_socktype = SOCK_STREAM;

    hints.ai_family = strchr( cip, ':' ) != NULL ? PF_INET6 : PF_INET;
    if( *cip && ! ( ret = getaddrinfo( cip, cport, &hints, &res ) ) )
    {
	memcpy( cli, res->ai_addr, SA_LEN( res->ai_addr ) );
	result = result | GOTCLIADDR;
	result = result | GOTCLIPORT;
	freeaddrinfo( res );
    }

    hints.ai_family = strchr( sip, ':' ) != NULL ? PF_INET6 : PF_INET;
    if( *sip && ! ( ret = getaddrinfo( sip, sport, &hints, &res ) ) )
    {
	memcpy( loc, res->ai_addr, SA_LEN( res->ai_addr ) );
	result = result | GOTLOCADDR;
	result = result | GOTLOCPORT;
	freeaddrinfo( res );
    }
#else
    cli_sin->sin_family = AF_INET;
    if( *cip && ( cli_sin->sin_addr.s_addr = inet_addr( cip ) ) != -1 )
	result = result | GOTCLIADDR;
    if( *cport )
    {
	cli_sin->sin_port = htons( atoi(cport) );
	result = result | GOTCLIPORT;
    }

    loc_sin->sin_family = AF_INET;
    if( *sip && ( loc_sin->sin_addr.s_addr = inet_addr( sip ) ) != -1 )
	result = result | GOTLOCADDR;
    if( *sport )
    {
	loc_sin->sin_port = htons( atoi(sport) );
	result = result | GOTLOCPORT;
    }
#endif

    return(result);
}

