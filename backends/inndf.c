/*

inndf [-i] <filesystem>

Ian Dickinson <idickins@fore.com>
Wed Jul 26 10:11:38 BST 1995 (My birthday - 27 today!)

$Id$

Replacement for 'df | awk' in innwatch.ctl
Reports free kilobytes (not disk blocks) or free inodes.

Doesn't sync, forks less, less complicated, etc
This is non-ANSI C - K&R still lives
It should be easy to port if you have some sort of statfs() syscall

Compile with -lserver (ie. /usr/lib/libserver.a) if you run Sun's
Online DiskSuite under SunOS 4.x.  The wrapper functions there
make the system call transparent - they copy the f_spare values to
the correct spots, so f_blocks, f_bfree, f_bavail can exceed 2GB.

Compile with -DHAVE_STATVFS for these systems:
	System V Release 4.x
	Solaris 2.x
	HP-UX 10.x
	OSF1

Compile with -DHAVE_STATFS for these systems:
	SunOS 4.x/Solaris 1.x
	HP-UX 9.x
	Linux
	NeXTstep 3.x

Thanks to these folks for bug fixes and porting information:
	Mahesh Ramachandran <rr@eel.ufl.edu>
	Chuck Swiger <chuck@its.com>
	Sang-yong Suh <sysuh@kigam.re.kr>
	Swa Frantzen <Swa.Frantzen@Belgium.EU.net>
	Brad Dickey <bdickey@haverford.edu>
	Taso N. Devetzis <devetzis@snet.net>
	Wei-Yeh Lee <weiyeh@columbia.edu>
	Jeff Garzik <jeff.garzik@spinne.com>
	
Here's the relevant portion of my innwatch.ctl:

##  If load is OK, check space (and inodes) on various filesystems
##  =()<!!! inndf @<_PATH_SPOOL>@ ! lt ! @<INNWATCH_SPOOLSPACE>@ ! throttle ! No space (spool)>()=
!!! inndf /news/spool ! lt ! 18000 ! throttle ! No space (spool)
##  =()<!!! inndf @<_PATH_OVERVIEWDIR>@ ! lt ! @<INNWATCH_NOVSPACE>@ ! throttle ! No space (overview)>()=
!!! inndf /news/lib/nov ! lt ! 1000 ! throttle ! No space (overview)
##  =()<!!! inndf @<_PATH_BATCHDIR>@ ! lt ! @<INNWATCH_BATCHSPACE>@ ! throttle ! No space (newsq)>()=
!!! inndf /news/spool/out.going ! lt ! 11000 ! throttle ! No space (newsq)
##  =()<!!! inndf @<_PATH_NEWSLIB>@ ! lt ! @<INNWATCH_LIBSPACE>@ ! throttle ! No space (newslib)>()=
!!! inndf /news/lib ! lt ! 10000 ! throttle ! No space (newslib)
##  =()<!!! inndf -i @<_PATH_SPOOL>@ ! lt ! @<INNWATCH_SPOOLNODES>@ ! throttle ! No space (spool inodes)>()=
!!! inndf -i /news/spool ! lt ! 1900 ! throttle ! No space (spool inodes)

*/

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef NeXT
/* NeXT puts optind's declaration in libc.h. */
#include <libc.h>
#endif
#include <sys/types.h>
#include "config.h"
#include "configdata.h"
#include "clibrary.h"
#include "libinn.h"
#include "storage.h"
#include "paths.h"
#include "qio.h"
#include "ov.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_STATVFS
#include <sys/statvfs.h>		/* specific includes */
#define STATFUNCT	statvfs		/* function call */
#define STATSTRUC	statvfs		/* structure name */
#define STATAVAIL	f_bavail	/* blocks available */
#define STATMULTI	f_frsize	/* fragment size/block size */
#define CAREFULL_STATMULTI f_bsize /* in case f_frsize is 0 */
#define STATINODE	f_favail	/* inodes available */
#define STATTYPES	u_long		/* type of f_bavail etc */
#define STATFORMT	"%lu"		/* format string to match */
#define STATFORMTPAD	"%*lu"		/* format string to match */
#endif /* HAVE_STATVFS */

