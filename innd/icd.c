/*  $Id$
**
**  Routines to read and write the active file.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/mmap.h"
#include <sys/uio.h>

#include "inn/innconf.h"
#include "inn/mmap.h"
#include "innd.h"
#include "inn/ov.h"

/* If we fork and exec under Cygwin, children hold onto the mmap */
/* of active, and Windows won't let us resize or replace it.     */
#ifdef __CYGWIN__
# undef HAVE_MMAP
#endif

static char		*ICDactpath = NULL;
static char		*ICDactpointer;
static int		ICDactfd;
static int		ICDactsize;


/*
**  Set and unset (or copy) IOVEC elements.  We make copies to
**  avoid problems with mmap.
*/
#ifdef HAVE_MMAP
static void
ICDiovset(struct iovec *iovp, char *base, int len)
{
    iovp->iov_len = len; 
    iovp->iov_base = xmalloc(iovp->iov_len); 
    memcpy(iovp->iov_base, base, iovp->iov_len);
}
#define ICDiovrelease(iovp)		free((iovp)->iov_base)

#else /* !HAVE_MMAP */

#define ICDiovset(iovp, base, len)	\
	(iovp)->iov_base = base, (iovp)->iov_len = len
#define ICDiovrelease(iovp)		/* NULL */

#endif /* HAVE_MMAP */


