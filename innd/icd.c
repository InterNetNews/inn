/*  $Id$
**
**  Routines to read and write the active file.
*/
#include "config.h"
#include "clibrary.h"
#include <netinet/in.h>
#include <sys/uio.h>

#include "innd.h"
#include "ov.h"

#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif

typedef struct iovec	IOVEC;


STATIC char		*ICDactpath = NULL;
STATIC char		*ICDactpointer;
STATIC int		ICDactfd;
STATIC int		ICDactsize;


/*
**  Set and unset (or copy) IOVEC elements.  We make copies to
**  avoid problems with mmap.
*/
#ifdef HAVE_MMAP
void ICDiovset(IOVEC *iovp, char *base, int len) {
    (iovp)->iov_len = len; 
    (iovp)->iov_base = NEW(char, (iovp)->iov_len); 
    (void)memcpy((POINTER)(iovp)->iov_base, (POINTER)base, 
	    (SIZE_T)(iovp)->iov_len);
}
#define ICDiovrelease(iovp)		DISPOSE((iovp)->iov_base)

#else /* !HAVE_MMAP */

#define ICDiovset(iovp, base, len)	\
	(iovp)->iov_base = base, (iovp)->iov_len = len
#define ICDiovrelease(iovp)		/* NULL */

#endif /* HAVE_MMAP */


/*
**  Close the active file, releasing its resources.
*/
STATIC void
ICDcloseactive()
{
    if (ICDactpointer) {
#ifdef HAVE_MMAP
	if (munmap(ICDactpointer, ICDactsize) < 0)
	    syslog(L_ERROR, "%s cant munmap %s %m", LogName, ICDactpath);
#else
	DISPOSE(ICDactpointer);
#endif
	ICDactpointer = NULL;
	if (close(ICDactfd) < 0) {
	    syslog(L_FATAL, "%s cant close %s %m", LogName, ICDactpath);
	    exit(1);
	}
    }
}


/*
**  Set up the hash and in-core tables.
*/
void
ICDsetup(StartSites)
    BOOL	StartSites;
{
    if (ICDneedsetup == TRUE) {
	ICDneedsetup = FALSE;
    }
    else {
	ICDcloseactive();
	NGparsefile();
    }
    if (NGfind("control") == NULL || NGfind("junk") == NULL) {
	syslog(L_FATAL, "%s internal no control and/or junk group", LogName);
	exit(1);
    }
    if (innconf->usecontrolchan && NGfind("control.cancel") == NULL) {
	syslog(L_FATAL, "%s internal no control.cancel group", LogName);
	exit(1);
    }
    if (innconf->mergetogroups && NGfind("to") == NULL) {
	syslog(L_FATAL, "%s internal no to group", LogName);
	exit(1);
    }
    SITEparsefile(StartSites);
}


/*
**  Write out all in-core data.
*/
void
ICDwrite()
{
    HISsync();
    SMflushcacheddata(SM_ALL);

    if (ICDactivedirty) {
	ICDwriteactive();
	ICDactivedirty = 0;
    }

    /* Flush log and error log. */
    if (fflush(Log) == EOF)
	syslog(L_ERROR, "%s cant fflush log %m", LogName);
    if (fflush(Errlog) == EOF)
	syslog(L_ERROR, "%s cant fflush errlog %m", LogName);
}


/*
**  Close things down.
*/
void
ICDclose()
{
    ICDwrite();
    ICDcloseactive();
}


/*
**  Scan the active file, and renumber the min/max counts.
*/
BOOL
ICDrenumberactive()
{
    register int	i;
    register NEWSGROUP	*ngp;

    for (i = nGroups, ngp = Groups; --i >= 0; ngp++)
	if (!NGrenumber(ngp))
	    return FALSE;
    if (i < 0)
	ICDwrite();
    return TRUE;
}


