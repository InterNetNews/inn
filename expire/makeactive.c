/*  $Revision$
**
**  Build an active file from either an old copy or by calling find
**  to get the directory names.
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include "paths.h"
#include "libinn.h"
#include "mydir.h"
#include "macros.h"
#include <syslog.h>  


STATIC char	*ACTIVE = NULL;
STATIC char	*SPOOL = NULL;
STATIC char	*OVERVIEWDIR = NULL;
STATIC BOOL	StorageAPI;
STATIC BOOL	OVERmmap;

/*
**  read from overview index
*/
STATIC void
ReadOverviewIndex(char *name, long *lomark, long *himark)
{
    char	*p;
    FILE	*fi;
    struct stat	sb;
    char	(*mapped)[][OVERINDEXPACKSIZE];
    int		count;
    int		i;
    OVERINDEX	index;
    char	packed[OVERINDEXPACKSIZE];

    p = NEW(char, strlen(OVERVIEWDIR) + strlen(name) + strlen(innconf->overviewname) + 32);
    sprintf(p, "%s/%s/%s.index", OVERVIEWDIR, name, innconf->overviewname);
    if ((fi = fopen(p, "r")) == NULL) {
	return;
    }
    DISPOSE(p);
    if (OVERmmap) {
	if (fstat(fileno(fi), &sb) < 0) {
	    fclose(fi);
	    return;
	}
	count = sb.st_size / OVERINDEXPACKSIZE;
	if (count == 0) {
	    fclose(fi);
	    return;
	}
	if ((mapped = (char (*)[][OVERINDEXPACKSIZE])mmap((MMAP_PTR)0, count * OVERINDEXPACKSIZE, 
	    PROT_READ, MAP__ARG, fileno(fi), 0)) == (char (*)[][OVERINDEXPACKSIZE])-1) {
	    fclose(fi);
	    return;
	}
	fclose(fi);
	for (i = 0; i < count; i++) {
	    UnpackOverIndex((*mapped)[i], &index);
	    if (index.artnum < *lomark)
		*lomark = index.artnum;
	    if (index.artnum > *himark)
		*himark = index.artnum;
	}
	(void)munmap((MMAP_PTR)mapped, count * OVERINDEXPACKSIZE);
    } else {
	while (fread(&packed, OVERINDEXPACKSIZE, 1, fi) == 1) {
	    UnpackOverIndex(packed, &index);
	    if (index.artnum < *lomark)
		*lomark = index.artnum;
	    if (index.artnum > *himark)
		*himark = index.artnum;
	}
	fclose(fi);
    }
}

/*
**  Given an newsgroup name, write the active file entry.
*/
STATIC BOOL
MakeEntry(char *name, char *rest, long oldhimark, long oldlomark, BOOL ComputeMarks)
{
    long	himark;
    long	lomark;
    DIR		*dp;
    DIRENTRY	*ep;
    long	j;
    char	*p;
    FILE	*fi;
    struct stat	sb;
    char	(*mapped)[][OVERINDEXPACKSIZE];
    int		count;
    int		i;
    OVERINDEX	index;
    char	packed[OVERINDEXPACKSIZE];

    /* Turn group name into directory name. */
    for (p = name; *p; p++)
	if (*p == '.')
	    *p = '/';

    /* Set initial hi and lo marks. */
    if (ComputeMarks) {
	himark = 0;
	lomark = 0;
    }
    else {
	himark = oldhimark;
	lomark = oldlomark;
    }

    if (StorageAPI) {
	ReadOverviewIndex(name, &lomark, &himark);
    } else {
        if ((dp = opendir(name)) != NULL) {
	    /* Scan through all entries in the directory. */
	    while ((ep = readdir(dp)) != NULL) {
	        p = ep->d_name;
	        if (!CTYPE(isdigit, p[0]) || strspn(p, "0123456789") != strlen(p)
	         || (j = atol(p)) == 0)
		    continue;
	        if (lomark == 0 || j < lomark)
		    lomark = j;
	        if (j > himark)
		    himark = j;
	    }
	    (void)closedir(dp);
	}
    }
    if (lomark == 0 || lomark - 1 > himark)
	lomark = himark + 1;

    /* Reset marks if computed them and didn't find any articles. */
    if (ComputeMarks && lomark == 1 && himark == 0) {
	himark = oldhimark;
	lomark = oldlomark;
    }
    /* Turn the directory name back into a newsgroup name. */
    for (p = name; *p; p++)
	if (*p == '/')
	    *p = '.';
    if (printf("%s %010ld %010ld %s\n",
	    name, himark, lomark, rest) == EOF
     || fflush(stdout) == EOF
     || ferror(stdout)) {
	(void)fprintf(stderr, "Error writing %s entry, %s\n",
		name, strerror(errno));
	return FALSE;
    }
    return TRUE;
}


/*
**  See if a line is too long to be a newsgroup name, return TRUE if so.
*/
STATIC BOOL
TooLong(char *buff, int i)
{
    char	*p;

    if ((p = strchr(buff, '\n')) == NULL) {
	(void)fprintf(stderr, "Line %d is too long:  \"%.40s\"...\n",
		i, buff);
	return TRUE;
    }
    *p = '\0';
    if (p - buff > SMBUF) {
	(void)fprintf(stderr, "Group line %d is too long: \"%.40s\"...\n",
		i, buff);
	return TRUE;
    }
    return FALSE;
}