#ifdef HAVE_STATFS
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif /* HAVE_SYS_VFS_H */
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif /* HAVE_SYS_MOUNT_H */
#define STATFUNCT	statfs
#if defined (__ultrix__)
#define STATSTRUC	fs_data
#define STATAVAIL	fd_req.bfreen
#define STATMULTI	fd_req.bsize
#define STATINODE	fd_req.gfree
#define STATTYPES	u_int
#else
#define STATSTRUC	statfs
#define STATAVAIL	f_bavail
#define STATMULTI	f_bsize
#define STATINODE	f_ffree
#define STATTYPES	unsigned long
#endif
#define STATFORMT	"%lu"
#define STATFORMTPAD	"%*lu"
#endif /* HAVE_STATFS */

#define KILOBYTES	1024L

void
Printspace(char *path, BOOL inode, BOOL needpadding)
{
	struct STATSTRUC buf;
	STATTYPES value;

#if defined (__ultrix__)
	if (STATFUNCT(path, &buf) < 1) {
#else
	if (STATFUNCT(path, &buf) != 0) {
#endif
		value = 0L;	 /* if there's an error - free space is zero */
	} else {
		if (!inode) {
			/* this is often the same as just buf.f_bavail */
			/* but we want to cope with different underlying */
			/* block/fragment sizes, and avoid overflow */
			STATTYPES x=buf.STATMULTI;
#ifdef CAREFULL_STATMULTI
			if (x==0) x=buf.CAREFULL_STATMULTI;
#endif
#if defined (__ultrix__)
			value = buf.STATAVAIL;
#else
			value = (STATTYPES)
				(((double) buf.STATAVAIL * x) /
				(STATTYPES) KILOBYTES);
#endif
		} else {
			value = buf.STATINODE;

                        /* This value is compared using the shell by innwatch,
                           and some shells can't cope with anything larger
                           than the maximum value of a signed long.  ReiserFS
                           returns 2^32 - 1, however, since it has no concept
                           of inodes.  So cap the returned value at the max
                           value of a signed long. */
                        if (value > (1UL << 31) - 1)
                            value = (1UL << 31) - 1;
		}
	}

	if (needpadding)
		(void) printf(STATFORMTPAD, 10, value);
	else
		(void) printf(STATFORMT, value);
}

void
Usage(void)
{
	(void)fprintf(stderr, "Usage: inndf [-i] director{y|ies} | [-n] -o\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	BOOL inode = FALSE;
	BOOL overview = FALSE;
	BOOL numberofoverview = FALSE;
	int i, count, total = 0;
	QIOSTATE *qp;
	char *p, *q;

	while ((i = getopt(argc, argv, "ino")) != EOF) {
		switch (i) {
		default:
			Usage();
			/* not reached */
		case 'i':
			inode = TRUE;
			break;
		case 'n':
			overview = TRUE;
			numberofoverview = TRUE;
			break;
		case 'o':
			overview = TRUE;
			break;
		}
	}
	argc -= optind;
	argv += optind;
	/* This argument handling is gross */
	if (argc == 0 && overview == FALSE) {
		Usage();
		/* not reached */
	}
	if (argc == 1) {
		Printspace(argv[0], inode, FALSE);
		printf("\n");
	} else {
		for (i = 0 ; i < argc ; i++) {
			printf("%-*s ", 40, argv[i]);
			Printspace(argv[i], inode, TRUE);
			if (inode)
				printf(" inodes available\n");
			else
				printf(" Kbytes available\n");
		}
	}
	if (overview) {
		int i;
		/* Set defaults. */
		if (ReadInnConf() < 0) exit(1);

		if (!OVopen(OV_READ)) {
			printf("OVopen failed\n");
			exit(1);
		}
		if (numberofoverview) {
			if ((qp = QIOopen(cpcatpath(innconf->pathdb, _PATH_ACTIVE))) == NULL) {
				(void)fprintf(stderr, "inndf: cannot open %s\n",cpcatpath(innconf->pathdb, _PATH_ACTIVE));
				exit(1);
			}
			while ((p = QIOread(qp)) != NULL) {
				if ((q = strchr(p, ' ')) != NULL)
					*q = '\0';
				if (!OVgroupstats(p, NULL, NULL, &count, NULL))
					continue;
				total += count;
			}
			printf("%d overview data stored\n", total);
		} else {
			if (OVctl(OVSPACE, (void *)&i)) {
				if (i == -1)
					printf("%s does not support -o\n", innconf->ovmethod);
				else
					printf("%d %% overview space used\n", i);
			}
		}
	}
	exit(0);
}
