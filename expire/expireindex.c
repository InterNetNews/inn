/*  $Revision$
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
    char	(*Index)[OVERINDEXPACKSIZE];
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
**  Global variables.
*/
STATIC char		*SPOOL = NULL;
STATIC BOOL		InSpoolDir;
STATIC BOOL		Verbose;
STATIC BOOL		Quiet;
STATIC BOOL		DoNothing;
STATIC BOOL		Append;
STATIC BOOL		Lowmark;
STATIC BOOL		Overwrite;
STATIC ARTOVERFIELD	*ARTfields;
STATIC int		ARTfieldsize;
STATIC BOOL		OVERmmap;
STATIC char		(*OVERindex)[][OVERINDEXPACKSIZE];
STATIC int		OVERicount;
STATIC int		ARTsize;
STATIC ARTLIST		*ARTnumbers;
STATIC char		(*OVERindexnew)[][OVERINDEXPACKSIZE];


/*
**  Append an article to an LIST.
*/
STATIC void LISTappend(LIST *Refresh, long Artnum, HASH *Hash)
{
    OVERINDEX		index;
    char		*p;

    if (Refresh->Size == Refresh->Used) {
        Refresh->Size *= 2;
        RENEW(Refresh->Articles, ARTNUM, Refresh->Size);
        if (Append) {
	    p = (char *)Refresh->Index;
	    RENEW(p, char, Refresh->Size * OVERINDEXPACKSIZE);
	    Refresh->Index = (char (*)[OVERINDEXPACKSIZE])p;
	    index.artnum = Artnum;
	    index.hash = *Hash;
	    PackOverIndex(&index, Refresh->Index[Refresh->Used]);
        }
	Refresh->Articles[Refresh->Used++] = Artnum;
    } else {
	if (Append) {
	    index.artnum = Artnum;
	    index.hash = *Hash;
	    PackOverIndex(&index, Refresh->Index[Refresh->Used]);
	}
	Refresh->Articles[Refresh->Used++] = Artnum;
    }
}


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
**  Try to make one directory.  Return FALSE on error.
*/
STATIC BOOL MakeDir(char *Name)
{
    struct stat	Sb;

    if (mkdir(Name, GROUPDIR_MODE) >= 0)
	return TRUE;

    /* See if it failed because it already exists. */
    return stat(Name, &Sb) >= 0 && S_ISDIR(Sb.st_mode);
}

/*
**  Make overview directory if not in spool directory.  Return 0 if ok,
**  else -1.
*/
STATIC BOOL MakeOverDir(char *Name)
{
    char	*p;
    BOOL	made;

    /* Optimize common case -- parent almost always exists. */
    if (MakeDir(Name))
	return TRUE;

    /* Try to make each of comp and comp/foo in turn. */
    for (p = Name; *p; p++)
	if (*p == '/') {
	    *p = '\0';
	    made = MakeDir(Name);
	    *p = '/';
	    if (!made)
		return FALSE;
	}
    return MakeDir(Name);
}