/*
**  Close the active file, releasing its resources.
*/
static void
ICDcloseactive(void)
{
    if (ICDactpointer) {
#ifdef HAVE_MMAP
	if (munmap(ICDactpointer, ICDactsize) < 0)
	    syslog(L_ERROR, "%s cant munmap %s %m", LogName, ICDactpath);
#else
	free(ICDactpointer);
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
ICDsetup(bool StartSites)
{
    if (ICDneedsetup == true) {
	ICDneedsetup = false;
    }
    else {
	ICDcloseactive();
	NGparsefile();
    }
    if (NGfind("control") == NULL || NGfind("junk") == NULL) {
	syslog(L_FATAL, "%s internal no control and/or junk group", LogName);
	exit(1);
    }
    if (NGfind("control.cancel") == NULL) {
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
ICDwrite(void)
{
    HISsync(History);
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
ICDclose(void)
{
    ICDwrite();
    ICDcloseactive();
}


/*
**  Scan the active file, and renumber the min/max counts.
*/
bool
ICDrenumberactive(void)
{
    int	i;
    NEWSGROUP	*ngp;

    for (i = nGroups, ngp = Groups; --i >= 0; ngp++)
	if (!NGrenumber(ngp))
	    return false;
    if (i < 0)
	ICDwrite();
    return true;
}


/*
**  Use writev() to replace the active file.
*/
static bool
ICDwritevactive(struct iovec *vp, int vpcount)
{
    static char		*BACKUP = NULL;
    static char         *NEWACT = NULL;
    static char		WHEN[] = "backup active";
    int	                fd;
    int			oerrno;
#ifdef __CYGWIN__
    size_t		newactsize, padactsize, wrote;
    struct iovec	*newvp;
    char		*filler;
    int			i;
#endif

    if (BACKUP == NULL)
	BACKUP = concatpath(innconf->pathdb, INN_PATH_OLDACTIVE);
    if (NEWACT == NULL)
	NEWACT = concatpath(innconf->pathdb, INN_PATH_NEWACTIVE);
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
	close(fd);
    }
    else if (close(fd) < 0) {
	oerrno = errno;
	syslog(L_ERROR, "%s cant close %s %m", LogName, BACKUP);
	IOError(WHEN, oerrno);
    }

#ifdef __CYGWIN__
    /* If we are shrinking active, junk will be at the end between the */
    /* writev and ftruncate.  Clobber it with values that overview and */
    /* nnrpd can ignore. */
    for (newactsize = 0, i = 0; i < vpcount; i++)
	 newactsize += vp[i].iov_len;
    if (newactsize < ICDactsize) {
	 padactsize = ICDactsize - newactsize;
	 newvp = xmalloc((vpcount + 1) * sizeof(struct iovec));
	 for (i = 0; i < vpcount; i++)
	      newvp[i] = vp[i];
	 filler = xcalloc(padactsize, 1);
	 *filler = '.';
	 filler[padactsize - 1] = '\n';
	 newvp[vpcount].iov_base = filler;
	 newvp[vpcount].iov_len = padactsize;
	 vpcount++;
    }
    else {
	 padactsize = 0;
	 newvp = vp;
    }
    oerrno = 0;
    if (lseek(ICDactfd, 0, SEEK_SET) == -1) {
        oerrno = errno;
	syslog(L_ERROR, "%s cant rewind %s %m", LogName, ICDactpath);
	IOError(WHEN, oerrno);
	goto bailout;
    }
    if (xwritev(ICDactfd, newvp, vpcount) < 0) {
	oerrno = errno;
	syslog(L_ERROR, "%s cant write %s %m", LogName, ICDactpath);
	IOError(WHEN, oerrno);
	goto bailout;
    }
    if (newactsize < ICDactsize && ftruncate(ICDactfd, newactsize) != 0) {
	oerrno = errno;
	syslog(L_ERROR, "%s cant truncate %s", LogName, ICDactpath);
    }

bailout:
    if (padactsize != 0) {
	 free(filler);
	 free(newvp);
    }
    if (oerrno != 0)
	 return false;

#else /* !__CYGWIN__, do it the Unix way. */

    /* Open the active file. */
    fd = open(NEWACT, O_WRONLY | O_TRUNC | O_CREAT, ARTFILE_MODE);
    if (fd < 0) {
	oerrno = errno;
	syslog(L_ERROR, "%s cant open %s %m", LogName, NEWACT);
	IOError(WHEN, oerrno);
	return false;
    }

    /* Write it. */
    if (xwritev(fd, vp, vpcount) < 0) {
	oerrno = errno;
	syslog(L_ERROR, "%s cant write %s %m", LogName, NEWACT);
	IOError(WHEN, oerrno);
	close(fd);
	return false;
    }

    /* Close it. */
    close(fd);

    /* Rename it to be the canonical active file */
    if (rename(NEWACT, ICDactpath) < 0) {
	oerrno = errno;
	syslog(L_ERROR, "%s cant rename %s to %s %m",
	       LogName, NEWACT, ICDactpath);
	IOError(WHEN, oerrno);
	return false;
    }

#endif /* __CYGWIN__ */

    /* Invalidate in-core pointers. */
    ICDcloseactive();

    /* Restore in-core pointers. */
    if (Mode != OMrunning) {
	ICDneedsetup = true;
	/* Force the active file into memory. */
	NGparsefile();
    }
    else
	ICDsetup(true);
    return true;
}


/*
**  Change the flag on a newsgroup.  Fairly easy.
*/
bool
ICDchangegroup(NEWSGROUP *ngp, char *Rest)
{
    static char		NEWLINE[] = "\n";
    int                 i;
    struct iovec	iov[3];
    bool		ret;
    char		*Name;
    long		Last;

    /* Set up the scatter/gather vectors. */
    ICDiovset(&iov[0], ICDactpointer, ngp->Rest - ICDactpointer);
    ICDiovset(&iov[1], Rest, strlen(Rest));
    Name = xstrdup(ngp->Name);
    Last = ngp->Last;
    if (++ngp < &Groups[nGroups]) {
	/* Not the last group, keep the \n from the next line. */
	i = ngp->Start;
	ICDiovset(&iov[2], &ICDactpointer[i - 1], ICDactsize - i + 1);
    }
    else {
	/* Last group -- append a newline. */
	ICDiovset(&iov[2], NEWLINE, strlen(NEWLINE));
    }
    ret = ICDwritevactive(iov, 3);
    ICDiovrelease(&iov[0]);
    ICDiovrelease(&iov[1]);
    ICDiovrelease(&iov[2]);

    if (ret) {
	if (innconf->enableoverview && !OVgroupadd(Name, 0, Last, Rest)) {
	    free(Name);
	    return false;
	}
    }
    free(Name);
    return ret;
}


/*
**  Add a newsgroup.  Append a line to the end of the active file and reload.
*/
bool
ICDnewgroup(char *Name, char *Rest)
{
    char		buff[SMBUF];
    struct iovec	iov[2];
    bool		ret;

    /* Set up the scatter/gather vectors. */
    if (strlen(Name) + strlen(Rest) > SMBUF - 24) {
	syslog(L_ERROR, "%s too_long %s", LogName, MaxLength(Name, Name));
	return false;
    }
    snprintf(buff, sizeof(buff), "%s 0000000000 0000000001 %s\n", Name, Rest);
    ICDiovset(&iov[0], ICDactpointer, ICDactsize);
    ICDiovset(&iov[1], buff, strlen(buff));

    ret = ICDwritevactive(iov, 2);
    ICDiovrelease(&iov[0]);
    ICDiovrelease(&iov[1]);
    if (ret) {
	if (innconf->enableoverview && !OVgroupadd(Name, 1, 0, Rest))
	    return false;
    }
    return ret;
}


/*
**  Remove a newsgroup.  Splice the line out of the active file and reload.
*/
bool
ICDrmgroup(NEWSGROUP *ngp)
{
    struct iovec iov[2];
    int i;
    bool ret;
    char *Name;

    /* Don't let anyone remove newsgroups that INN requires exist. */
    if (strcmp(ngp->Name, "junk") == 0 || strcmp(ngp->Name, "control") == 0)
        return false;
    if (innconf->mergetogroups && strcmp(ngp->Name, "to") == 0)
        return false;

    Name = xstrdup(ngp->Name);
    /* If this is the first group in the file, write everything after. */
    if (ngp == &Groups[0]) {
	i = ngp[1].Start;
	ICDiovset(&iov[0], &ICDactpointer[i], ICDactsize - i);
	ret = ICDwritevactive(iov, 1);
	ICDiovrelease(&iov[0]);
	if (ret) {
	    if (innconf->enableoverview && !OVgroupdel(Name)) {
		free(Name);
		return false;
	    }
	}
	free(Name);
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
		free(Name);
		return false;
	    }
	}
	free(Name);
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
	    free(Name);
	    return false;
	}
    }
    free(Name);
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
	ICDactpath = concatpath(innconf->pathdb, INN_PATH_ACTIVE);
    if ((ICDactfd = open(ICDactpath, O_RDWR)) < 0) {
	syslog(L_FATAL, "%s cant open %s %m", LogName, ICDactpath);
	exit(1);
    }
    close_on_exec(ICDactfd, true);

#ifdef HAVE_MMAP

    if (fstat(ICDactfd, &Sb) < 0) {
	syslog(L_FATAL, "%s cant fstat %d %s %m",
	    LogName, ICDactfd, ICDactpath);
	exit(1);
    }
    ICDactsize = Sb.st_size;
    ICDactpointer = mmap(NULL, ICDactsize, PROT_READ|PROT_WRITE,
                         MAP_SHARED, ICDactfd, 0);
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
ICDwriteactive(void)
{
#ifdef HAVE_MMAP
    if (msync_page(ICDactpointer, ICDactsize, MS_ASYNC) < 0) {
        syslog(L_FATAL, "%s msync failed %s 0x%lx %d %m", LogName, ICDactpath, (unsigned long) ICDactpointer, ICDactsize);
        exit(1);
    }
#else /* !HAVE_MMAP */
    if (lseek(ICDactfd, 0, SEEK_SET) == -1) {
	syslog(L_FATAL, "%s cant rewind %s %m", LogName, ICDactpath);
	exit(1);
    }
    if (xwrite(ICDactfd, ICDactpointer, ICDactsize) < 0) {
	syslog(L_FATAL, "%s cant write %s %m", LogName, ICDactpath);
	exit(1);
    }
#endif /* HAVE_MMAP */
}
