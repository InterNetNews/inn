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
#include <syslog.h>  


/*
**  Get the next filename from the history file.
*/
STATIC BOOL GetName(FILE *F, char *buff, BOOL *Againp)
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
STATIC BOOL HistorySeek(FILE *F, OFFSET_T offset)
{
    int	c;
    int	i;

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
IhaveSendme(STRING History, char What)
{
    FILE		*F;
    char		*p;
    char		*q;
    HASH		key;
    OFFSET_T            offset;
    struct stat		Sb;
    BOOL		More;
    char		buff[BUFSIZ];
    char		Name[SPOOLNAMEBUFF];
#ifndef DO_TAGGED_HASH
    idxrec		ionevalue;
#endif

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
#ifdef	DO_TAGGED_HASH
	offset = dbzfetch(key);
#else
	if (!dbzfetch(key, &ionevalue)) {
	    if (What == 'i')
		(void)printf("%s\n", p);
	    continue;
	}
	offset = ionevalue.offset;
#endif

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
STATIC NORETURN
Usage(void)
{
    (void)fprintf(stderr, "Usage: grephistory [flags] MessageID\n");
    exit(1);
}


int
main(int ac, char *av[])
{
    int			i;
    FILE		*F;
    STRING		History;
    char                *keystr;
    HASH		key;
    OFFSET_T		offset;
    struct stat		Sb;
    BOOL		More;
    char		What;
    char		Name[SPOOLNAMEBUFF];
#ifndef DO_TAGGED_HASH
    BOOL		val;
    idxrec		ionevalue;
    TOKEN		token;
    int			len;
    char		*p, *q;
#endif

    /* First thing, set up logging and our identity. */
    openlog("grephistory", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);     

    /* Set defaults. */
    if (ReadInnConf() < 0) exit(1);

    History = COPY(cpcatpath(innconf->pathdb, _PATH_HISTORY));

    What = '?';

    /* Parse JCL. */
#ifdef	DO_TAGGED_HASH
    while ((i = getopt(ac, av, "f:ehiltnqs")) != EOF)
#else
    while ((i = getopt(ac, av, "f:ehiloTtnqs")) != EOF)
#endif
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
#ifndef	DO_TAGGED_HASH
	case 'o':
	case 'T':
#endif
	    if (What != '?') {
#ifdef	DO_TAGGED_HASH
		(void)fprintf(stderr, "Only one [eiltnqs] flag allowed.\n");
#else
		(void)fprintf(stderr, "Only one [eiloTtnqs] flag allowed.\n");
#endif
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
#ifdef	DO_TAGGED_HASH
    if ((offset = dbzfetch(key)) < 0) {
	if (What == 'n')
	    (void)fprintf(stderr, "Not found.\n");
	exit(1);
    }
#else
    if (!dbzfetch(key, &ionevalue)) {
	if (What == 'n')
	    (void)fprintf(stderr, "Not found.\n");
	exit(1);
    }
    offset = ionevalue.offset;
#endif

    /* Simple case? */
    if (What == 'q')
	exit(0);

    /* Just give offset into history file */
    if (What == 't') {
      (void)printf("%lu\n", offset);
      exit(0);
    }

#ifndef	DO_TAGGED_HASH
    /* Just give offset into overview */
    if (What == 'T') {
      (void)printf("Overview offset is not available.\n");
      exit(0);
    }

    /* Get overview */
    if (What == 'o') {
      (void)printf("Overview offset is not available.\n");
    }
#endif

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
