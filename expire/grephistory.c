/*  $Id$
**
**  Get data from history database.
*/

#include "clibrary.h"
#include <syslog.h>  
#include <sys/stat.h>

#include "inn/history.h"
#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/libinn.h"
#include "inn/paths.h"
#include "inn/storage.h"

/*
**  Read stdin for list of Message-ID's, output list of ones we
**  don't have.  Or, output list of files for ones we DO have.
*/
static void
IhaveSendme(struct history *h, char What)
{
    char		*p;
    char		*q;
    char		buff[BUFSIZ];

    while (fgets(buff, sizeof buff, stdin) != NULL) {
	time_t arrived, posted, expires;
	TOKEN token;

	for (p = buff; ISWHITE(*p); p++)
	    ;
	if (*p != '<')
	    continue;
	for (q = p; *q && *q != '>' && !ISWHITE(*q); q++)
	    ;
	if (*q != '>')
	    continue;
	*++q = '\0';

	if (!HIScheck(h, p)) {
	    if (What == 'i')
		printf("%s\n", p);
	    continue;
	}

	/* Ihave -- say if we want it, and continue. */
	if (What == 'i') {
	    continue;
	}

	if (HISlookup(h, p, &arrived, &posted, &expires, &token))
	    printf("%s\n", TokenToText(token));
    }
}


/*
**  Print a usage message and exit.
*/
static void
Usage(void)
{
    fprintf(stderr, "Usage: grephistory [flags] MessageID\n");
    exit(1);
}


int
main(int ac, char *av[])
{
    int			i;
    const char		*History;
    char                *key;
    char		What;
    struct history	*history;
    time_t arrived, posted, expires;
    TOKEN token;
    unsigned int	Verbosity = 0;

    /* First thing, set up logging and our identity. */
    openlog("grephistory", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
    message_program_name = "grephistory";

    /* Set defaults. */
    if (!innconf_read(NULL))
        exit(1);

    History = concatpath(innconf->pathdb, INN_PATH_HISTORY);

    What = '?';

    /* Parse JCL. */
    while ((i = getopt(ac, av, "vf:eilnqs")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'v':
	    Verbosity++;
	    break;
	case 'f':
	    History = optarg;
	    break;
	case 'e':
	case 'i':
	case 'l':
	case 'n':
	case 'q':
	case 's':
	    if (What != '?') {
                die("only one [eilnqs] flag allowed");
	    }
	    What = (char)i;
	    break;
	}
    ac -= optind;
    av += optind;

    history = HISopen(History, innconf->hismethod, HIS_RDONLY);
    if (history == NULL)
        die("cannot open history");

    /* Set operating mode. */
    switch (What) {
    case '?':
	What = 'n';
	break;
    case 'i':
    case 's':
	IhaveSendme(history, What);
	HISclose(history);
	exit(0);
	/* NOTREACHED */
    }

    /* All modes other than -i -l want a Message-ID. */
    if (ac != 1)
	Usage();

    key = av[0];
    if (*key == '[') {
        warn("accessing history by hash isn't supported");
	HISclose(history);
	exit(1);
    } else {
        /* Add optional braces if not already present. */
	if (*key != '<')
            key = concat("<", key, ">", (char *) 0);
    }

    if (!HIScheck(history, key)) {
	if (What == 'n') {
	    if (Verbosity > 0)
		die("not found (hash is %s)", HashToText(HashMessageID(key)));
	    else
		die("not found");
	}
    }
    else if (What != 'q') {
	if (HISlookup(history, key, &arrived, &posted, &expires, &token)) {
	    if (What == 'l') {
		printf("[]\t%ld~-~%ld\t%s\n", (long)arrived, (long)posted,
		       TokenToText(token));
	    }
	    else {
		if (Verbosity > 0)
		    printf("%s (hash is %s)\n", TokenToText(token),
			    HashToText(HashMessageID(key)));
		else
		    printf("%s\n", TokenToText(token));
	    }
	}
	else if (What == 'n')
	    printf("/dev/null\n");
    }
    HISclose(history);
    return 0;
}
