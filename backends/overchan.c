/*  $Revision$
**
**  Parse input to add to news overview database.
*/
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include "configdata.h"
#include "clibrary.h"
#include <errno.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include "libinn.h"
#include "macros.h"
#include "paths.h"
#include "qio.h"
#include <syslog.h>  

/*
**  Try to make one directory.  Return FALSE on error.
*/
STATIC BOOL MakeDir(char *Name)
{
    struct stat		Sb;

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
    char	        *p;
    BOOL		made;

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
**  Get the lock for the group, then open the data file and append the
**  new data.  Return FALSE on error.
*/
STATIC BOOL WriteData(char *Dir, char *Art, char *Rest, BOOL Complain)
{
    static char		TAB[] = "\t";
    static char		NL[] = "\n";
    struct iovec	iov[4];
    int	                fd;
    char		file[SPOOLNAMEBUFF];
    int	                i;
    struct stat		Sb;

    /* Name the data file. */
    (void)sprintf(file, "%s/%s", Dir, innconf->overviewname);
    /* Open and lock the file. */
    for ( ; ; ) {
	if ((fd = open(file, O_WRONLY | O_CREAT | O_APPEND, ARTFILE_MODE)) < 0) {
	    if (Complain && errno != ENOENT)
		(void)fprintf(stderr, "overchan cant open %s, %s\n",
		    file, strerror(errno));
	    return FALSE;
	}
	if (LockFile(fd, FALSE) < 0)
	    /* Wait for it. */
	    (void)LockFile(fd, TRUE);
	else {
	    /* Got the lock; make sure the file is still there. */
	    if (fstat(fd, &Sb) < 0) {
		(void)fprintf(stderr, "overchan cant fstat %s, %s\n",
		    file, strerror(errno));
		(void)close(fd);
		return FALSE;
	    }
	    if (Sb.st_nlink > 0)
		break;
	}
	/* Close file -- expireover might have removed it -- and try again. */
	(void)close(fd);
    }
    /* Build the I/O vector. */
    i = 0;
    iov[i].iov_base = Art;
    iov[i++].iov_len = strlen(Art);
    iov[i].iov_base = TAB;
    iov[i++].iov_len = 1;
    iov[i].iov_base = Rest;
    iov[i++].iov_len = strlen(Rest);
    iov[i].iov_base = NL;
    iov[i++].iov_len = 1;

    if (xwritev(fd, iov, i) < 0) {
	(void)fprintf(stderr, "overchan cant write %s %s\n",
	    file, strerror(errno));
	close(fd);
	return FALSE;
    }
    if (close(fd) < 0) {
	(void)fprintf(stderr, "overchan cant close %s %s\n",
	    file, strerror(errno));
	return FALSE;
    }
    return TRUE;
}

/*
**  Get the lock for the group, then open the data file and append the
**  new data.  Return FALSE on error.
*/
STATIC BOOL WriteUnifiedData(HASH *Hash, char *Dir, char *Art)
{
    int	                fd;
    char		file[SPOOLNAMEBUFF];
    OVERINDEX		index;
    struct stat		Sb;
    char                packed[OVERINDEXPACKSIZE];

    /* Name the data file. */
    (void)sprintf(file, "%s/%s.index", Dir, innconf->overviewname);
    /* Open and lock the file. */
    for ( ; ; ) {
	if ((fd = open(file, O_WRONLY | O_CREAT | O_APPEND, ARTFILE_MODE)) < 0) {
	    if (errno != ENOENT)
		(void)fprintf(stderr, "overchan cant open %s, %s\n",
		    file, strerror(errno));
	    return FALSE;
	}
	if (LockFile(fd, FALSE) < 0)
	    /* Wait for it. */
	    (void)LockFile(fd, TRUE);
	else {
	    /* Got the lock; make sure the file is still there. */
	    if (fstat(fd, &Sb) < 0) {
		(void)fprintf(stderr, "overchan cant fstat %s, %s\n",
		    file, strerror(errno));
		(void)close(fd);
		return FALSE;
	    }
	    if (Sb.st_nlink > 0)
		break;
	}
	/* Close file -- expireover might have removed it -- and try again. */
	(void)close(fd);
    }
    index.artnum = atol(Art);
    index.hash = *Hash;
    PackOverIndex(&index, packed);
    if (xwrite(fd, packed, OVERINDEXPACKSIZE) < 0) {
	(void)fprintf(stderr, "overchan cant write %s %s\n",
	    file, strerror(errno));
	close(fd);
	return FALSE;
    }
    if (close(fd) < 0) {
	(void)fprintf(stderr, "overchan cant close %s %s\n",
	    file, strerror(errno));
	return FALSE;
    }
    return TRUE;
}


/*
**  Process the input.  Data can come from innd:
**	news/group/name/<number> [space news/group/<number>]... \t data
**  or from mkov:
**	news/group/name \t number \t data
*/
STATIC void ProcessIncoming(QIOSTATE *qp)
{
    char	        *Xref = NULL;
    char		*OrigXref;
    char                *Data;
    char                *Dir;
    char	        *Art;
    HASH                Hash;
    char	        *p;

    for ( ; ; ) {
	/* Read the first line of data. */
	if ((Data = QIOread(qp)) == NULL) {
	    if (QIOtoolong(qp)) {
		(void)fprintf(stderr, "overchan line too long\n");
		continue;
	    }
	    break;
	}

	/* Check if we're handling a token and if so split it out from
	 * the rest of the data */
	if (innconf->storageapi) {
	    if (Data[0] == '[') {
		p = strchr(Data, ' ');
		*p = '\0';
		if (!((p - Data == sizeof(HASH) * 2 + 2) && *(p-1) == ']')) {
		    fprintf(stderr, "overchan malformed token, %s\n", Data);
		    continue;
		}
		Hash = TextToHash(&Data[1]);
		for (p++; *p == ' '; p++);
		Xref = p;
	    } else {
		fprintf(stderr, "overchan malformed token, %s\n", Data);
		continue;
	    }
	}  else {
	    /* Find the groups and article numbers. */
	    if ((Xref = strstr(Data, "Xref:")) == NULL) {
		fprintf(stderr, "overchan missing xref header\n");
		continue;
	    }
	    if ((Xref = strchr(Xref, ' ')) == NULL) {
		fprintf(stderr, "overchan malformed xref header\n");
		continue;
	    }
	    for (Xref++; *Xref == ' '; Xref++);
	    if ((Xref = strchr(Xref, ' ')) == NULL) {
		fprintf(stderr, "overchan malformed xref header\n");
		continue;
	    }
	    for (Xref++; *Xref == ' '; Xref++);
	}
	Xref = COPY(Xref);
	OrigXref = Xref; /* save pointer so we can do free() later */

	for (p = Xref; *p; p++) {
	    if (*p == '.')
		*p = '/';
	    if (*p == '\t') {
		*p = '\0';
		break;
	    }
	}


	/* Process all fields in the first part. */
	for (; *Xref; Xref = p) {

	    /* Split up this field, then split it up. */
	    for (p = Dir = Xref; *p; p++)
		if (ISWHITE(*p)) {
		    *p++ = '\0';
		    break;
		}

	    if ((Art = strrchr(Dir, ':')) == NULL || Art[1] == '\0') {
		(void)fprintf(stderr, "overchan bad entry %s\n", Dir);
		continue;
	    }
	    *Art++ = '\0';

	    /* Write data. */
	    if (innconf->storageapi) {
		if (!WriteUnifiedData(&Hash, Dir, Art) &&
		    (!MakeOverDir(Dir) || !WriteUnifiedData(&Hash, Dir, Art)))
		    (void)fprintf(stderr, "overchan cant update %s %s\n",
			Dir, strerror(errno));
	    } else {
		if (!WriteData(Dir, Art, Data, FALSE) &&
		    (!MakeOverDir(Dir) || !WriteData(Dir, Art, Data, TRUE)))
		    (void)fprintf(stderr, "overchan cant update %s %s\n",
			Dir, strerror(errno));
	    }
	}
	DISPOSE(OrigXref);
    }

    if (QIOerror(qp))
	(void)fprintf(stderr, "overchan cant read %s\n", strerror(errno));
    QIOclose(qp);
}


STATIC NORETURN Usage(void)
{
    (void)fprintf(stderr, "usage:  overchan [-c] [-D dir] [files...]\n");
    exit(1);
}


int main(int ac, char *av[])
{
    int 	        i;
    QIOSTATE		*qp;
    char		*Dir;

    /* First thing, set up logging and our identity. */
    openlog("overchan", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);           
	
    /* Set defaults. */
    if (ReadInnConf() < 0) exit(1);
    Dir = innconf->pathoverview;
    (void)umask(NEWSUMASK);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "D:")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'D':
	    Dir = optarg;
	    break;
	}
    ac -= optind;
    av += optind;

    if (chdir(Dir) < 0) {
	(void)fprintf(stderr, "overchan cant chdir %s %s\n",
		Dir, strerror(errno));
	exit(1);
    }

    if (ac == 0)
	ProcessIncoming(QIOfdopen(STDIN));
    else {
	for ( ; *av; av++)
	    if (EQ(*av, "-"))
		ProcessIncoming(QIOfdopen(STDIN));
	    else if ((qp = QIOopen(*av)) == NULL)
		(void)fprintf(stderr, "overchan cant open %s %s\n",
			*av, strerror(errno));
	    else
		ProcessIncoming(qp);
    }

    exit(0);
    /* NOTREACHED */
}
