/*  $Id$
**
**  ident authenticator.
*/
#include "libauth.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/signal.h>

#include "libinn.h"
#include "macros.h"

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

    signal(SIGALRM,out);
    alarm(15);

#define IDENT_PORT 113

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
		if ((s = getservbyname(optarg, "tcp")) == (struct servent *) 0) {
		    fprintf(stderr, "ident: can't getservbyname(%s/tcp)\n", optarg);
		    exit(1);
		}
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
    if (get_res((struct sockaddr *)&loc,(struct sockaddr *)&cli) != (char) GOT_ALL) {
	fprintf(stderr, "ident: didn't get ident parameter\n");
	exit(1);
    }
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
    if ( sock < 0 ) {
	fprintf(stderr, "ident: couldn't create socket: %s\n", strerror(errno));
	exit(1);
    }
    if (bind(sock, (struct sockaddr*) &loc, SA_LEN((struct sockaddr *)&loc)) < 0) {
	fprintf(stderr, "ident: couldn't bind socket: %s\n", strerror(errno));
	exit(1);
    }
    if (connect(sock, (struct sockaddr*) &cli, SA_LEN((struct sockaddr *)&cli)) < 0) {
      if (errno != ECONNREFUSED) {
	fprintf(stderr, "ident: couldn't connect to ident server: %s\n", strerror(errno));
      }
      exit(1);
    }

    /* send the request out */
    sprintf(buf, "%d , %d\r\n", cport, lport);
    got = 0;
    while (got != strlen(buf)) {
	opt = write(sock, buf+got, strlen(buf)-got);
	if (opt < 0)
	    exit(1);
	else if (!opt)
	    exit(1);
	got += opt;
    }

    /* get the answer back */
    got = 0;
    do {
	opt = read(sock, buf+got, sizeof(buf)-got);
	if (opt < 0)
	    exit(1);
	else if (!opt)
	    exit(1);
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
	exit(1);
    iter++;

    while (*iter && ISWHITE(*iter))
	iter++;
    endstr = iter;
    while (*endstr && *endstr != ':' && !ISWHITE(*endstr))
	endstr++;
    if (!*endstr)
	/* malformed response */
	exit(1);
    if (*endstr != ':') {
	*endstr++ = '\0';
	while (*endstr != ':')
	    endstr++;
    }

    *endstr = '\0';

    if (!strcmp(iter, "ERROR"))
	exit(1);
    else if (strcmp(iter, "USERID") != 0)
	/* malformed response */
	exit(1);

    /* skip the operating system */
    if (!(iter = strchr(endstr+1, ':')))
	exit(1);

    /* everything else is username */
    iter++;
    while (*iter && ISWHITE(*iter))
	iter++;
    if (!*iter || *iter == '[')
	/* null, or encrypted response */
	exit(1);
    if ((truncate_domain == 1) && ((p = strchr(iter, '@')) != NULL))
	*p = '\0';
    printf("User:%s\n", iter);

    exit(0);
}
