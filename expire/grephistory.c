/*  $Id$
**
**  Get data from history database.
*/

#include "clibrary.h"
#include <errno.h>
#include <syslog.h>  
#include <sys/stat.h>

#include "dbz.h"
#include "libinn.h"
#include "macros.h"
#include "paths.h"
#include "storage.h"


/*
**  Get the next filename from the history file.
*/
static bool GetName(FILE *F, char *buff, bool *Againp)
{
    int	                c;
    char	        *p;

    /* Skip whitespace before filename. */
    while ((c = getc(F)) == ' ')
	continue;
    if (c == EOF || c == '\n')
	return FALSE;

    (void)strcpy(buff, innconf->patharticles);
    p = &buff[strlen(innconf->patharticles)];
    *p++ = '/';
    *p++ = (char)c;
    while ((c = getc(F)) != EOF && c != ' ' && c != '\n')
	*p++ = (char)(c == '.' ? '/' : c);
    *p = '\0';
    *Againp = c != EOF && c != '\n';
    p = &buff[strlen(innconf->patharticles) + 1];
    if (IsToken(p))
	memmove(buff, p, strlen(p) + 1);
    return TRUE;
}

/*
**  Given a DBZ value, seek to the right spot.
*/
static bool HistorySeek(FILE *F, off_t offset)
{
    int	c;
    int	i;

    if (fseeko(F, offset, SEEK_SET) == -1) {
	(void)fprintf(stderr, "Can't seek to %ld, %s\n", offset, strerror(errno));
	return FALSE;
    }

    /* Move to the filename fields. */
    for (i = 2; (c = getc(F)) != EOF && c != '\n'; )
	if (c == HIS_FIELDSEP && --i == 0)
	    break;
    if (c != HIS_FIELDSEP)
	/* Could have only two fields (if expired) so don't complain now.
	 * (void)fprintf(stderr, "Bad text line for \"%s\", %s\n",
	 *	key, strerror(errno));
	 */
	return FALSE;

    return TRUE;
}


/*
**  Print the full line from the history file.
*/
static void FullLine(FILE *F, off_t offset)
{
    int	                c;

    if (fseeko(F, offset, SEEK_SET) == -1) {
	(void)fprintf(stderr, "Can't seek to %ld, %s\n", offset, strerror(errno));
	exit(1);
    }

    while ((c = getc(F)) != EOF && c != '\n')
	(void)putchar(c);
    (void)putchar('\n');
}


/*
**  Read stdin for list of Message-ID's, output list of ones we
**  don't have.  Or, output list of files for ones we DO have.
*/
static void
IhaveSendme(const char *History, char What)
{
    FILE		*F;
    char		*p;
    char		*q;
    HASH		key;
    off_t               offset;
    struct stat		Sb;
    bool		More;
    char		buff[BUFSIZ];
    char		Name[SPOOLNAMEBUFF];

    /* Open history. */
    if (!dbzinit(History)) {
	(void)fprintf(stderr, "Can't open history database, %s\n",
		strerror(errno));
	exit(1);
    }
    if ((F = fopen(History, "r")) == NULL) {
	(void)fprintf(stderr, "Can't open \"%s\", %s\n",
		History, strerror(errno));
	exit(1);
    }

    while (fgets(buff, sizeof buff, stdin) != NULL) {
	for (p = buff; ISWHITE(*p); p++)
	    continue;
	if (*p != '<')
	    continue;
	for (q = p; *q && *q != '>' && !ISWHITE(*q); q++)
	    continue;
	if (*q != '>')
	    continue;
	*++q = '\0';

	key = HashMessageID(p);
	if (!dbzfetch(key, &offset)) {
	    if (What == 'i')
		(void)printf("%s\n", p);
	    continue;
	}

	/* Ihave -- say if we want it, and continue. */
	if (What == 'i') {
	    if (offset < 0)
		(void)printf("%s\n", p);
	    continue;
	}

	/* Sendme -- print a filename for the message. */
	if (offset < 0)
	    /* Doesn't exist. */
	    continue;
	if (HistorySeek(F, offset))
	    while (GetName(F, Name, &More)) {
		if (IsToken(Name) || (stat(Name, &Sb) >= 0)) {
		    (void)printf("%s\n", Name);
		    break;
		}
		if (!More)
		    break;
	    }
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
    FILE		*F;
    const char		*History;
    char                *keystr;
    HASH		key;
    off_t		offset;
    struct stat		Sb;
    bool		More;
    char		What;
    char		Name[SPOOLNAMEBUFF];

    /* First thing, set up logging and our identity. */
    openlog("grephistory", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);     

    /* Set defaults. */
    if (ReadInnConf() < 0) exit(1);

    History = concatpath(innconf->pathdb, _PATH_HISTORY);

    What = '?';

    /* Parse JCL. */
    while ((i = getopt(ac, av, "f:ehiltnqs")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'f':
	    History = optarg;
	    break;
	case 'e':
	case 'h':
	case 'i':
	case 'l':
	case 't':
	case 'n':
	case 'q':
	case 's':
	    if (What != '?') {
		(void)fprintf(stderr, "Only one [eiltnqs] flag allowed.\n");
		exit(1);
	    }
	    What = (char)i;
	    break;
	}
    ac -= optind;
    av += optind;

    /* Set operating mode. */
    switch (What) {
    case '?':
	What = 'n';
	break;
    case 'i':
    case 's':
	IhaveSendme(History, What);
	exit(0);
	/* NOTREACHED */
    }

    /* All modes other than -i -l want a Message-ID. */
    if (ac != 1)
	Usage();

    keystr = av[0];
    if (*av[0] == '[') {
	key = TextToHash(&av[0][1]);
    } else {
	if (*av[0] != '<') {
	    /* Add optional braces. */
	    keystr = NEW(char, 1 + strlen(av[0]) + 1 + 1);
	    (void)sprintf(keystr, "<%s>", av[0]);
	}
	key = HashMessageID(keystr);
    }

    if (What == 'h') {
	(void)printf("[%s]\n", HashToText(key));
	exit(0);
    }

    /* Open the history file, do the lookup. */
    if (!dbzinit(History)) {
	(void)fprintf(stderr, "Can't open history database, %s\n",
		strerror(errno));
	exit(1);
    }

    /* Not found. */
    if (!dbzfetch(key, &offset)) {
	if (What == 'n')
	    (void)fprintf(stderr, "Not found.\n");
	exit(1);
    }

    /* Simple case? */
    if (What == 'q')
	exit(0);

    /* Just give offset into history file */
    if (What == 't') {
      (void)printf("%lu\n", offset);
      exit(0);
    }

    /* Open the text file, go to the entry. */
    if ((F = fopen(History, "r")) == NULL) {
	(void)fprintf(stderr, "Can't open \"%s\", %s\n",
		History, strerror(errno));
	exit(1);
    }
    if (What == 'l') {
	FullLine(F, offset);
	exit(0);
    }

    /* Loop until we find an existing file. */
    if (HistorySeek(F, offset))
	while (GetName(F, Name, &More)) {
	    if (IsToken(Name) || (stat(Name, &Sb) >= 0)) {
		(void)printf("%s\n", Name);
		exit(0);
	    }
	    if (!More)
		break;
	}

    if (What == 'n')
	(void)printf("/dev/null\n");
    exit(0);
    /* NOTREACHED */
}
