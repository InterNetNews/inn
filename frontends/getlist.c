/*  $Id$
**
**  Get a file list from an NNTP server.
*/
#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <syslog.h>

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

    /* First thing, set up logging and our identity. */
    openlog("getlist", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);     

    if (ReadInnConf() < 0) exit(1);

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
	    if (port <= 0) {
	       (void)fprintf(stderr, "Illegal value for -p option\n");
	       	exit(1);
	    }
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
    if (host == NULL
     && (host = innconf->server) == NULL) {
	(void)fprintf(stderr, "Can't get server name, %s\n", strerror(errno));
	exit(1);
    }
    buff[0] = '\0';
    if (NNTPconnect(host, port, &FromServer, &ToServer, buff) < 0) {
	(void)fprintf(stderr, "Can't connect to server, %s\n",
		buff[0] ? buff : strerror(errno));
	exit(1);
    }
    if(authinfo && NNTPsendpassword(host, FromServer, ToServer) < 0) {
        (void)fprintf(stderr, "Can't authenticate to server\n");
        exit(1);
    }

    /* Get the data from the server. */
    active = CAlistopen(FromServer, ToServer, EQ(list, "active") ? NULL : list);
    if (active == NULL) {
	(void)fprintf(stderr, "Can't retrieve data, %s\n", strerror(errno));
	(void)fclose(FromServer);
	(void)fclose(ToServer);
	exit(1);
    }

    /* Set up to read it quickly. */
    if ((qp = QIOfdopen((int)fileno(active))) == NULL) {
	(void)fprintf(stderr, "Can't read temp file, %s\n", strerror(errno));
	(void)fclose(FromServer);
	(void)fclose(ToServer);
	exit(1);
    }

    /* Scan server's output, displaying appropriate lines. */
    i = 1;
    while ((line = QIOread(qp)) != NULL) {
	i++;

	/* No pattern means print all. */
	if (pattern == NULL) {
	    (void)printf("%s\n", line);
	    continue;
	}

	/* Get the group name, see if it's one we want. */
	if ((p = strchr(line, ' ')) == NULL) {
	    (void)fprintf(stderr, "Line %d is malformed\n", i);
	    continue;
	}
	*p = '\0';
	if (!uwildmat(line, pattern))
	    continue;
	*p = ' ';

	/* If no group types, we want them all. */
	if (types == NULL) {
	    (void)printf("%s\n", line);
	    continue;
	}

	/* Find the fourth field. */
	if ((p = strchr(p + 1, ' ')) == NULL) {
	    (void)fprintf(stderr, "Line %d (field 2) is malformed.\n", i);
	    continue;
	}
	if ((p = strchr(p + 1, ' ')) == NULL) {
	    (void)fprintf(stderr, "Line %d (field 3) is malformed.\n", i);
	    continue;
	}
	field4 = p + 1;
	if ((p = strchr(field4, ' ')) != NULL) {
	    (void)fprintf(stderr, "Line %d has more than 4 fields\n", i);
	    continue;
	}

	/* Is this the type of line we want? */
	if (strchr(types, field4[0]) != NULL)
	    (void)printf("%s\n", line);
    }

    /* Determine why we stopped */
    if (QIOerror(qp)) {
	(void)fprintf(stderr, "Can't read temp file at line %d, %s\n",
	    i, strerror(errno));
	i = 1;
    }
    else if (QIOtoolong(qp)) {
	(void)fprintf(stderr, "Line %d is too long\n", i);
	i = i;
    }
    else
	i = 0;

    /* All done. */
    CAclose();
    (void)fprintf(ToServer, "quit\r\n");
    (void)fclose(ToServer);
    (void)fgets(buff, sizeof buff, FromServer);
    (void)fclose(FromServer);
    exit(i);
    /* NOTREACHED */
}
