/*  $Id$
**
**  Prune file names from history file.
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <syslog.h>

#include "libinn.h"
#include "paths.h"
#include "inn/history.h"


/*
**  Print usage message and exit.
*/
static void
Usage(void)
{
    fprintf(stderr, "Usage:  prunehistory [-p] [-f file] [input]\n");
}


int
main(int ac, char *av[])
{
    char                *p;
    int                 i;
    char		buff[BUFSIZ];
    const char		*History;
    bool		Passing;
    struct history	*history = NULL;
    int			rc = 0;

    /* First thing, set up logging and our identity. */
    openlog("prunehistory", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);      

    /* Set defaults. */
    if (ReadInnConf() < 0) exit(1);

    History = concatpath(innconf->pathdb, _PATH_HISTORY);
    Passing = FALSE;

    /* Parse JCL. */
    while ((i = getopt(ac, av, "f:p")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'f':
	    History = optarg;
	    break;
	case 'p':
	    Passing = TRUE;
	    break;
	}
    ac -= optind;
    av += optind;
    if (ac) {
	Usage();
	rc = 1;
	goto fail;
    }

    history = HISopen(History, innconf->hismethod, HIS_RDWR);
    if (history == NULL) {
	fprintf(stderr, "Can't set up \"%s\" database, %s\n",
		History, strerror(errno));
	rc = 1;
	goto fail;
    }

    /* Loop over all input. */
    while (fgets(buff, sizeof buff, stdin) != NULL) {
	time_t arrived, posted, expires;

	if ((p = strchr(buff, '\n')) == NULL) {
	    if (Passing)
		printf("%s\n", buff);
	    else
		fprintf(stderr, "Line too long, ignored:\n\t%s\n", buff);
	    continue;
	}
	*p = '\0';

	/* Ignore blank and comment lines. */
	if (buff[0] == '\0' || buff[0] == COMMENT_CHAR) {
	    if (Passing)
		printf("%s\n", buff);
	    continue;
	}

	if (buff[0] != '<' || (p = strchr(buff, '>')) == NULL) {
	    if (Passing)
		printf("%s\n", buff);
	    else
		fprintf(stderr,
		    "Line doesn't start with a <Message-ID>, ignored:\n\t%s\n",
		    buff);
	    continue;
	}
	*++p = '\0';

	if (HISlookup(history, buff, &arrived, &posted, &expires, NULL)) {
	    if (!HISreplace(history, buff, arrived, posted, expires, NULL)) {
		fprintf(stderr, "Can't write new text for \"%s\", %s\n",
			buff, strerror(errno));
	    }
	} else {
	    fprintf(stderr, "No entry for \"%s\", %s\n",
		    buff, strerror(errno));
	}
    }

 fail:
    /* Close files; we're done. */
    if (history != NULL && !HISclose(history)) {
	fprintf(stderr, "Can't close \"%s\", %s\n",
		History, strerror(errno));
	rc = 1;
    }

    closelog();
    return rc;
}
