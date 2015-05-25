/*
 * EXPIRECTL.C
 *
 * expirectl
 *
 * This program uses expire.ctl.ctl as input; please see the end of this
 * file for an example of such a file.
 */

/*
 * Date: Mon, 21 Nov 1994 12:29:52 -0801
 * From: Matthew Dillon <dillon@apollo.west.oic.com>
 * Message-Id: <199411212030.MAA21835@apollo.west.oic.com>
 * To: rsalz@uunet.uu.net
 * Subject: Re:  INN is great, bug fix for BSDI
 * 
 * [...]
 *     Oh, while I'm at it, I also wrote a cute program that builds the 
 *     expire.ctl file dynamically based on available space.   Feel free
 *     to include this in the dist (or not) as you please.
 * 
 *     Basically, the expirectl programs determines the amount of disk blocks
 *     and inodes free in the spool and creates a new expire.ctl file based
 *     on an expire.ctl.ctl template.  The template specifies expiration times
 *     as a fraction of nominal.  expirectl adjusts the nominal expiration
 *     up or down based on available disk space.
 * 
 *     The idea is to make expiration as hands off as possible.  I tested
 *     it on a smaller spool and it appeared to work fine.  Currently it
 *     only works for single-partition news spools tho.  The above spool
 *     will not really exercise the program for another 14 days or so :-).
 */

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <time.h>

/* Following portability code lifted from inndf.c */
#if HAVE_STATVFS
# include <sys/statvfs.h>
# define df_stat(p, s)  (statvfs((p), (s)) == 0)
# define df_declare(s)  struct statvfs s
# define df_total(s)    ((s).f_blocks)
# define df_avail(s)    ((s).f_bavail)
# define df_scale(s)    ((s).f_frsize == 0 ? (s).f_bsize : (s).f_frsize)
# define df_files(s)    ((s).f_files)
# define df_favail(s)   ((s).f_favail)
#elif HAVE_STATFS
# if HAVE_SYS_VFS_H
#  include <sys/vfs.h>
# endif
# if HAVE_SYS_PARAM_H
#  include <sys/param.h>
# endif
# if HAVE_SYS_MOUNT_H
#  include <sys/mount.h>
# endif
# ifdef __ultrix__
#  define df_stat(p, s) (statfs((p), (s)) >= 1)
#  define df_declare(s) struct fs_data s
#  define df_total(s)   ((s).fd_btot)
#  define df_avail(s)   ((s).fd_bfreen)
#  define df_scale(s)   1024
#  define df_files(s)   ((s).fd_gtot)
#  define df_favail(s)  ((s).fd_gfree)
# else
#  define df_stat(p, s) (statfs((p), (s)) == 0)
#  define df_declare(s) struct statfs s
#  define df_total(s)   ((s).f_blocks)
#  define df_avail(s)   ((s).f_bavail)
#  define df_scale(s)   ((s).f_bsize)
#  define df_files(s)   ((s).f_files)
#  define df_favail(s)  ((s).f_ffree)
# endif
#else
# error "Platform not supported.  Neither statvfs nor statfs available."
#endif

#define EXPIRE_CTL_DIR	"/home/news"
#define NEWS_SPOOL	"/home/news/spool/news/."

#define EXPIRE_DAYS	EXPIRE_CTL_DIR "/expire.days"
#define EXPIRE_CTL	EXPIRE_CTL_DIR "/expire.ctl"
#define EXPIRE_CTL_CTL	EXPIRE_CTL_DIR "/expire.ctl.ctl"