/*
**  Use writev() to replace the active file.
*/
STATIC BOOL ICDwritevactive(IOVEC *vp, int vpcount)
{
    static char		*BACKUP = NULL;
    static char         *NEWACT = NULL;
    static char		WHEN[] = "backup active";
    int	                fd;
    int			oerrno;

    if (BACKUP == NULL)
	BACKUP = COPY(cpcatpath(innconf->pathdb, _PATH_OLDACTIVE));
    if (NEWACT == NULL)
	NEWACT = COPY(cpcatpath(innconf->pathdb, _PATH_NEWACTIVE));
    /* Write the current file to a backup. */
    if (unlink(BACKUP) < 0 && errno != ENOENT) {
	oerrno = errno;
	syslog(L_ERROR, "%s cant unlink %s %m", LogName, BACKUP);
	IOError(WHEN, oerrno);
    }
    if ((fd = open(BACKUP, O_WRONLY | O_TRUNC | O_CREAT, 0664)) < 0) {
	oerrno = errno;
	syslog(L_ERROR, "%s cant open %s %m", LogName, BACKUP);
	IOError(WHEN, oerrno);
    }
    else if (xwrite(fd, ICDactpointer, ICDactsize) < 0) {
	oerrno = errno;
	syslog(L_ERROR, "%s cant write %s %m", LogName, BACKUP);
	IOError(WHEN, oerrno);
	(void)close(fd);
    }
    else if (close(fd) < 0) {
	oerrno = errno;
	syslog(L_ERROR, "%s cant close %s %m", LogName, BACKUP);
	IOError(WHEN, oerrno);
    }

    /* Open the active file. */
    fd = open(NEWACT, O_WRONLY | O_TRUNC | O_CREAT, ARTFILE_MODE);
    if (fd < 0) {
	oerrno = errno;
	syslog(L_ERROR, "%s cant open %s %m", LogName, NEWACT);
	IOError(WHEN, oerrno);
	return FALSE;
    }

    /* Write it. */
    if (xwritev(fd, vp, vpcount) < 0) {
	oerrno = errno;
	syslog(L_ERROR, "%s cant write %s %m", LogName, NEWACT);
	IOError(WHEN, oerrno);
	(void)close(fd);
	return FALSE;
    }

    /* Close it. */
    (void)close(fd);
    if (AmRoot)
	xchown(NEWACT);

    /* Rename it to be the canonical active file */
    if (rename(NEWACT, ICDactpath) < 0) {
	oerrno = errno;
	syslog(L_ERROR, "%s cant rename %s to %s %m",
	       LogName, NEWACT, ICDactpath);
	IOError(WHEN, oerrno);
	return FALSE;
    }

    /* Invalidate in-core pointers. */
    ICDcloseactive();

    /* Restore in-core pointers. */
    if (Mode != OMrunning) {
	ICDneedsetup = TRUE;
	/* Force the active file into memory. */
	NGparsefile();
    }
    else
	ICDsetup(TRUE);
    return TRUE;
}


/*
**  Change the flag on a newsgroup.  Fairly easy.
*/
BOOL ICDchangegroup(NEWSGROUP *ngp, char *Rest)
{
    static char		NEWLINE[] = "\n";
    register int	i;
    IOVEC		iov[3];
    BOOL		ret;
    char		*Name;
    long		Last;

    /* Set up the scatter/gather vectors. */
    ICDiovset(&iov[0], ICDactpointer, ngp->Rest - ICDactpointer);
    ICDiovset(&iov[1], Rest, strlen(Rest));
    Name = COPY(ngp->Name);
    Last = ngp->Last;
    if (++ngp < &Groups[nGroups]) {
	/* Not the last group, keep the \n from the next line. */
	i = ngp->Start;
	ICDiovset(&iov[2], &ICDactpointer[i - 1], ICDactsize - i + 1);
    }
    else {
	/* Last group -- append a newline. */
	ICDiovset(&iov[2], NEWLINE, STRLEN(NEWLINE));
    }
    ret = ICDwritevactive(iov, 3);
    ICDiovrelease(&iov[0]);
    ICDiovrelease(&iov[1]);
    ICDiovrelease(&iov[2]);

    if (ret) {
	if (innconf->enableoverview && !OVgroupadd(Name, 0, Last, Rest)) {
	    DISPOSE(Name);
	    return FALSE;
	}
    }
    DISPOSE(Name);
    return ret;
}


/*
**  Add a newsgroup.  Append a line to the end of the active file and reload.
*/
BOOL ICDnewgroup(char *Name, char *Rest)
{
    char		buff[SMBUF];
    IOVEC		iov[2];
    BOOL		ret;

    /* Set up the scatter/gather vectors. */
    if (strlen(Name) + strlen(Rest) > (SIZE_T)(SMBUF - 24)) {
	syslog(L_ERROR, "%s too_long %s", LogName, MaxLength(Name, Name));
	return FALSE;
    }
    (void)sprintf(buff, "%s 0000000000 0000000001 %s\n", Name, Rest);
    ICDiovset(&iov[0], ICDactpointer, ICDactsize);
    ICDiovset(&iov[1], buff, strlen(buff));

    ret = ICDwritevactive(iov, 2);
    ICDiovrelease(&iov[0]);
    ICDiovrelease(&iov[1]);
    if (ret) {
	if (innconf->enableoverview && !OVgroupadd(Name, 1, 0, Rest))
	    return FALSE;
    }
    return ret;
}


