/*  $Id$
**
**  Reports free kilobytes (not disk blocks) or free inodes.
**
**  Written by Ian Dickinson <idickins@fore.com>
**  Wed Jul 26 10:11:38 BST 1995 (My birthday - 27 today!)
**
**  inndf is a replacement for 'df | awk' in innwatch.ctl and for reporting
**  free space in other INN scripts.  It doesn't sync, it forks less, and
**  it's generally less complicated.
**
**  Usage: inndf [-i] <directory> [<directory> ...]
**         inndf -n
**         inndf -o
**
**  Compile with -lserver (ie. /usr/lib/libserver.a) if you run Sun's Online
**  DiskSuite under SunOS 4.x.  The wrapper functions there make the system
**  call transparent; they copy the f_spare values to the correct spots, so
**  f_blocks, f_bfree, f_bavail can exceed 2GB.
**
**  Compile with -DHAVE_STATVFS for these systems:
**          System V Release 4.x
**          Solaris 2.x
**          HP-UX 10.x
**          OSF1
**  
**  Compile with -DHAVE_STATFS for these systems:
**          SunOS 4.x/Solaris 1.x
**          HP-UX 9.x
**          Linux
**          NeXTstep 3.x
**
**  (Or even better, let autoconf take care of it.)
**  
**  Thanks to these folks for bug fixes and porting information:
**          Mahesh Ramachandran <rr@eel.ufl.edu>
**          Chuck Swiger <chuck@its.com>
**          Sang-yong Suh <sysuh@kigam.re.kr>
**          Swa Frantzen <Swa.Frantzen@Belgium.EU.net>
**          Brad Dickey <bdickey@haverford.edu>
**          Taso N. Devetzis <devetzis@snet.net>
**          Wei-Yeh Lee <weiyeh@columbia.edu>
**          Jeff Garzik <jeff.garzik@spinne.com>
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/qio.h"
#include "libinn.h"
#include "ov.h"
#include "paths.h"

/* The portability mess.  Hide everything in macros so that the actual code
   is relatively clean.  SysV uses statvfs, BSD uses statfs, and ULTRIX is
   just weird (and isn't worth checking for in configure).

   df_declare declares a variable of the appropriate type to pass to df_stat
   along with a path; df_stat will return true on success, false on failure.
   df_avail gives the number of free blocks, the size of those blocks given
   in df_bsize (which handles SysV's weird fragment vs. preferred block size
   thing).  df_inodes returns the free inodes. */
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

static const char usage[] = "\
Usage: inndf [-i] [-f filename] [-F] <directory> [<directory> ...]\n\
       inndf -n\n\
       inndf -o\n\
\n\
The first form gives the free space in kilobytes (or the count of free\n\
inodes if -i is given) in the file systems given by the arguments.  If\n\
-f is given, the corresponding file should be a list of directories to\n\
check in addition to the arguments.  -F uses <pathetc>/filesystems as the\n\
file and is otherwise the same.\n\
\n\
The second form gives the total count of overview records stored.  The\n\
third form gives the percentage space allocated to overview that's been\n\
used (if the overview method used supports this query).";

/*
**  Given a path, a flag saying whether to look at inodes instead of free
**  disk space, and a flag saying whether to format in columns, print out
**  the amount of free space or inodes on that file system.  Returns the
**  percentage free, which may be printed out by the caller.
*/
static void
printspace(const char *path, bool inode, bool fancy)
{
    df_declare(info);
    unsigned long amount;
    double percent;

    if (df_stat(path, &info)) {
        if (inode) {
            amount = df_favail(info);

            /* This value is compared using the shell by innwatch, and some
               shells can't cope with anything larger than the maximum value
               of a signed long.  ReiserFS returns 2^32 - 1, however, since it
               has no concept of inodes.  So cap the returned value at the max
               value of a signed long. */
            if (amount > (1UL << 31) - 1)
                amount = (1UL << 31) - 1;

	    /* 2.6 kernels show 0 available and used inodes, instead. */
	    if (amount == 0 && df_files(info) == 0)
		amount = (1UL << 31) - 1;
        } else {
            /* Do the multiplication in floating point to try to retain
               accuracy if the free space in bytes would overflow an
               unsigned long.  This should be safe until file systems larger
               than 4TB (which may not be much longer -- we should use long
               long instead if we have it).

               Be very careful about the order of casts here; it's too
               easy to cast back into an unsigned long a value that
               overflows, and one then gets silently wrong results. */
            amount = (unsigned long)
                (((double) df_avail(info) * df_scale(info)) / 1024.0);
        }
    } else {
        /* On error, free space is zero. */
        amount = 0;
    }
    printf(fancy ? "%10lu" : "%lu", amount);
    if (fancy) {
        printf(inode ? " inodes available " : " Kbytes available ");
        if (inode)
            percent = 100 * ((double) df_favail(info) / df_files(info));
        else
            percent = 100 * ((double) df_avail(info) / df_total(info));
        if (percent < 9.95)
            printf("  (%3.1f%%)", percent);
        else if (percent < 99.95)
            printf(" (%4.1f%%)", percent);
        else
            printf("(%5.1f%%)", percent);
    }
}

