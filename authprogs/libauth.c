/*  $Id$
**
**  Common code for authenticators and resolvers.
**
**  Collects common code to read information from nnrpd that should be done
**  the same for all authenticators, and common code to get information about
**  the incoming connection.
*/

#include "config.h"
#include "clibrary.h"
#include "libinn.h"

#include "libauth.h"

#define NAMESTR "ClientAuthname: "
#define PASSSTR "ClientPassword: "

#ifdef HAVE_INET6
# include <netdb.h>
#endif

/* Get a username and password from nnrpd, returning an allocated authinfo
   struct on success. */
struct authinfo *
get_auth(void)
{
    struct authinfo *authinfo;
    char buff[SMBUF];
    size_t length;

    /* Read input from nnrpd a line at a time, stripping \r\n. */
    authinfo = xmalloc(sizeof(struct authinfo));
    authinfo->username = NULL;
    authinfo->password = NULL;
    while (fgets(buff, sizeof(buff), stdin) != NULL) {
        length = strlen(buff);
        if (length == 0 || buff[length - 1] != '\n') {
            free(authinfo);
            return NULL;
        }
        buff[length - 1] = '\0';
        if (length > 1 && buff[length - 2] == '\r')
            buff[length - 2] = '\0';

        /* See if the line is a username or password. */
        if (strncmp(buff, NAMESTR, strlen(NAMESTR)) == 0)
            authinfo->username = xstrdup(buff + strlen(NAMESTR));
        if (strncmp(buff, PASSSTR, strlen(PASSSTR)) == 0)
            authinfo->password = xstrdup(buff + strlen(PASSSTR));
    }

    /* Any information that nnrpd didn't give us is set to an empty string. */
    if (authinfo->username == NULL)
        authinfo->username = xstrdup("");
    if (authinfo->password == NULL)
        authinfo->password = xstrdup("");

    return authinfo;
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