/*
**  Remove a newsgroup.  Splice the line out of the active file and reload.
*/
BOOL ICDrmgroup(NEWSGROUP *ngp)
{
    IOVEC	iov[2];
    int		i;
    BOOL	ret;
    char	*Name;

    Name = COPY(ngp->Name);
    /* If this is the first group in the file, write everything after. */
    if (ngp == &Groups[0]) {
	i = ngp[1].Start;
	ICDiovset(&iov[0], &ICDactpointer[i], ICDactsize - i);
	ret = ICDwritevactive(iov, 1);
	ICDiovrelease(&iov[0]);
	if (ret) {
	    if (innconf->enableoverview && !OVgroupdel(Name)) {
		DISPOSE(Name);
		return FALSE;
	    }
	}
	DISPOSE(Name);
	return ret;
    }

    /* Write everything up to this group. */
    ICDiovset(&iov[0], ICDactpointer, ngp->Start);

    /* If this is the last group, that's all we have to write. */
    if (ngp == &Groups[nGroups - 1]) {
	ret = ICDwritevactive(iov, 1);
	ICDiovrelease(&iov[0]);
	if (ret) {
	    if (innconf->enableoverview && !OVgroupdel(Name)) {
		DISPOSE(Name);
		return FALSE;
	    }
	}
	DISPOSE(Name);
	return ret;
    }

    /* Write everything after this group. */
    i = ngp[1].Start;
    ICDiovset(&iov[1], &ICDactpointer[i], ICDactsize - i);
    ret = ICDwritevactive(iov, 2);
    ICDiovrelease(&iov[0]);
    ICDiovrelease(&iov[1]);
    if (ret) {
	if (innconf->enableoverview && !OVgroupdel(Name)) {
	    DISPOSE(Name);
	    return FALSE;
	}
    }
    DISPOSE(Name);
    return ret;
}



/*
**  Open the active file and "map" it into memory.
*/
char *
ICDreadactive(endp)
    char		**endp;
{
    struct stat		Sb;

    if (ICDactpointer) {
	*endp = ICDactpointer + ICDactsize;
	return ICDactpointer;
    }
    if (ICDactpath == NULL) 
	ICDactpath = COPY(cpcatpath(innconf->pathdb, _PATH_ACTIVE));
    if ((ICDactfd = open(ICDactpath, O_RDWR)) < 0) {
	syslog(L_FATAL, "%s cant open %s %m", LogName, ICDactpath);
	exit(1);
    }
    CloseOnExec(ICDactfd, TRUE);

#ifdef HAVE_MMAP

    if (fstat(ICDactfd, &Sb) < 0) {
	syslog(L_FATAL, "%s cant fstat %d %s %m",
	    LogName, ICDactfd, ICDactpath);
	exit(1);
    }
    ICDactsize = Sb.st_size;
    ICDactpointer = mmap((caddr_t)0, ICDactsize, PROT_READ|PROT_WRITE,
			MAP__ARG, ICDactfd, (OFFSET_T)0);
    if (ICDactpointer == (char *)-1) {
	syslog(L_FATAL, "%s cant mmap %d %s %m",
	    LogName, ICDactfd, ICDactpath);
	exit(1);
    }

#else /* !HAVE_MMAP */

    if ((ICDactpointer = ReadInDescriptor(ICDactfd, &Sb)) == NULL) {
	syslog(L_FATAL, "%s cant read %s %m", LogName, ICDactpath);
	exit(1);
    }
    ICDactsize = Sb.st_size;

#endif /* HAVE_MMAP */

    *endp = ICDactpointer + ICDactsize;
    return ICDactpointer;
}


/*
**  Write the active file out.
*/
void
ICDwriteactive()
{
#ifdef HAVE_MMAP
# ifdef MMAP_NEEDS_MSYNC
#  ifdef HAVE_MSYNC_3_ARG
    if (msync(ICDactpointer, ICDactsize, MS_ASYNC) < 0) {
        syslog(L_FATAL, "%s msync failed %s %m", LogName, ICDactpath);
        exit(1);
    }
#  else /* !HAVE_MSYNC_3_ARG */
    if (msync(ICDactpointer, ICDactsize) < 0) {
        syslog(L_FATAL, "%s msync failed %s %m", LogName, ICDactpath);
        exit(1);
    }
#  endif /* HAVE_MSYNC_3_ARG */
# endif /* MMAP_NEEDS_MSYNC */

#else /* !HAVE_MMAP */

    if (lseek(ICDactfd, (OFFSET_T)0, SEEK_SET) == -1) {
	syslog(L_FATAL, "%s cant rewind %s %m", LogName, ICDactpath);
	exit(1);
    }
    if (xwrite(ICDactfd, ICDactpointer, ICDactsize) < 0) {
	syslog(L_FATAL, "%s cant write %s %m", LogName, ICDactpath);
	exit(1);
    }
#endif /* HAVE_MMAP */
}
