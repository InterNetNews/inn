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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_STATVFS
#include <sys/statvfs.h>		/* specific includes */
#define STATFUNCT	statvfs		/* function call */
#define STATSTRUC	statvfs		/* structure name */
#define STATAVAIL	f_bavail	/* blocks available */
#define STATMULTI	f_frsize	/* fragment size/block size */
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
#define STATSTRUC	statfs
#define STATAVAIL	f_bavail
#define STATMULTI	f_bsize
#define STATINODE	f_ffree;
#define STATTYPES	long
#define STATFORMT	"%ld"
#define STATFORMTPAD	"%*ld"
#endif /* HAVE_STATFS */

#define KILOBYTES	1024L

void
Printspace(char *path, BOOL inode, BOOL needpadding)
{
	struct STATSTRUC buf;
	STATTYPES value;

	if (STATFUNCT(path, &buf) != 0) {
		value = 0L;	 /* if there's an error - free space is zero */
	} else {
		if (!inode) {
			/* this is often the same as just buf.f_bavail */
			/* but we want to cope with different underlying */
			/* block/fragment sizes, and avoid overflow */
			value = (STATTYPES)
				(((double) buf.STATAVAIL * buf.STATMULTI) /
				(STATTYPES) KILOBYTES);
		} else {
			value = buf.STATINODE;	  /* simple! */
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
	(void)fprintf(stderr, "Usage: inndf [-i] director{y|ies}\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	BOOL inode = FALSE;
	int i;

	while ((i = getopt(argc, argv, "i")) != EOF) {
		switch (i) {
		default:
			Usage();
			/* not reached */
		case 'i':
			inode = TRUE;
			break;
		}
	}
	argc -= optind;
	argv += optind;
	/* This argument handling is gross */
	if (argc == 0) {
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
	exit(0);
}