int
main(int ac, char **av)
{
    df_declare(sfs);
    long minFree = 100 * 1024 * 1024;
    long minIFree = 20 * 1024;
    long expireDays = 2;
    time_t expireIncTime = time(NULL) - 24 * 60 * 60;
    int modified = 0;
    int verbose = 0;

    /*
     * options
     */

    {
	int i;

	for (i = 1; i < ac; ++i) {
	    char *ptr = av[i];

	    if (*ptr == '-') {
		ptr += 2;
		switch(ptr[-1]) {
		case 'v':
		    verbose = 1;
		    break;
		case 'f':
		    modified = 1;
		    break;
		case 'n':
		    modified = -1;
		    break;
		case 'b':
		    minFree = strtol(((*ptr) ? ptr : av[++i]), &ptr, 0);
		    if (*ptr == 'k')
			minFree *= 1024;
		    if (*ptr == 'm')
			minFree *= 1024 * 1024;
		    break;
		case 'i':
		    minIFree = strtol(((*ptr) ? ptr : av[++i]), NULL, 0);
		    if (*ptr == 'k')
			minIFree *= 1024;
		    if (*ptr == 'm')
			minIFree *= 1024 * 1024;
		    break;
		default:
		    fprintf(stderr, "bad option: %s\n", ptr - 2);
		    exit(1);
		}
	    } else {
		fprintf(stderr, "bad option: %s\n", ptr);
		exit(1);
	    }
	}
    }

    if (!df_stat("/home/news/spool/news/.", &sfs)) {
	fprintf(stderr, "expirectl: couldn't fsstat /home/news/spool/news/.\n");
	exit(1);
    }

    /*
     * Load /home/news/expire.days
     */
    
    {
	FILE *fi;
	char buf[256];

	if ((fi = fopen(EXPIRE_DAYS, "r")) != NULL) {
	    while (fgets(buf, sizeof(buf), fi) != NULL) {
		if (strncmp(buf, "time", 4) == 0) {
		    expireIncTime = strtol(buf + 4, NULL, 0);
		} else if (strncmp(buf, "days", 4) == 0) {
		    expireDays = strtol(buf + 4, NULL, 0);
		}
	    }
	    fclose(fi);
	} else {
	    if (modified >= 0)
		modified = 1;
	    printf("creating %s\n", EXPIRE_DAYS);
	}
    }

    /*
     * print status
     */

    if (verbose) {
	printf("spool: %4.2lfM / %3.2lfKinode free\n",
	    (double)df_scale(sfs) * (double)df_avail(sfs) / (1024.0 * 1024.0),
	    (double)df_favail(sfs) / 1024.0
	);
	printf("decrs: %4.2lfM / %3.2lfKinode\n",
	    (double)(minFree) / (double)(1024*1024),
	    (double)(minIFree) / (double)(1024)
	);
	printf("incrs: %4.2lfM / %3.2lfKinode\n",
	    (double)(minFree * 2) / (double)(1024*1024),
	    (double)(minIFree * 2) / (double)(1024)
	);
    }

    /*
     * Check limits, update as appropriate
     */

    {
	double bytes;
	long inodes;

	bytes = (double)df_scale(sfs) * (double)df_avail(sfs);
	inodes = df_favail(sfs);

	if (bytes < (double)minFree || inodes < minIFree) {
	    if (--expireDays <= 0) {
		expireDays = 1;
		expireIncTime = time(NULL) - 24 * 60 * 60;
	    }
	    if (modified >= 0)
		modified = 1;
	    printf("decrement expiration to %ld days\n", expireDays);
	} else if (bytes >= (double)minFree * 2.0 && inodes >= minIFree * 2) {
	    long dt = (long)(time(NULL) - expireIncTime);

	    if (dt >= 60 * 60 * 24 || dt < -60) {
		++expireDays;
		expireIncTime = time(NULL);
		if (modified >= 0)
		    modified = 1;
		printf("increment expiration to %ld days\n", expireDays);
	    } else {
		printf("will increment expiration later\n");
	    }
	} else if (verbose) {
	    printf("expiration unchanged: %ld\n", expireDays);
	}
    }

    /*
     * Write EXPIRE_CTL file from EXPIRE_CTL_CTL template
     */

    if (modified > 0) {
	FILE *fi;
	FILE *fo;

	if ((fi = fopen(EXPIRE_CTL_CTL, "r")) != NULL) {
	    if ((fo = fopen(EXPIRE_CTL ".tmp", "w")) != NULL) {
		char sbuf[2048];
		char dbuf[4096];

		while (fgets(sbuf, sizeof(sbuf), fi) != NULL) {
		    char *base = sbuf;
		    char *sptr;
		    char *dptr = dbuf;

		    while ((sptr = strchr(base, '[')) != NULL) {
			double d;
			int m = 0;

			bcopy(base, dptr, sptr - base);
			dptr += sptr - base;

			d = strtod(sptr + 1, &sptr);
			if (*sptr == '/')
			    m = strtol(sptr + 1, &sptr, 0);
			if (*sptr == ']') {
			    long v = (long)((double)expireDays * d + 0.5);
			    if (v < 1)
				v = 1;
			    if (v < m)
				v = m;
			    sprintf(dptr, "%ld", v);
			    dptr += strlen(dptr);
			    ++sptr;
			}
			base = sptr;
		    }
		    strcpy(dptr, base);
		    fputs(dbuf, fo);
		}
		fclose(fo);
		if (rename(EXPIRE_CTL ".tmp", EXPIRE_CTL) != 0) {
		    fprintf(stderr, "rename(%s,%s): %s\n",
			EXPIRE_CTL ".tmp",
			EXPIRE_CTL,
			strerror(errno)
		    );
		}
	    }
	    fclose(fi);
	}
    }

    /*
     * Write EXPIRE_DAYS file
     */
    
    if (modified > 0) {
	FILE *fo;

	if ((fo = fopen(EXPIRE_DAYS, "w")) != NULL) {
	    fprintf(fo, "time 0x%08lx\n", (unsigned long) expireIncTime);
	    fprintf(fo, "days %ld\n", expireDays);
	    fclose(fo);
	} else {
	    fprintf(stderr, "unable to create %s\n", EXPIRE_DAYS);
	}
    }
    exit(0);
}


/*

# Start of sample expire.ctl.ctl file.

# EXPIRE.CTL.CTL (EXPIRE.CTL GENERATED FROM EXPIRE.CTL.CTL !!!)
#
# The expire.ctl file is generated by the expirectl program from the
# expire.ctl.ctl file.  The expirectl program calculates the proper
# expiration based on the number of free inodes and free bytes available.
#
# This file is exactly expire.ctl but with the multiplier [N] replaced by 
# a calculated value, where a multiplier of '1' nominally fills the whole
# disk.
#
# Any field [N] is substituted after being multiplied by the expiration
# time (in days).  A integer minimum can also be specified with a slash,
# as in [N/minimum].
#
# expirectl is normally run just after expire is run.  Note that expirectl
# isn't very useful for the case where you are 'catching up' on news after
# a long period of downtime UNLESS you use the -p option to expire.

/remember/:[1.2/20]

##  Keep for 1-10 days, allow Expires headers to work.
#
*:A:1:[1.0]:[6.0]
*.advocacy:A:1:[0.5]:[2.0]
alt.binaries.pictures.erotica:A:1:[0.8]:[2.0]

# permanent, semi-permanent
#
best.intro:A:never:never:never
best.announce:A:5:60:120
best.general:A:never:never:never
best.bugs:A:never:never:never

# End of sample expire.ctl.ctl file.

*/