static void
printspace_formatted(const char *path, bool inode)
{
    printf("%-40s ", path);
    printspace(path, inode, true);
    printf("\n");
}

static char *
readline(QIOSTATE *qp)
{
    char *line, *p;

    for (line = QIOread(qp); line != NULL; line = QIOread(qp)) {
        p = strchr(line, '#');
        if (p != NULL)
            *p = '\0';
        for (; *line == ' ' || *line == '\t'; line++)
            ;
        if (*line != '\0') {
            for (p = line; *p != '\0' && *p != ' ' && *p != '\t'; p++)
                ;
            *p = '\0';
            return line;
        }
    }
    return NULL;
}

int
main(int argc, char *argv[])
{
    int option, i, count;
    unsigned long total;
    QIOSTATE *qp;
    char *active, *group, *line, *p;
    char *file = NULL;
    bool inode = false;
    bool overview = false;
    bool ovcount = false;
    bool use_filesystems = false;

    while ((option = getopt(argc, argv, "hinof:F")) != EOF) {
        switch (option) {
        default:
            die(usage);
        case 'h':
            printf("%s\n", usage);
            exit(0);
        case 'i':
            inode = true;
            break;
        case 'n':
            ovcount = true;
            break;
        case 'o':
            overview = true;
            break;
        case 'f':
            if (file != NULL)
                die("inndf: Only one of -f or -F may be given");
            file = xstrdup(optarg);
            break;
        case 'F':
            if (file != NULL)
                die("inndf: Only one of -f or -F may be given");
            if (!innconf_read(NULL))
                exit(1);
            file = concatpath(innconf->pathetc, INN_PATH_FILESYSTEMS);
            use_filesystems = true;
            break;
        }
    }
    argc -= optind;
    argv += optind;

    if (argc == 0 && !overview && !ovcount && file == NULL)
        die(usage);

    /* Set the program name now rather than earlier so that it doesn't get
       prepended to usage messages. */
    message_program_name = "inndf";

    /* If directories were specified, get statistics about them.  If only
       one was given, just print out the number without the path or any
       explanatory text; this mode is used by e.g. innwatch.  Otherwise,
       format things nicely. */
    if (argc == 1 && !overview && !ovcount && file == NULL) {
        printspace(argv[0], inode, false);
        printf("\n");
    } else {
        for (i = 0; i < argc; i++)
            printspace_formatted(argv[i], inode);
        if (file != NULL) {
            qp = QIOopen(file);
            if (qp == NULL) {
                if (!use_filesystems)
                    sysdie("can't open %s", file);
            } else {
                line = readline(qp);
                while (line != NULL) {
                    printspace_formatted(line, inode);
                    line = readline(qp);
                }
                QIOclose(qp);
            }
            free(file);
        }
    }

    /* If we're going to be getting information from overview, do the icky
       initialization stuff. */
    if (overview || ovcount) {
        if (!use_filesystems)
            if (!innconf_read(NULL))
                exit(1);
        if (!OVopen(OV_READ))
            die("OVopen failed");
    }

    /* For the count, we have to troll through the active file and query the
       overview backend for each group. */
    if (ovcount) {
        active = concatpath(innconf->pathdb, _PATH_ACTIVE);
        qp = QIOopen(active);
        if (qp == NULL)
            sysdie("can't open %s", active);

        total = 0;
        group = QIOread(qp);
        while (group != NULL) {
            p = strchr(group, ' ');
            if (p != NULL)
                *p = '\0';
            if (!OVgroupstats(group, NULL, NULL, &count, NULL))
                continue;
            total += count;
            group = QIOread(qp);
        }
        QIOclose(qp);
        printf("%lu overview records stored\n", total);
    }

    /* Percentage used is simpler, but only some overview methods understand
       that query. */
    if (overview) {
        if (OVctl(OVSPACE, &count)) {
            if (count == -1)
                printf("Space used is meaningless for the %s method\n",
                       innconf->ovmethod);
            else
                printf("%d%% overview space used\n", count);
        }
    }
    exit(0);
}
