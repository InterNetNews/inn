/*  $Id$
**
**  Ident authenticator.
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <syslog.h>

#include "inn/messages.h"
#include "libinn.h"
#include "macros.h"

#include "libauth.h"

#define IDENT_PORT 113

static void out(int sig UNUSED) {
    exit(1);
}

int main(int argc, char *argv[])
{
    struct servent *s;
    char buf[2048];
    struct sockaddr_storage loc, cli;
    int sock;
    int opt;
    int truncate_domain = 0;
    char *iter;
    char *p;
    unsigned int got;
    int lport, cport, identport;
    char *endstr;
    socklen_t length;

    message_program_name = "ident";

    xsignal_norestart(SIGALRM,out);
    alarm(15);

    s = getservbyname("ident", "tcp");
    if (!s)
	identport = IDENT_PORT;
    else
	identport = ntohs(s->s_port);

    while ((opt = getopt(argc, argv, "p:t")) != -1) {
	switch (opt) {
	  case 'p':
	    for (iter = optarg; *iter; iter++)
		if (*iter < '0' || *iter > '9')
		    break;
	    if (*iter) {
		/* not entirely numeric */
                s = getservbyname(optarg, "tcp");
                if (s == NULL)
                    die("cannot getsrvbyname(%s, tcp)", optarg);
		identport = s->s_port;
	    } else
		identport = atoi(optarg);
	    break;
	case 't':
	    truncate_domain = 1;
	    break;
	}
    }

    memset(&cli, '\0', sizeof(cli));
    memset(&loc, '\0', sizeof(loc));

    /* read the connection info from stdin */
    if (get_res((struct sockaddr *)&loc,(struct sockaddr *)&cli) != GOT_ALL)
        die("did not get client information from nnrpd");

#ifdef HAVE_INET6
    if( loc.ss_family == AF_INET6 )
    {
	lport = ntohs( ((struct sockaddr_in6 *)&loc)->sin6_port );
	((struct sockaddr_in6 *)&loc)->sin6_port = 0;
	cport = ntohs( ((struct sockaddr_in6 *)&cli)->sin6_port );
	((struct sockaddr_in6 *)&cli)->sin6_port = htons( identport );
	sock = socket(PF_INET6, SOCK_STREAM, 0);
    } else
#endif
    {
	lport = htons( ((struct sockaddr_in *)&loc)->sin_port );
	((struct sockaddr_in *)&loc)->sin_port = 0;
	cport = htons( ((struct sockaddr_in *)&cli)->sin_port );
	((struct sockaddr_in *)&cli)->sin_port = htons( identport );
	sock = socket(PF_INET, SOCK_STREAM, 0);
    }
    if ( sock < 0 )
        sysdie("cannot create socket");
    length = SA_LEN((struct sockaddr *) &loc);
    if (bind(sock, (struct sockaddr*) &loc, length) < 0)
        sysdie("cannot bind socket");
    if (connect(sock, (struct sockaddr*) &cli, length) < 0) {
        if (errno != ECONNREFUSED)
            sysdie("cannot connect to ident server");
        else
            sysdie("client host does not accept ident connections");
    }

    /* send the request out */
    snprintf(buf, sizeof(buf), "%d , %d\r\n", cport, lport);
    opt = xwrite(sock, buf, strlen(buf));
    if (opt < 0)
        sysdie("cannot write to ident server");

    /* get the answer back */
    got = 0;
    do {
	opt = read(sock, buf+got, sizeof(buf)-got);
	if (opt < 0)
            sysdie("cannot read from ident server");
	else if (!opt)
	    die("end of file from ident server before response");
	while (opt--)
	    if (buf[got] != '\n')
		got++;
    } while (buf[got] != '\n');
    buf[got] = '\0';
    if (buf[got-1] == '\r')
	buf[got-1] = '\0';

    /* buf now contains the entire ident response. */
    if (!(iter = strchr(buf, ':')))
	/* malformed response */
        die("malformed response \"%s\" from ident server", buf);
    iter++;

    while (*iter && ISWHITE(*iter))
	iter++;
    endstr = iter;
    while (*endstr && *endstr != ':' && !ISWHITE(*endstr))
	endstr++;
    if (!*endstr)
	/* malformed response */
        die("malformed response \"%s\" from ident server", buf);
    if (*endstr != ':') {
	*endstr++ = '\0';
	while (*endstr != ':')
	    endstr++;
    }

    *endstr = '\0';

    if (strcmp(iter, "ERROR") == 0)
        die("ident server reported an error");
    else if (strcmp(iter, "USERID") != 0)
        die("ident server returned \"%s\", not USERID", iter);

    /* skip the operating system */
    if (!(iter = strchr(endstr+1, ':')))
	exit(1);

    /* everything else is username */
    iter++;
    while (*iter && ISWHITE(*iter))
	iter++;
    if (!*iter || *iter == '[')
	/* null, or encrypted response */
        die("ident response is null or encrypted");
    if ((truncate_domain == 1) && ((p = strchr(iter, '@')) != NULL))
	*p = '\0';
    printf("User:%s\n", iter);

    exit(0);
}