/*
**  Renumber the active file based on the old active file.
*/
STATIC BOOL
RebuildFromOld(BOOL ComputeMarks)
{
    FILE	*F;
    char	*p;
    int		i;
    BOOL	Ok;
    char	buff[BUFSIZ];
    STRING	rest;
    long	lomark;
    long	himark;
    char	*save1;
    char	*save2;

    /* Open the file. */
    if ((F = fopen(ACTIVE, "r")) == NULL) {
	(void)fprintf(stderr, "Can't open \"%s\", %s\n",
		ACTIVE, strerror(errno));
	exit(1);
    }

    /* Process each entry. */
    for (i = 1, Ok = TRUE; fgets(buff, sizeof buff, F) != NULL; i++) {
	if (TooLong(buff, i)) {
	    Ok = FALSE;
	    continue;
	}

	/* Set default fields. */
	lomark = 0;
	himark = 0;
	rest = "y";

	/* Try to parse the other fields. */
	if ((p = strchr(buff, ' ')) != NULL) {
	    *p++ = '\0';
	    save1 = p;
	    if ((p = strchr(p, ' ')) != NULL) {
		*p++ = '\0';
		save2 = p;
		if ((p = strchr(p, ' ')) != NULL) {
		    *p++ = '\0';
		    rest = p;
		    lomark = atol(save2);
		    himark = atol(save1);
		}
	    }
	}

	if (!MakeEntry(buff, (char *)rest, himark, lomark, ComputeMarks)) {
	    Ok = FALSE;
	    break;
	}
    }

    (void)fclose(F);
    return Ok;
}


STATIC BOOL
RebuildFromFind(void)
{
    int	i;
    char	*p;
    FILE	*F;
    BOOL	Ok;
    char		buff[BUFSIZ];

    /* Start getting a list of the directories. */
#if	defined(HAVE_SYMLINK)
    F = popen("exec find . -follow -type d -print", "r");
#else
    F = popen("exec find . -type d -print", "r");
#endif	/* defined(HAVE_SYMLINK) */
    if (F == NULL) {
	(void)fprintf(stderr, "Can't start find, %s\n", strerror(errno));
	exit(1);
    }

    /* Loop over all input. */
    for (i = 1, Ok = TRUE; fgets(buff, sizeof buff, F) != NULL; i++) {
	if (TooLong(buff, i)) {
	    Ok = FALSE;
	    continue;
	}

	/* Skip leading "./" and some known-to-be-bad directories. */
	p = buff[0] == '.' && buff[1] == '/' ? &buff[2] : buff;
	if (EQ(p, "lost+found") || strchr(p, '.') != NULL)
	    continue;
	if (!MakeEntry(p, "y", 0L, 0L, FALSE)) {
	    Ok = FALSE;
	    break;
	}
    }

    /* Clean up. */
    i = pclose(F) >> 8;
    if (i) {
	(void)fprintf(stderr, "Find exited with status %d\n", i);
	Ok = FALSE;
    }
    return Ok;
}


/*
**  Print a usage message and exit.
*/
STATIC NORETURN
Usage(void)
{
    (void)fprintf(stderr, "Usage: makeactive [-o [-m] ] >output\n");
    exit(1);
    /* NOTREACHED */
}


int
main(int ac, char *av[])
{
    BOOL		Ok;
    int	i;
    BOOL		OldFile;
    BOOL		ComputeMarks;

    /* First thing, set up logging and our identity. */
    openlog("makeactive", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);     

    /* Set defaults. */
    OldFile = FALSE;
    ComputeMarks = FALSE;

    if (ReadInnConf() < 0) exit(1);

    (void)umask(NEWSUMASK);

    ACTIVE = COPY(cpcatpath(innconf->pathdb, _PATH_ACTIVE));

    /* Parse JCL. */
    while ((i = getopt(ac, av, "mo")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'm':
	    ComputeMarks = TRUE;
	    break;
	case 'o':
	    OldFile = TRUE;
	    break;
	}
    ac -= optind;
    av += optind;
    if (ac || (ComputeMarks && !OldFile))
	Usage();

    StorageAPI = innconf->storageapi;
    OVERmmap = innconf->overviewmmap;
    /* Go to where the articles are. */
    SPOOL = innconf->patharticles;
    OVERVIEWDIR = innconf->pathoverview;
    if (chdir(StorageAPI ? OVERVIEWDIR : SPOOL) < 0) {
	(void)fprintf(stderr, "Can't change to spool directory, %s\n",
		strerror(errno));
	exit(1);
    }

    if (OldFile)
	Ok = RebuildFromOld(ComputeMarks);
    else
	Ok = RebuildFromFind();

    if (fflush(stdout) || ferror(stdout)) {
	(void)fprintf(stderr, "Can't flush stdout, %s\n", strerror(errno));
	Ok = FALSE;
    }

    exit(Ok ? 0 : 1);
    /* NOTREACHED */
}
