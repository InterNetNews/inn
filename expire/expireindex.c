/*
**  Expire overview index.
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <ctype.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "qio.h"
#include "mydir.h"
#include "libinn.h"
#include "macros.h"
#include "paths.h"


#define	START_LIST_SIZE	128


/*
**   Information about a line in the overview file.
*/
typedef struct _LINE {
    ARTNUM	Article;
    char	*Start;
    int		Length;
    int		Offset;
} LINE;

/*
**  A list of articles; re-uses space.
*/
typedef struct _LIST {
    int		Used;
    int		Size;
    ARTNUM	*Articles;
} LIST;


/*
**  A buffer; re-uses space.
*/
typedef struct _BUFFER {
    int		Used;
    int		Size;
    char	*Data;
} BUFFER;

typedef struct _ARTLIST {
    ARTNUM	ArtNum;
    char	(*Index)[OVERINDEXPACKSIZE];
} ARTLIST;

/*
**  Information about the schema of the news overview files.
*/
typedef struct _ARTOVERFIELD {
    char       *Header;
    int                Length;
    BOOL       HasHeader;
} ARTOVERFIELD;

    
/*
**  Append an article to an LIST.
*/
#define LISTappend(L, a)	\
	if ((L).Size == (L).Used) {			\
	    (L).Size *= 2;				\
	    RENEW((L).Articles, ARTNUM, (L).Size);	\
	    (L).Articles[(L).Used++] = (a);		\
	}						\
	else						\
	    (L).Articles[(L).Used++] = (a)


/*
**  Global variables.
*/
STATIC char		SPOOL[] = _PATH_SPOOL;
STATIC BOOL		InSpoolDir;
STATIC BOOL		Verbose;
STATIC BOOL		Quiet;
STATIC BOOL		DoNothing;
STATIC ARTOVERFIELD	*ARTfields;
STATIC int		ARTfieldsize;
STATIC BOOL		OVERmmap;
STATIC char		(*OVERindex)[][OVERINDEXPACKSIZE];
STATIC int		OVERicount;
STATIC int		ARTsize;
STATIC ARTLIST		*ARTnumbers;
STATIC char		(*OVERindexnew)[][OVERINDEXPACKSIZE];


/*
**  Sorting predicate for qsort to put articles in numeric order.
*/
STATIC int LISTcompare(CPOINTER p1, CPOINTER p2)
{
    ARTNUM	*ip1;
    ARTNUM	*ip2;

    ip1 = CAST(ARTNUM*, p1);
    ip2 = CAST(ARTNUM*, p2);
    return *ip1 - *ip2;
}


/*
**  If list is big enough, and out of order, sort it.
*/
STATIC void LISTsort(LIST *lp)
{
    int	                i;
    ARTNUM	        *ap;

    for (ap = lp->Articles, i = lp->Used - 1; --i >= 0; ap++)
	if (ap[0] >= ap[1]) {
	    qsort((POINTER)lp->Articles, (SIZE_T)lp->Used,
		sizeof lp->Articles[0], LISTcompare);
	    break;
	}
}

/*
**  Unlock the group.
*/
STATIC void UnlockGroup(int lfd, char *lockfile)
{
    if (lfd > 0) {
	if (unlink(lockfile) < 0 && errno != ENOENT)
	    (void)fprintf(stderr, "expireover cant unlink %s %s\n",
		    lockfile, strerror(errno));
	if (close(lfd) < 0)
	    (void)fprintf(stderr, "expireover cant close %s %s\n",
		    lockfile, strerror(errno));
	lfd = -1;
    }
}


/*
**  Sorting predicate to put newsgroup names into numeric order.
*/      
STATIC int ARTcompare(CPOINTER p1, CPOINTER p2)
{
    return ((ARTLIST *)p1)->ArtNum - ((ARTLIST *)p2)->ArtNum;
}

STATIC void WriteIndex(int fd)
{
    int				i, count;
    OVERINDEX			index;

    for (i = 0, count = 0; i < OVERicount; i++) {
	if (ARTnumbers[i].ArtNum != 0) {
	    memcpy((*OVERindexnew)[count++], *(ARTnumbers[i].Index), OVERINDEXPACKSIZE);
	}
    }
    write(fd, OVERindexnew, count * OVERINDEXPACKSIZE);
}


