/*
 * $Revision$
 * Rebuild dbz file for history db.
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
#include "inndcomm.h"
#include "dbz.h"
#include "storage.h"
#include "qio.h"
#include "macros.h"
#include <dirent.h>
#include <syslog.h>  
#include "ov3.h"

char *TextFile = NULL;
char *HistoryDir = NULL;
char *HISTORY = NULL;
/*
**  Remove the DBZ files for the specified base text file.
*/
STATIC void
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
**  Rebuild the DBZ file from the text file.
*/
STATIC void Rebuild(OFFSET_T size, BOOL IgnoreOld, BOOL Overwrite)
{
    QIOSTATE	        *qp;
    char	        *p;
    char	        *save;
    OFFSET_T	        count;
    OFFSET_T		where;
    HASH		key;
    char		temp[SMBUF];
    dbzoptions          opt;
#ifndef	DO_TAGGED_HASH
    TOKEN	token;
    void        *ivalue;
#endif

    if (chdir(HistoryDir) < 0) {
	fprintf(stderr, "makedbz: can't cd to %s\n", HistoryDir);
	exit(1);
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
	RemoveDBZFiles(TextFile);
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
	if (!dbzfresh(p, dbzsize(size), 0)) {
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
	    (void)fprintf(stderr, "Bad line #%ld \"%.30s...\"\n", count, p);
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
	switch (dbzstore(key, (OFFSET_T)where)) {
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
		TextFile, count, strerror(errno));
	if (temp[0])
	    (void)unlink(temp);
	exit(1);
    }
    if (QIOtoolong(qp)) {
	(void)fprintf(stderr, "Line %ld is too long\n", count);
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

void
Usage()
{
    fprintf(stderr, "Usage: makedbz [-f histfile] [-s numlines] [-i] [-o]\n");
    exit(1);
}

int
main(int argc, char **argv)
{
    BOOL	Overwrite;
    BOOL	IgnoreOld;
    OFFSET_T	size;
    int		i;
    char	*p;

    /* First thing, set up logging and our identity. */
    openlog("makedbz", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);     
	
    /* Set defaults. */
    if (ReadInnConf() < 0) exit(1);
    TextFile = COPY(cpcatpath(innconf->pathdb, _PATH_HISTORY));
    HISTORY = COPY(cpcatpath(innconf->pathdb, _PATH_HISTORY));
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
	    IgnoreOld = TRUE;
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
    exit(0);
}
