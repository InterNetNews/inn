/*  $Revision$
**
**  Parse input to add links for cross posted articles
**  Input format is one line per article.  Dots '.' are changed to 
**  '/'.  Commas ',' or blanks ' ' separate entries.  Typically this
**  is via a channel feed from innd though an edit of the history file
**  can also be used for recovery purposes.  Sample newsfeeds entry:
**
**	# Create the links for cross posted articles
**	crosspost:*:Tc,Ap,WR:/usr/local/newsbin/crosspost
**  
*/
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include "configdata.h"
#if     defined(DO_NEED_TIME)
#include <time.h>
#endif  /* defined(DO_NEED_TIME) */
#include <syslog.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include "clibrary.h"
#include "logging.h"
#include "libinn.h"
#include "macros.h"
#include "paths.h"
#include "qio.h"


STATIC BOOL	InSpoolDir;
STATIC char	*Dir;

STATIC int	debug = FALSE;
STATIC int	syncfiles = TRUE;

STATIC unsigned long STATTime    = 0;
STATIC unsigned long STATMissing = 0; /* Source file missing */
STATIC unsigned long STATTooLong = 0; /* Name Too Long (src or dest) */
STATIC unsigned long STATLink    = 0; /* Link done */
STATIC unsigned long STATLError  = 0; /* Link problem */
STATIC unsigned long STATSymlink = 0; /* Symlink done */
STATIC unsigned long STATSLError = 0; /* Symlink problem */
STATIC unsigned long STATMkdir   = 0; /* Mkdir done */
STATIC unsigned long STATMdError = 0; /* Mkdir problem */
STATIC unsigned long STATOError  = 0; /* Other errors */

#define MAXXPOST 256
#define STATREFRESH 10800   /* 3 hours */

/*
**  Write some statistics and reset all counters.
*/
void
ProcessStats()
{
  time_t Time;

  Time = time (NULL);
  syslog(L_NOTICE,
	"seconds %lu links %lu %lu symlinks %lu %lu mkdirs %lu %lu missing %lu toolong %lu other %lu",
	Time - STATTime, STATLink, STATLError, STATSymlink, STATSLError,
	STATMkdir, STATMdError, STATMissing, STATTooLong, STATOError);

  STATMissing = STATTooLong = STATLink = STATLError = 0;
  STATSymlink = STATSLError = STATMkdir = STATMdError = STATOError = 0;
  STATTime = Time;
}

/*
**  Try to make one directory.  Return FALSE on error.
*/
STATIC BOOL
MakeDir(Name)
    char		*Name;
{
    struct stat		Sb;

    if (mkdir(Name, GROUPDIR_MODE) >= 0) {
        STATMkdir++;
	return TRUE;
    }

    /* See if it failed because it already exists. */
    return stat(Name, &Sb) >= 0 && S_ISDIR(Sb.st_mode);
}


/*
**  Make spool directory.  Return FALSE on error.
*/
STATIC BOOL
MakeSpoolDir(Name)
    register char	*Name;
{
    register char	*p;
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
	    if (!made) {
	        STATMdError++;
	        return FALSE;
	    }
	}

    return MakeDir(Name);
}


