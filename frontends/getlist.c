/*  $Id$
**
**  Get a file list from an NNTP server.
*/
#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <syslog.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/qio.h"
#include "macros.h"
#include "libinn.h"
#include "paths.h"


/*
**  Print usage message and exit.
*/
static void
Usage(void)
{
    fprintf(stderr, "Usage: getlist [-p port] [-h host] [-A] [type [pat [groups]]\n");
    exit(1);
}


int
main(int ac, char *av[])
{
    FILE	*active;
    FILE	*FromServer;
    FILE	*ToServer;
    QIOSTATE	*qp;
    char	*field4;
    char	*types;
    char	*host;
    char	*line;
    const char	*list;
    char	*p;
    char	*pattern;
    char	buff[512 + 1];
    int		port;
    int		authinfo;
    int		i;

    /* First thing, set up our identity. */
    message_program_name = "getlist";

    if (!innconf_read(NULL))
        exit(1);

    /* Set defaults. */
    host = NULL;
    pattern = NULL;
    types = NULL;
    port = NNTP_PORT;
    authinfo = 0;

    /* Parse JCL. */
    while ((i = getopt(ac, av, "Ah:p:")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'A':
	    authinfo = 1;
	    break;
	case 'h':
	    host = optarg;
	    break;
	case 'p':
	    port = atoi(optarg);
	    if (port <= 0)
                die("illegal value for -p option");
	    break;
	}
    ac -= optind;
    av += optind;

    /* Parse parameters. */
    switch (ac) {
    default:
	Usage();
	/* NOTREACHED */
    case 0:
    case 1:
	break;
    case 2:
	pattern = av[1];
	break;
    case 3:
	pattern = av[1];
	types = av[2];
	break;
    }
    if (av[0] == NULL)
	list = "active";
    else {
	list = av[0];
        if (!EQ(list, "active") && types != NULL)
            Usage();
	if (!EQ(list, "active") && !EQ(list, "newsgroups")
         && pattern != NULL)
	    Usage();
    }

    /* Open a connection to the server. */
    if (host == NULL && (host = innconf->server) == NULL)
        sysdie("cannot get server name");
    buff[0] = '\0';
    if (NNTPconnect(host, port, &FromServer, &ToServer, buff) < 0)
        die("cannot connect to server: %s", buff[0] ? buff : strerror(errno));
    if (authinfo && NNTPsendpassword(host, FromServer, ToServer) < 0)
        die("cannot authenticate to server");

    /* Get the data from the server. */
    active = CAlistopen(FromServer, ToServer, EQ(list, "active") ? NULL : list);
    if (active == NULL)
        sysdie("cannot retrieve data");

    /* Set up to read it quickly. */
    if ((qp = QIOfdopen((int)fileno(active))) == NULL)
        sysdie("cannot read temporary file");

    /* Scan server's output, displaying appropriate lines. */
    i = 1;
    while ((line = QIOread(qp)) != NULL) {
	i++;

	/* No pattern means print all. */
	if (pattern == NULL) {
	    printf("%s\n", line);
	    continue;
	}

	/* Get the group name, see if it's one we want. */
	if ((p = strchr(line, ' ')) == NULL) {
            warn("line %d is malformed", i);
	    continue;
	}
	*p = '\0';
	if (!uwildmat(line, pattern))
	    continue;
	*p = ' ';

	/* If no group types, we want them all. */
	if (types == NULL) {
	    printf("%s\n", line);
	    continue;
	}

	/* Find the fourth field. */
	if ((p = strchr(p + 1, ' ')) == NULL) {
            warn("line %d (field 2) is malformed", i);
	    continue;
	}
	if ((p = strchr(p + 1, ' ')) == NULL) {
            warn("line %d (field 3) is malformed", i);
	    continue;
	}
	field4 = p + 1;
	if ((p = strchr(field4, ' ')) != NULL) {
            warn("line %d has more than 4 fields", i);
	    continue;
	}

	/* Is this the type of line we want? */
	if (strchr(types, field4[0]) != NULL)
	    printf("%s\n", line);
    }

    /* Determine why we stopped */
    if (QIOerror(qp)) {
        syswarn("cannot read temporary file at line %d", i);
	i = 1;
    }
    else if (QIOtoolong(qp)) {
        warn("line %d is too long", i);
	i = i;
    }
    else
	i = 0;

    /* All done. */
    CAclose();
    fprintf(ToServer, "quit\r\n");
    fclose(ToServer);
    fgets(buff, sizeof buff, FromServer);
    fclose(FromServer);
    exit(i);
    /* NOTREACHED */
}
