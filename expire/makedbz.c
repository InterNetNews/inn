/*  $Id$
**
**  Rebuild dbz file for history db.
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <syslog.h>  

#include "dbz.h"
#include "inn/qio.h"
#include "libinn.h"
#include "macros.h"
#include "paths.h"
#include "storage.h"

/* FIXME: once we figure out how to integrate this stuff with the
 * history API this external visibility of internal voodoo should
 * go */
#define HIS_FIELDSEP            '\t'

char *TextFile = NULL;
char *HistoryDir = NULL;
char *HISTORY = NULL;

/*
**  Remove the DBZ files for the specified base text file.
*/
static void
RemoveDBZFiles(char *p)
{
    static char	NOCANDO[] = "Can't remove \"%s\", %s\n";
    char	buff[SMBUF];

    (void)sprintf(buff, "%s.dir", p);
    if (unlink(buff) && errno != ENOENT)
	(void)fprintf(stderr, NOCANDO, buff, strerror(errno));
#ifdef	DO_TAGGED_HASH
    (void)sprintf(buff, "%s.pag", p);
    if (unlink(buff) && errno != ENOENT)
	(void)fprintf(stderr, NOCANDO, buff, strerror(errno));
#else
    (void)sprintf(buff, "%s.index", p);
    if (unlink(buff) && errno != ENOENT)
	(void)fprintf(stderr, NOCANDO, buff, strerror(errno));
    (void)sprintf(buff, "%s.hash", p);
    if (unlink(buff) && errno != ENOENT)
	(void)fprintf(stderr, NOCANDO, buff, strerror(errno));
#endif
}


/*
**  Count lines in the history text.  A long-winded way of saying "wc -l"
*/
static off_t
Countlines(void)
{
    QIOSTATE *qp;
    off_t count;

    /* Open the text file. */
    qp = QIOopen(TextFile);
    if (qp == NULL) {
	fprintf(stderr, "Can't open \"%s\", %s\n",
		TextFile, strerror(errno));
	exit(1);
    }

    /* Loop through all lines in the text file. */
    count = 0;
    for (; QIOread(qp) != NULL;)
	count++;
    if (QIOerror(qp)) {
	fprintf(stderr, "Can't read \"%s\" near line %lu, %s\n",
		TextFile, (unsigned long) count, strerror(errno));
	exit(1);
    }
    if (QIOtoolong(qp)) {
	fprintf(stderr, "Line %lu of \"%s\" is too long\n",
                (unsigned long) count, TextFile);
	exit(1);
    }

    QIOclose(qp);
    return count;
}