/*
**  Take in a sorted list of count article numbers in group, and delete
**  them from the overview file.
*/
STATIC void RemoveLines(char *group, LIST *Deletes)
{
    static int			LineSize;
    ARTLIST		        *an;
    LINE		        *end;
    char		        *p;
    ARTNUM		        *ap;
    int		                i;
    struct stat			Sb;
    char			ifile[SPOOLNAMEBUFF];
    char			ilockfile[SPOOLNAMEBUFF];
    int				count, icount;
    int				ifd, ilfd;
    int				ARTarraysize;
    OVERINDEX			index;
    char			(*tmp)[][OVERINDEXPACKSIZE];
    
    ARTsize = 0;

    if (Verbose) {
	for (ap = Deletes->Articles, i = Deletes->Used; --i >= 0; ap++)
	    (void)printf("- %s/%ld\n", group, *ap);
	if (DoNothing)
	    return;
    }

    /* Lock the group. */
    (void)sprintf(ilockfile, "%s/.LCK%s.index", group, _PATH_OVERVIEW);
    ilfd = open(ilockfile, O_WRONLY | O_TRUNC | O_CREAT, ARTFILE_MODE);
    if (ilfd < 0) {
	(void)fprintf(stderr, "Can't open %s, %s\n", ilockfile, strerror(errno));
	return;
    }

    /* Open file, lock it. */
    (void)sprintf(ifile, "%s/%s.index", group, _PATH_OVERVIEW);
    for (i = 0; i < 15; i++) {
	if ((ifd = open(ifile, O_RDWR)) < 0) {
	    (void)fprintf(stderr, "Can't open %s, %s\n", ifile, strerror(errno));
	    UnlockGroup(ilfd, ilockfile);
	    return;
	}
	if (LockFile(ifd, FALSE) >= 0)
	    break;
	/* Wait for lock; close file -- might be unlinked -- and try again. */
	(void)LockFile(ifd, TRUE);
	(void)close(ifd);
	sleep(i);
    }
    if (i >= 15) {
	fprintf(stderr, "Can't open/lock %s, %s\n", ifile, strerror(errno));
	close(ifd);
	return;
    }
    if (fstat(ifd, &Sb) < 0) {
	(void)fprintf(stderr, "Can't open %s, %s\n", ifile, strerror(errno));
	UnlockGroup(ilfd, ilockfile);
	(void)close(ifd);
	return;
    }
    if (Sb.st_size == 0) {
	/* Empty file; done deleting. */
	UnlockGroup(ilfd, ilockfile);
	(void)close(ifd);
	return;
    }

    icount = Sb.st_size / OVERINDEXPACKSIZE;
    if (OVERmmap) {
	if ((tmp = (char (*)[][OVERINDEXPACKSIZE])mmap((MMAP_PTR)0, icount * OVERINDEXPACKSIZE,
	    PROT_READ, MAP__ARG, ifd, 0)) == (char (*)[][OVERINDEXPACKSIZE])-1) {
	    (void)fprintf(stderr, "cant mmap index %s, %s\n", ifile, strerror(errno));
	    UnlockGroup(ilfd, ilockfile);
	    (void)close(ifd);
	    return;
	}
    } else {
	tmp = (char (*)[][OVERINDEXPACKSIZE])NEW(char, icount * OVERINDEXPACKSIZE);
	if (read(ifd, tmp, icount * OVERINDEXPACKSIZE) != (icount * OVERINDEXPACKSIZE)) {
	    (void)fprintf(stderr, "cant read index %s, %s\n", ifile, strerror(errno));
	    UnlockGroup(ilfd, ilockfile);
	    (void)close(ifd);
	    return;
	}
    }
    if (OVERindex) {
	if (OVERmmap) {
	    if ((munmap((MMAP_PTR)OVERindex, OVERicount * OVERINDEXPACKSIZE)) < 0)
		(void)fprintf(stderr, "cant munmap index %s, %s\n", ifile, strerror(errno));
	} else {
	    DISPOSE(OVERindex);
	}
    }
    OVERindex = tmp;
    OVERicount = icount;
    if (ARTarraysize == 0) {
	ARTnumbers = NEW(ARTLIST, OVERicount);
	OVERindexnew = (char (*)[][OVERINDEXPACKSIZE])NEW(char, OVERicount * OVERINDEXPACKSIZE);
    } else {
	ARTnumbers = RENEW(ARTnumbers, ARTLIST, OVERicount);
	p = (char *)OVERindexnew;
	p = RENEW(p, char, OVERicount * OVERINDEXPACKSIZE);
	OVERindexnew = (char (*)[][OVERINDEXPACKSIZE])p;
    }
    ARTarraysize = OVERicount;
    for (i = 0; i < OVERicount; i++) {
	UnpackOverIndex((*OVERindex)[i], &index);
	ARTnumbers[ARTsize].ArtNum = index.artnum;
	ARTnumbers[ARTsize++].Index = &(*OVERindex)[i];
    }
    qsort((POINTER)ARTnumbers, (SIZE_T)ARTsize, sizeof(ARTLIST), ARTcompare);

    /* Remove duplicates. */
    for (i = 1; i < OVERicount; i++)
	if (ARTnumbers[i].ArtNum == ARTnumbers[i-1].ArtNum)
	    ARTnumbers[i].ArtNum = 0;

    /* Scan through lines, collecting clumps and skipping holes. */
    ap = Deletes->Articles;
    count = Deletes->Used;
    for (i = 0, an = ARTnumbers; i < OVERicount; an++, i++) {
	/* An already-removed article, or one that should be? */
	if (an->ArtNum == 0)
	    continue;

	/* Skip delete items before the current one. */
	while (*ap < an->ArtNum && count > 0) {
	    ap++;
	    count--;
	}

	if (count > 0 && an->ArtNum == *ap) {
	    while (*ap == an->ArtNum && count > 0) {
		ap++;
		count--;
		an->ArtNum = 0;
	    }
	    continue;
	}
    }

    WriteIndex(ilfd);

    if (rename(ilockfile, ifile) < 0)
	(void)fprintf(stderr, "Can't rename %s, %s\n",
		ilockfile, strerror(errno));

    /* Don't call UnlockGroup; do it inline. */
    if (close(ifd) < 0)
	(void)fprintf(stderr, "expireover cant close %s %s\n",
		ifile, strerror(errno));
    if (close(ilfd) < 0)
	(void)fprintf(stderr, "expireover cant close %s %s\n",
		ilockfile, strerror(errno));
}


