/*  $Revision$
**
*/
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "configdata.h"
#include "clibrary.h"
#include "nnrpd.h"
#if	NNRP_LOADLIMIT > 0
#ifdef linux

/*
**  Get the current load average as an integer.
*/
int
GetLoadAverage()
{
    FILE *ProcLoadAve;
    float load;

    if ((ProcLoadAve=fopen("/proc/loadavg", "r"))==NULL)
        return -1;
    if (fscanf(ProcLoadAve,"%f", &load)!=1)
    	return -1;
    fclose(ProcLoadAve);
    return (int)(load+0.5);
}

#else 
#include <nlist.h>
STATIC struct nlist NameList[] = {
    { "_avenrun" },
#define	X_AVENRUN	0
    { NULL }
};


/*
**  Get the current load average as an integer.
*/
int
GetLoadAverage()
{
    int		fd;
    int		oerrno;
#if	defined(FSCALE)
    long	avenrun[3];
#else
    double	avenrun[3];
#endif	/* defined(FSCALE) */

    fd = open("/dev/kmem", 0, 0);
    if (fd < 0)
	return -1;

#if	defined(HPUX)
    (void)nlist("/hp-ux", NameList);
#else
#if	defined(SUNOS5)
    (void)nlist("/dev/ksyms", NameList);
#else
    (void)nlist("/vmunix", NameList);
#endif	/* defined(SUNOS5) */
#endif	/* !defined(HPUX) */
    if (NameList[0].n_type == 0
     || lseek(fd, (OFFSET_T) NameList[X_AVENRUN].n_value, SEEK_SET) == -1
     || read(fd, (char *)avenrun, sizeof avenrun) != sizeof avenrun) {
	oerrno = errno;
	(void)close(fd);
	errno = oerrno;
	return -1;
    }

    (void)close(fd);

#if	defined(FSCALE)
    return (int)(avenrun[0] + FSCALE / 2) >> FSHIFT;
#else
    return (int)(avenrun[0] + 0.5);
#endif	/* defined(FSCALE) */
}
#endif /* linux */
#endif	/* NNRP_LOADLIMIT > 0 */
