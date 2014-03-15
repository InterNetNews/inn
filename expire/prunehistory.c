/*  $Id$
**
**  Prune file names from history file.
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <syslog.h>

#include "inn/history.h"
#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/libinn.h"
#include "inn/paths.h"


/*
**  Print usage message and exit.
*/
static void
Usage(void)
{
    fprintf(stderr, "Usage:  prunehistory [-p] [-f file]\n");
    exit(1);
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
    message_program_name = "prunehistory";

    /* Set defaults. */
    if (!innconf_read(NULL))
        exit(1);

    History = concatpath(innconf->pathdb, INN_PATH_HISTORY);
    Passing = false;

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
	    Passing = true;
	    break;
	}
    ac -= optind;
    if (ac) {
	Usage();
	rc = 1;
	goto fail;
    }

    history = HISopen(History, innconf->hismethod, HIS_RDWR);
    if (history == NULL) {
        syswarn("cannot set up %s database", History);
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
                warn("line too long, ignored: %.40s", buff);
	    continue;
	}
	*p = '\0';

	/* Ignore blank and comment lines. */
	if (buff[0] == '\0' || buff[0] == '#') {
	    if (Passing)
		printf("%s\n", buff);
	    continue;
	}

	if (buff[0] != '<' || (p = strchr(buff, '>')) == NULL) {
	    if (Passing)
		printf("%s\n", buff);
	    else
                warn("line doesn't start with a message ID, ignored: %.40s",
                     buff);
	    continue;
	}
	*++p = '\0';

	if (HISlookup(history, buff, &arrived, &posted, &expires, NULL)) {
	    if (!HISreplace(history, buff, arrived, posted, expires, NULL))
                syswarn("cannot write new text for %s", buff);
        } else {
            syswarn("no entry for %s", buff);
        }
    }

 fail:
    /* Close files; we're done. */
    if (history != NULL && !HISclose(history)) {
        syswarn("cannot close %s", History);
	rc = 1;
    }

    return rc;
}