/*
**  Take in a sorted list of count article numbers in group, and delete
**  or add them from or into the overview index file.
*/
STATIC void RefreshLines(char *group, LIST *Refresh)
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
    static int			ARTarraysize = 0;
    OVERINDEX			index;
    char			(*tmp)[][OVERINDEXPACKSIZE];
    
    ARTsize = 0;

    if (Verbose) {
	for (ap = Refresh->Articles, i = Refresh->Used; --i >= 0; ap++)
	    (void)printf("- %s/%ld\n", group, *ap);
	if (DoNothing)
	    return;
    }

    /* Lock the group. */
    (void)sprintf(ilockfile, "%s/.LCK%s.index", group, innconf->overviewname);
    ilfd = open(ilockfile, O_WRONLY | O_TRUNC | O_CREAT, ARTFILE_MODE);
    if (ilfd < 0) {
	if (!MakeOverDir(group)) {
	    (void)fprintf(stderr, "Can't mkdir %s, %s\n", group, strerror(errno));
	    return;
	}
	ilfd = open(ilockfile, O_WRONLY | O_TRUNC | O_CREAT, ARTFILE_MODE);
	if (ilfd < 0) {
	    (void)fprintf(stderr, "Can't open %s, %s\n", ilockfile, strerror(errno));
	    return;
	}
    }

    /* Open file, lock it. */
    (void)sprintf(ifile, "%s/%s.index", group, innconf->overviewname);
    for (i = 0; i < 15; i++) {
	if ((ifd = open(ifile, O_RDWR | O_CREAT, ARTFILE_MODE)) < 0) {
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
	(void)fprintf(stderr, "Can't stat %s, %s\n", ifile, strerror(errno));
	UnlockGroup(ilfd, ilockfile);
	(void)close(ifd);
	return;
    }
    if (!Append && Sb.st_size == 0) {
	/* Empty file; done deleting. */
	UnlockGroup(ilfd, ilockfile);
	(void)close(ifd);
	return;
    }

    if (Overwrite)
	icount = 0;
    else
	icount = Sb.st_size / OVERINDEXPACKSIZE;
    if (OVERmmap) {
	if (icount != 0 && (tmp = (char (*)[][OVERINDEXPACKSIZE])mmap((MMAP_PTR)0, icount * OVERINDEXPACKSIZE,
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
	    munmap((MMAP_PTR)OVERindex, OVERicount * OVERINDEXPACKSIZE);
	} else {
	    DISPOSE(OVERindex);
	}
    }
    OVERindex = tmp;
    if (Append)
	OVERicount = icount + Refresh->Used;
    else
	OVERicount = icount;
    if (ARTarraysize == 0) {
	ARTnumbers = NEW(ARTLIST, OVERicount);
	OVERindexnew = (char (*)[][OVERINDEXPACKSIZE])NEW(char, OVERicount * OVERINDEXPACKSIZE);
	ARTarraysize = OVERicount;
    } else if (ARTarraysize < OVERicount) {
	RENEW(ARTnumbers, ARTLIST, OVERicount);
	p = (char *)OVERindexnew;
	RENEW(p, char, OVERicount * OVERINDEXPACKSIZE);
	OVERindexnew = (char (*)[][OVERINDEXPACKSIZE])p;
	ARTarraysize = OVERicount;
    }
    for (i = 0; i < icount; i++) {
	UnpackOverIndex((*OVERindex)[i], &index);
	ARTnumbers[ARTsize].ArtNum = index.artnum;
	ARTnumbers[ARTsize++].Index = &(*OVERindex)[i];
    }
    if (Append) {
	for (i = 0; i < Refresh->Used; i++) {
	    ARTnumbers[ARTsize].ArtNum = Refresh->Articles[i];
	    ARTnumbers[ARTsize++].Index = &(Refresh->Index)[i];
	}
    }
    qsort((POINTER)ARTnumbers, (SIZE_T)ARTsize, sizeof(ARTLIST), ARTcompare);

    /* Remove duplicates. */
    for (i = 1; i < OVERicount; i++)
	if (ARTnumbers[i].ArtNum == ARTnumbers[i-1].ArtNum)
	    ARTnumbers[i-1].ArtNum = 0;

    if (!Append) {
        /* Scan through lines, collecting clumps and skipping holes. */
        ap = Refresh->Articles;
        count = Refresh->Used;
        for (i = 0, an = ARTnumbers; i < OVERicount; an++, i++) {
	    /* An already-removed article, or one that should be? */
	    if (an->ArtNum == 0)
	        continue;

	    if (Lowmark) {
		if (an->ArtNum < *ap) {
		    an->ArtNum = 0;
		}
	        continue;
	    }
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
    char	        *p, *q, *r;
    char		group[SPOOLNAMEBUFF];
    HASH		hash;

    if (List.Articles == NULL) {
	List.Size = START_LIST_SIZE;
	List.Articles = NEW(ARTNUM, List.Size);
	if (Append) {
	    p = NEW(char, START_LIST_SIZE * OVERINDEXPACKSIZE);
	    List.Index = (char (*)[OVERINDEXPACKSIZE])p;
	}
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
	    if (Append) {
		if ((p = strrchr(line, ' ')) == NULL)
		    continue;
		if (!(*line == '[' && (p - line == sizeof(HASH) * 2 + 2) && *(p-1) == ']'))
		    continue;
		*p++ = '\0';
		if ((q = strrchr(p, ':')) == NULL)
		    continue;
		*q++ = '\0';
		if (List.Used == 0) {
		    (void)strcpy(group, p);
		    List.Used = 0;
		}
		else if (!EQ(p, group)) {
		    LISTsort(&List);
		    for (r = group;*r;r++)
			if (*r == '.')
			    *r = '/';
		    RefreshLines(group, &List);
		    (void)strcpy(group, p);
		    List.Used = 0;
		}
		hash = TextToHash(++line);
		LISTappend(&List, atol(q), &hash);
	    } else {
		if ((p = strrchr(line, ':')) == NULL)
		    continue;
		*p++ = '\0';
		if (List.Used == 0) {
		    (void)strcpy(group, line);
		    List.Used = 0;
		}
		else if (!EQ(line, group)) {
		    LISTsort(&List);
		    RefreshLines(group, &List);
		    (void)strcpy(group, line);
		    List.Used = 0;
		}
		LISTappend(&List, atol(p), (HASH *)NULL);
	    }
	}

	/* Do the last group. */
	if (List.Used) {
	    if (Append)
		for (r = group;*r;r++)
		    if (*r == '.')
			*r = '/';
	    LISTsort(&List);
	    RefreshLines(group, &List);
	}
    } else if (Lowmark) {
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
	    if ((q = strrchr(line, ' ')) == NULL)
		continue;
	    *q++ = '\0';
	    for (p = line;*p;p++) {
		if (*p == '.')
		    *p = '/';
		if (*p == ' ') {
		    *p = '\0';
		    break;
		}
	    }
	    List.Articles[0] = atol(q);
	    List.Used = 1;
	    RefreshLines(line, &List);
	}
    } else {
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
	    if (Append) {
		if ((p = strrchr(line, ' ')) == NULL)
		    continue;
		if (!(*line == '[' && (p - line == sizeof(HASH) * 2 + 2) && *(p-1) == ']'))
		    continue;
		*p++ = '\0';
		if ((q = strrchr(p, ':')) == NULL)
		    continue;
		*q++ = '\0';
		for (r = p;*r;r++)
		    if (*r == '.')
			*r = '/';
		List.Used = 0;
		hash = TextToHash(++line);
		LISTappend(&List, atol(q), &hash);
		RefreshLines(p, &List);
	    } else {
		if ((p = strrchr(line, ':')) == NULL)
		    continue;
		*p++ = '\0';
		List.Articles[0] = atol(p);
		List.Used = 1;
		RefreshLines(line, &List);
	    }
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

    /* Set defaults. */
    if (ReadInnConf() < 0) exit(-1);
    SPOOL = innconf->patharticles;
    Dir = innconf->pathoverview;
    SortedInput = FALSE;
    Append = FALSE;
    Lowmark = FALSE;
    Overwrite = FALSE;
    (void)umask(NEWSUMASK);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "aD:lnoqvz")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'a':
	    Append = TRUE;
	    break;
	case 'D':
	    Dir = optarg;
	    break;
	case 'l':
	    Lowmark = TRUE;
	    break;
	case 'n':
	    DoNothing = TRUE;
	    break;
	case 'o':
	    Overwrite = TRUE;
	    Append = TRUE;
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
    if (Append && Lowmark || Lowmark && SortedInput)
	Usage();

    /* Setup. */
    if (chdir(Dir) < 0) {
	(void)fprintf(stderr, "Cant chdir to %s, %s\n", Dir, strerror(errno));
	exit(1);
    }
    InSpoolDir = EQ(Dir, SPOOL);

    OVERmmap = innconf->overviewmmap;
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