/*
**  Expire by batch, or line at a time.
*/
STATIC void Expire(BOOL SortedInput, QIOSTATE *qp)
{
    static LIST		List;
    char	        *line;
    char	        *p;
    char		group[SPOOLNAMEBUFF];

    if (List.Articles == NULL) {
	List.Size = START_LIST_SIZE;
	List.Articles = NEW(ARTNUM, List.Size);
    }
    List.Used = 0;

    if (SortedInput) {
	for ( ; ; ) {
	    if ((line = QIOread(qp)) == NULL) {
		if (QIOerror(qp)) {
		    (void)fprintf(stderr, "Can't read input %s\n",
			    strerror(errno));
		    break;
		}
		if (QIOtoolong(qp))
		    continue;
		break;
	    }
	    if ((p = strrchr(line, ':')) == NULL)
		continue;
	    *p++ = '\0';
	    if (List.Used == 0) {
		(void)strcpy(group, line);
		List.Used = 0;
	    }
	    else if (!EQ(line, group)) {
		LISTsort(&List);
		RemoveLines(group, &List);
		(void)strcpy(group, line);
		List.Used = 0;
	    }
	    LISTappend(List, atol(p));
	}

	/* Do the last group. */
	if (List.Used) {
	    LISTsort(&List);
	    RemoveLines(group, &List);
	}
    }
    else {
	for (List.Used = 1; ; ) {
	    if ((line = QIOread(qp)) == NULL) {
		if (QIOerror(qp)) {
		    (void)fprintf(stderr, "Can't read input %s\n",
			    strerror(errno));
		    break;
		}
		if (QIOtoolong(qp))
		    continue;
		break;
	    }
	    if ((p = strrchr(line, '/')) == NULL)
		continue;
	    *p++ = '\0';
	    List.Articles[0] = atol(p);
	    RemoveLines(line, &List);
	}
    }

    QIOclose(qp);
}


/*
**  Print usage message and exit.
*/
STATIC NORETURN Usage(void)
{
    (void)fprintf(stderr, "Usage:  expireindex [flags] [file...]\n");
    exit(1);
}


int main(int ac, char *av[])
{
    int	                i;
    QIOSTATE		*qp;
    BOOL		SortedInput;
    char		*Dir;
    char		*Name;

    /* Set defaults. */
    Dir = _PATH_OVERVIEWDIR;
    Name = _PATH_ACTIVE;
    SortedInput = FALSE;
    (void)umask(NEWSUMASK);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "D:f:nqvz")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'D':
	    Dir = optarg;
	    break;
	case 'f':
	    Name = optarg;
	    break;
	case 'n':
	    DoNothing = TRUE;
	    break;
	case 'q':
	    Quiet = TRUE;
	    break;
	case 'v':
	    Verbose = TRUE;
	    break;
	case 'z':
	    SortedInput = TRUE;
	    break;
	}
    ac -= optind;
    av += optind;

    /* Setup. */
    if (chdir(Dir) < 0) {
	(void)fprintf(stderr, "Cant chdir to %s, %s\n", Dir, strerror(errno));
	exit(1);
    }
    InSpoolDir = EQ(Dir, SPOOL);

    OVERmmap = GetBooleanConfigValue(_CONF_OVERMMAP, TRUE);
    /* Do work. */
    if (ac == 0)
	Expire(SortedInput, QIOfdopen(STDIN));
    else {
	for ( ; *av; av++)
	    if (EQ(*av, "-"))
		Expire(SortedInput, QIOfdopen(STDIN));
	    else if ((qp = QIOopen(*av)) == NULL)
		(void)fprintf(stderr, "Can't open %s, %s\n",
			*av, strerror(errno));
	    else
		Expire(SortedInput, qp);
    }

    exit(0);
    /* NOTREACHED */
}