/*
**  Rebuild the DBZ file from the text file.
*/
static void
Rebuild(off_t size, bool IgnoreOld, bool Overwrite)
{
    QIOSTATE	        *qp;
    char	        *p;
    char	        *save;
    off_t	        count;
    off_t		where;
    HASH		key;
    char		temp[SMBUF];
    dbzoptions          opt;

    if (chdir(HistoryDir) < 0) {
	fprintf(stderr, "makedbz: can't cd to %s\n", HistoryDir);
	exit(1);
    }

    /* If we are ignoring the old database and the user didn't specify a table
       size, determine one ourselves from the size of the text history file.
       Note that this will still use the defaults in dbz if the text file is
       empty, since size will still be left set to 0. */
    if (IgnoreOld == TRUE && size == 0) {
	size = Countlines();
	size += (size / 10);
        if (size > 0)
            fprintf(stderr, "no size specified, using %ld\n",
                    (unsigned long) size);
    }

    /* Open the text file. */
    qp = QIOopen(TextFile);
    if (qp == NULL) {
	(void)fprintf(stderr, "Can't open \"%s\", %s\n",
		TextFile, strerror(errno));
	exit(1);
    }

    /* If using the standard history file, force DBZ to use history.n. */
    if (EQ(TextFile, HISTORY) && !Overwrite) {
	(void)sprintf(temp, "%s.n", HISTORY);
	if (link(HISTORY, temp) < 0) {
	    (void)fprintf(stderr, "Can't make temporary link to \"%s\", %s\n",
		    temp, strerror(errno));
	    exit(1);
	}
	RemoveDBZFiles(temp);
	p = temp;
    }
    else {
	temp[0] = '\0';
	/* 
	** only do removedbz files if a. we're using something besides 
	** $pathdb/history, or b. we're ignoring the old db.
	*/
	if (!EQ(TextFile, HISTORY) || IgnoreOld) RemoveDBZFiles(TextFile);
	p = TextFile;
    }

    /* Open the new database, using the old file if desired and possible. */
    dbzgetoptions(&opt);
    opt.pag_incore = INCORE_MEM;
#ifndef	DO_TAGGED_HASH
    opt.exists_incore = INCORE_MEM;
#endif
    dbzsetoptions(opt);
    if (IgnoreOld) {
	if (!dbzfresh(p, dbzsize(size))) {
	    (void)fprintf(stderr, "Can't do dbzfresh, %s\n",
		    strerror(errno));
	    if (temp[0])
		(void)unlink(temp);
	    exit(1);
	}
    }
    else {
	if (!dbzagain(p, HISTORY)) {
	    (void)fprintf(stderr, "Can't do dbzagain, %s\n", strerror(errno));
	    if (temp[0])
		(void)unlink(temp);
	    exit(1);
	}
    }

    /* Loop through all lines in the text file. */
    count = 0;
    for (where = QIOtell(qp); (p = QIOread(qp)) != NULL; where = QIOtell(qp)) {
	count++;
	if ((save = strchr(p, HIS_FIELDSEP)) == NULL) {
	    (void)fprintf(stderr, "Bad line #%ld \"%.30s...\"\n",
                          (unsigned long) count, p);
	    if (temp[0])
		(void)unlink(temp);
	    exit(1);
	}
	*save = '\0';
	switch (*p) {
	case '[':
	    if (strlen(p) != ((sizeof(HASH) * 2) + 2)) {
		fprintf(stderr, "Invalid length for hash %s, skipping\n", p);
		continue;
	    }
	    key = TextToHash(p+1);
	    break;
	default:
	    fprintf(stderr, "Invalid message-id \"%s\" in history text\n", p);
	    continue;
	}
	switch (dbzstore(key, where)) {
	case DBZSTORE_EXISTS:
            fprintf(stderr, "Duplicate message-id \"%s\" in history text\n", p);
	    break;
	case DBZSTORE_ERROR:
	    fprintf(stderr, "Can't store \"%s\", %s\n",
		    p, strerror(errno));
	    if (temp[0])
		(void)unlink(temp);
	    exit(1);
	default:
	    break;
	}
    }
    if (QIOerror(qp)) {
	(void)fprintf(stderr, "Can't read \"%s\" near line %ld, %s\n",
		TextFile, (unsigned long) count, strerror(errno));
	if (temp[0])
	    (void)unlink(temp);
	exit(1);
    }
    if (QIOtoolong(qp)) {
	fprintf(stderr, "Line %ld is too long\n", (unsigned long) count);
	if (temp[0])
	    (void)unlink(temp);
	exit(1);
    }

    /* Close files. */
    QIOclose(qp);
    if (!dbzclose()) {
	(void)fprintf(stderr, "Can't close history, %s\n", strerror(errno));
	if (temp[0])
	    (void)unlink(temp);
	exit(1);
    }

    if (temp[0])
	(void)unlink(temp);
}

static void
Usage(void)
{
    fprintf(stderr, "Usage: makedbz [-f histfile] [-s numlines] [-i] [-o]\n");
    exit(1);
}

int
main(int argc, char **argv)
{
    bool	Overwrite;
    bool	IgnoreOld;
    off_t	size = 0;
    int		i;
    char	*p;

    /* First thing, set up logging and our identity. */
    openlog("makedbz", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);     
	
    /* Set defaults. */
    if (ReadInnConf() < 0) exit(1);
    TextFile = concatpath(innconf->pathdb, _PATH_HISTORY);
    HISTORY = concatpath(innconf->pathdb, _PATH_HISTORY);
    HistoryDir = innconf->pathdb;
    IgnoreOld = FALSE;
    Overwrite = FALSE;

    while ((i = getopt(argc, argv, "s:iof:")) != EOF) {
	switch (i) {
	default:
	    Usage();
	case 'f':
	    TextFile = optarg;
	    break;
	case 's':
	    size = atol(optarg);
	    IgnoreOld = TRUE;
	    break;
	case 'o':
	    Overwrite = TRUE;
	    break;
	case 'i':
	    IgnoreOld = TRUE;
	    break;
	}
    }

    argc -= optind;
    argv += optind;
    if (argc) {
	Usage();
    }

    if ((p = strrchr(TextFile, '/')) == NULL) {
	/* find the default history file directory */
	HistoryDir = innconf->pathdb;
    } else {
	*p = '\0';
	HistoryDir = COPY(TextFile);
	*p = '/';
    }

    if (chdir(HistoryDir) < 0) {
	fprintf(stderr, "makedbz: can't cd to %s\n", HistoryDir);
	exit(1);
    }

    Rebuild(size, IgnoreOld, Overwrite);
    closelog();
    exit(0);
}
