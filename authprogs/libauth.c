/*  
**
**  Common code for authenticators and resolvers.
**
*/

#include "libauth.h"
#include <sys/socket.h>


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
get_res(struct sockaddr_in* loc,
	struct sockaddr_in* cli)
{
    char result = 0;
    char buf[2048];

    cli->sin_family = AF_INET;
    loc->sin_family = AF_INET;

    /* read the connection info from stdin */

    while(fgets(buf, sizeof(buf), stdin) != (char*) 0) {
	/* strip '\n' */
	buf[strlen(buf)-1] = '\0';

	if (!strncmp(buf, IPNAME, strlen(IPNAME))) {
	    cli->sin_addr.s_addr = inet_addr(buf+strlen(IPNAME));
	    result = result | GOTCLIADDR;
	} else if (!strncmp(buf, PORTNAME, strlen(PORTNAME))) {
	    cli->sin_port = htons(atoi(buf+strlen(PORTNAME)));
	    result = result | GOTCLIPORT;
	} else if (!strncmp(buf, LOCIP, strlen(LOCIP))) {
	    loc->sin_addr.s_addr = inet_addr(buf+strlen(LOCIP));
	    result = result | GOTLOCADDR;
	} else if (!strncmp(buf, LOCPORT, strlen(LOCPORT))) {
	    loc->sin_port = htons(atoi(buf+strlen(LOCPORT)));
	    result = result | GOTLOCPORT;
	}
    }

    return(result);
}