/*
**  Process the input.  Data can come from innd:
**	news/group/name/<number> [space news/group/<number>]...
**  or
**	news.group.name/<number>,[news.group.name/<number>]...
*/
STATIC void
ProcessIncoming(qp)
    QIOSTATE		*qp;
{
    register char	*p;
    register int	i;
    int			nxp;
    int			fd;
    int			lnval ;
    char	*names[MAXXPOST];


    for ( ; ; ) {

        if (time(NULL) - STATTime > STATREFRESH)
	  ProcessStats();

	/* Read the first line of data. */
	if ((p = QIOread(qp)) == NULL) {
	    if (QIOtoolong(qp)) {
		(void)fprintf(stderr, "crosspost line too long\n");
		STATTooLong++;
		continue;
	    }
	    break;
	}

	for (i = 0; *p && (i < MAXXPOST); i++) { /* parse input into array */
	    names[i] = p;
	    for ( ; *p; p++) {
		if (*p == '.') *p++ = '/'; /* dot to slash translation */
		else if ((*p == ',')	   /* name separators */
		  ||     (*p == ' ')
		  ||     (*p == '\t')
		  ||     (*p == '\n')) {
		    *p++ = '\0';
		    break;
		}
	    }
	}
	nxp = i;
	if (debug) {
	    for (i = 0; i < nxp; i++)
		(void)fprintf(stderr, "crosspost: debug %d %s\n",
		    i, names[i]);
	}

	if(syncfiles) fd = open(names[0], O_RDWR);

	for (i = 1; i < nxp; i++) {
            lnval = link(names[0], names[i]) ;
	    if (lnval == 0) STATLink++;
	    if (lnval < 0 && errno != EXDEV) { /* first try to link */
		register int j;
		char path[SPOOLNAMEBUFF+2];

		for (j = 0; (path[j] = names[i][j]) != '\0' ; j++) ;
		for (j--; (j > 0) && (path[j] != '/'); j--) ;
		if (path[j] == '/') {
		    path[j] = '\0';
		    /* try making parent dir */
		    if (MakeSpoolDir(path) == FALSE) {
			(void)fprintf(stderr, "crosspost cant mkdir %s\n",
				path);
		    }
		    else {
			/* 2nd try to link */
			lnval = link(names[0], names[i]) ;
			if (lnval == 0) STATLink++;
			if (lnval < 0 && errno == EXDEV) {
#if !defined(HAVE_SYMLINK)
			    (void)fprintf(stderr, "crosspost cant link %s %s",
				names[0], names[i]);
			    perror(" ");
#else
			    /* Try to make a symbolic link
			    ** to the full pathname.
			    */
			    for (j = 0, p = Dir; (j < SPOOLNAMEBUFF) && *p; )
				path[j++] = *p++; /* copy spool dir */
			    if (j < SPOOLNAMEBUFF) path[j++] = '/';
			    for (p = names[0]; (j < SPOOLNAMEBUFF) && *p; )
				path[j++] = *p++;	/* append path */
			    path[j++] = '\0';
			    if (symlink(path, names[i]) < 0) {
				(void)fprintf(stderr,
				    "crosspost cant symlink %s %s",
				    path, names[i]);
				perror(" ");
				STATSLError++;
			    }
			    else
			      STATSymlink++;
#endif	/* !defined(HAVE_SYMLINK) */
			} else if (lnval < 0) {
			    if (lnval == ENOENT)
			      STATMissing++;
			    else {
			      (void)fprintf(stderr, "crosspost cant link %s %s",
					    names[0], names[i]);
			      perror(" ");
			      STATLError++;
			    }
                        }
		    }
		} else {
		    (void)fprintf(stderr, "crosspost bad path %s\n",
			    names[i]);
		    STATOError++;
		}
	    } else if (lnval < 0) {
		register int j;
		char path[SPOOLNAMEBUFF+2];

#if !defined(HAVE_SYMLINK)
                (void)fprintf(stderr, "crosspost cant link %s %s",
                              names[0], names[i]);
                perror(" ");
#else
                /* Try to make a symbolic link
                ** to the full pathname.
                */
                for (j = 0, p = Dir; (j < SPOOLNAMEBUFF) && *p; )
                    path[j++] = *p++; /* copy spool dir */
                if (j < SPOOLNAMEBUFF) path[j++] = '/';
                for (p = names[0]; (j < SPOOLNAMEBUFF) && *p; )
                    path[j++] = *p++;	/* append path */
                path[j++] = '\0';
                if (symlink(path, names[i]) < 0) {
                    (void)fprintf(stderr,
                                  "crosspost cant symlink %s %s",
                                  path, names[i]);
                    perror(" ");
		    STATSLError++;
                }
		else
		  STATSymlink++;
#endif	/* !defined(HAVE_SYMLINK) */
            }
	}

	if (syncfiles && (fd >= 0)) {
	    (void)fsync(fd);
	    (void)close(fd);
	}
    }

    if (QIOerror(qp))
	(void)fprintf(stderr, "crosspost cant read %s\n", strerror(errno));
    QIOclose(qp);
}


STATIC NORETURN
Usage()
{
    (void)fprintf(stderr, "usage:  crosspost [-D dir] [files...]\n");
    exit(1);
}


int
main(ac, av)
    int			ac;
    char		*av[];
{
    register int	i;
    QIOSTATE		*qp;

    /* Set defaults. */
    Dir = _PATH_SPOOL;
    (void)umask(NEWSUMASK);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "D:ds")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'D':
	    Dir = optarg;	/* specify spool path */
	    break;
	case 'd':
	    debug = TRUE;
	    break;
	case 's':
	    syncfiles = FALSE;	/* do not fsync articles */
	    break;
	}
    ac -= optind;
    av += optind;
    InSpoolDir = EQ(Dir, _PATH_SPOOL);

    if (chdir(Dir) < 0) {
	(void)fprintf(stderr, "crosspost cant chdir %s %s\n",
		Dir, strerror(errno));
	exit(1);
    }
    openlog("crosspost", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
    STATTime = time (NULL);
    if (ac == 0)
	ProcessIncoming(QIOfdopen(STDIN));
    else {
	for ( ; *av; av++)
	    if (EQ(*av, "-"))
		ProcessIncoming(QIOfdopen(STDIN));
	    else if ((qp = QIOopen(*av)) == NULL)
		(void)fprintf(stderr, "crosspost cant open %s %s\n",
			*av, strerror(errno));
	    else
		ProcessIncoming(qp);
    }

    ProcessStats();
    exit(0);
    /* NOTREACHED */
}
