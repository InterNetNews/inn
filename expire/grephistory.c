/*  $Revision$
**
**  Get data from history database.
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <ctype.h>
#include <sys/stat.h>
#if	defined(DO_NEED_TIME)
#include <time.h>
#endif	/* defined(DO_NEED_TIME) */
#include <sys/time.h>
#include <errno.h>
#include "paths.h"
#include "libinn.h"
#include "dbz.h"
#include "macros.h"


/*
**  Get the next filename from the history file.
*/
STATIC BOOL GetName(FILE *F, char *buff, BOOL *Againp)
{
    static char		SPOOL[] = _PATH_SPOOL;
    int	                c;
    char	        *p;

    /* Skip whitespace before filename. */
    while ((c = getc(F)) == ' ')
	continue;
    if (c == EOF || c == '\n')
	return FALSE;

    (void)strcpy(buff, SPOOL);
    p = &buff[STRLEN(SPOOL)];
    *p++ = '/';
    *p++ = (char)c;
    while ((c = getc(F)) != EOF && c != ' ' && c != '\n')
	*p++ = (char)(c == '.' ? '/' : c);
    *p = '\0';
    *Againp = c != EOF && c != '\n';
    p = &buff[STRLEN(SPOOL) + 1];
    if (IsToken(p))
	memmove(buff, p, strlen(p) + 1);
    return TRUE;
}

/*
**  Given a DBZ value, seek to the right spot.
*/
STATIC BOOL HistorySeek(FILE *F, OFFSET_T offset)
{
    register int	c;
    register int	i;

    if (fseek(F, offset, SEEK_SET) == -1) {
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
STATIC void FullLine(FILE *F, OFFSET_T offset)
{
    int	                c;

    if (fseek(F, offset, SEEK_SET) == -1) {
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
STATIC void
IhaveSendme(History, What)
    STRING		History;
    register char	What;
{
    register FILE	*F;
    register char	*p;
    register char	*q;
    HASH		key;
    OFFSET_T            offset;
    struct stat		Sb;
    BOOL		More;
    char		buff[BUFSIZ];
    char		Name[SPOOLNAMEBUFF];

    /* Open history. */
    if (!dbminit(History)) {
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
	offset = dbzfetch(key);

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
		if (stat(Name, &Sb) >= 0) {
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
STATIC NORETURN
Usage()
{
    (void)fprintf(stderr, "Usage: grephistory [flags] MessageID\n");
    exit(1);
}


int
main(ac, av)
    int			ac;
    char		*av[];
{
    register int	i;
    register FILE	*F;
    STRING		History;
    char                *keystr;
    HASH		key;
    OFFSET_T		offset;
    struct stat		Sb;
    BOOL		More;
    char		What;
    char		Name[SPOOLNAMEBUFF];

    /* Set defaults. */
    History = _PATH_HISTORY;
    What = '?';

    /* Parse JCL. */
    while ((i = getopt(ac, av, "f:eiltnqs")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'f':
	    History = optarg;
	    break;
	case 'e':
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

    /* Open the history file, do the lookup. */
    if (!dbminit(History)) {
	(void)fprintf(stderr, "Can't open history database, %s\n",
		strerror(errno));
	exit(1);
    }
    
    keystr = av[0];
    if (*av[0] != '<') {
	/* Add optional braces. */
	keystr = NEW(char, 1 + strlen(av[0]) + 1 + 1);
	(void)sprintf(keystr, "<%s>", av[0]);
    }

    key = HashMessageID(keystr);

    /* Not found. */
    if ((offset = dbzfetch(key)) < 0) {
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
