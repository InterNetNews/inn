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

void out() {
    exit(1);
}

int main(int argc, char *argv[])
{
    struct servent *s;
    char buf[2048];
    struct sockaddr_in sin, loc, cli;
    int sock;
    int opt;
    int truncate = 0;
    extern char *optarg;
    char *iter;
    char *p;
    int got;
    char *endstr;
    char result = 0;

    signal(SIGALRM,out);
    alarm(15);
    memset(&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;

#define IDENT_PORT 113

    s = getservbyname("ident", "tcp");
    if (!s)
	sin.sin_port = htons(IDENT_PORT);
    else
	sin.sin_port = s->s_port;

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
		sin.sin_port = s->s_port;
	    } else
		sin.sin_port = atoi(optarg);
	    sin.sin_port = htons(sin.sin_port);
	    break;
	case 't':
	    truncate = 1;
	    break;
	}
    }

    memset(&cli, '\0', sizeof(cli));
    memset(&loc, '\0', sizeof(loc));

    /* read the connection info from stdin */
    if (get_res(&loc,&cli) != (char) GOT_ALL) {
	fprintf(stderr, "ident: didn't get ident parameter\n");
	exit(1);
    }
    /* got all the client parameters, create our local socket. */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	fprintf(stderr, "ident: couldn't create socket: %s\n", strerror(errno));
	exit(1);
    }
    opt = loc.sin_port;
    loc.sin_port = 0;
    if (bind(sock, (struct sockaddr*) &loc, sizeof(loc)) < 0) {
	fprintf(stderr, "ident: couldn't bind socket: %s\n", strerror(errno));
	exit(1);
    }
    loc.sin_port = opt;
    sin.sin_addr.s_addr = cli.sin_addr.s_addr;
    if (connect(sock, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
      if (errno != ECONNREFUSED) {
	fprintf(stderr, "ident: couldn't connect to %s:%d: %s\n",
	  inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), strerror(errno));
      }
      exit(1);
    }

    /* send the request out */
    sprintf(buf, "%d , %d\r\n", ntohs(cli.sin_port), ntohs(loc.sin_port));
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
    if ((truncate == 1) && ((p = strchr(iter, '@')) != NULL))
	*p = '\0';
    printf("User:%s\n", iter);

    exit(0);
}
