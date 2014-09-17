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
#include "inn/network.h"
#include "inn/libinn.h"

#include "libauth.h"

#define IDENT_PORT 113

/*
**  The signal handler for a timeout.  Just exit with a non-zero status.
*/
static void
timeout(int sig UNUSED)
{
    exit(1);
}


int
main(int argc, char *argv[])
{
    struct servent *s;
    char buf[2048];
    struct res_info *res;
    int sock;
    int opt;
    int truncate_domain = 0;
    char *iter;
    char *p;
    unsigned int got;
    int identport;
    char *endstr;

    message_program_name = "ident";

    xsignal_norestart(SIGALRM, timeout);
    alarm(15);

    s = getservbyname("ident", "tcp");
    if (s == NULL)
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

    /* Read the connection info from stdin. */
    res = get_res_info(stdin);
    if (res == NULL)
        die("did not get client information from nnrpd");

    /* Connect back to the client system. */
    sock = network_connect_host(res->clientip, identport, res->localip,
                                DEFAULT_TIMEOUT);
    if (sock < 0) {
        if (errno != ECONNREFUSED)
            sysdie("cannot connect to ident server");
        else
            sysdie("client host does not accept ident connections");
    }

    /* send the request out */
    snprintf(buf, sizeof(buf), "%s , %s\r\n", res->clientport, res->localport);
    opt = xwrite(sock, buf, strlen(buf));
    if (opt < 0)
        sysdie("cannot write to ident server");
    free_res_info(res);

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
    if (*iter == '\0' || *iter == '[')
	/* null, or encrypted response */
        die("ident response is null or encrypted");
    for (p = iter; *p != '\0' && !ISWHITE(*p); p++)
        ;
    *p = '\0';
    if (truncate_domain) {
        p = strchr(iter, '@');
        if (p != NULL)
            *p = '\0';
    }
    print_user(iter);

    exit(0);
}
