/*  $Id$
**
*/

#include "config.h"
#include "clibrary.h"

#include "nnrpd.h"

#if	NNRP_LOADLIMIT > 0
#ifdef linux
#ifdef HAVE_SYS_SYSINFO_H
#include <sys/sysinfo.h>
#else
/* not all linuces have sys/sysinfo.h -- egil@kvaleberg.no */
#include <linux/kernel.h>
#endif

/*
**  Get the current load average as an integer.
*/
int
GetLoadAverage(void)
{
    struct sysinfo si;

    if (sysinfo (&si))
	return -1;

    return (si.loads[0] + (1 << 15)) >> 16;
}

#else 
#include <nlist.h>
static struct nlist NameList[] = {
    { "_avenrun" },
#define	X_AVENRUN	0
    { NULL }
};


/*
**  Get the current load average as an integer.
*/
int
GetLoadAverage(void)
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

#if	defined(_HPUX_SOURCE)
    (void)nlist("/hp-ux", NameList);
#else
#if	defined(SUNOS5)
    (void)nlist("/dev/ksyms", NameList);
#else
    (void)nlist("/vmunix", NameList);
#endif	/* defined(SUNOS5) */
#endif	/* !defined(HPUX) */
    if (NameList[0].n_type == 0
     || lseek(fd, NameList[X_AVENRUN].n_value, SEEK_SET) == -1
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
