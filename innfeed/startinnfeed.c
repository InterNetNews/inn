/*  $Id$
**
**  Raise system limits and exec innfeed.
**
**  This is a setuid root wrapper around innfeed to increase system limits
**  (file descriptor limits and stack and data sizes).  In order to prevent
**  abuse, it uses roughly the same security model as inndstart; only the
**  news user can run this program, and it attempts to drop privileges when
**  doing operations that don't require it.
*/
#include "config.h"
#include "clibrary.h"
#include <syslog.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>

/* FreeBSD 3.4 RELEASE needs <sys/time.h> before <sys/resource.h>. */
#if HAVE_SETRLIMIT
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif
# include <sys/resource.h>
#endif

#include "libinn.h"
#include "macros.h"

/* Options for debugging malloc. */
#ifdef USE_DMALLOC
# define DMALLOC_OPTIONS \
    "DMALLOC_OPTIONS=debug=0x4e405c3,inter=100,log=innfeed-logfile"
#endif


/*
**  Drop or regain privileges.  On systems with POSIX saved UIDs, we can
**  simply set the effective UID directly, since the saved UID preserves our
**  ability to get back root access.  Otherwise, we have to swap the real
**  and effective UIDs (which doesn't work correctly on AIX).  Assume any
**  system with seteuid() has POSIX saved UIDs.  First argument is the new
**  effective UID, second argument is the UID to preserve (not used if the
**  system has saved UIDs).
*/
static void
set_user (uid_t euid, uid_t ruid)
{
#ifdef HAVE_SETEUID
    if (seteuid(euid) < 0) {
        syslog(L_ERROR, "seteuid(%d) failed: %m", euid);
        exit(1);
    }
#else
# ifdef HAVE_SETREUID
#  ifdef _POSIX_SAVED_IDS
    ruid = -1;
#  endif
    if (setreuid(ruid, euid) < 0) {
        syslog(L_ERROR, "setreuid(%d, %d) failed: %m", ruid, euid);
        exit(1);
    }
# endif /* HAVE_SETREUID */
#endif /* HAVE_SETEUID */
}


int
main(int argc, char *argv[])
{
    struct passwd *     pwd;
    struct group *      grp;
    uid_t               news_uid;
    gid_t               news_gid;
    char **             innfeed_argv;
    int                 i;

#if HAVE_SETRLIMIT
    struct rlimit       rl;
#endif

    openlog("innfeed", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);

    /* Convert NEWSUSER and NEWSGRP to a UID and GID.  getpwnam() and
       getgrnam() don't set errno normally, so don't print strerror() on
       failure; it probably contains garbage.*/
    pwd = getpwnam(NEWSUSER);
    if (!pwd) {
        syslog(L_FATAL, "getpwnam(%s) failed", NEWSUSER);
        exit(1);
    }
    news_uid = pwd->pw_uid;
    grp = getgrnam(NEWSGRP);
    if (!grp) {
        syslog(L_FATAL, "getgrnam(%s) failed", NEWSGRP);
        exit(1);
    }
    news_gid = grp->gr_gid;

    /* Exit if run by another user. */
    if (getuid() != news_uid) {
        syslog(L_FATAL, "ran by UID %d, who isn't %s (%d)", getuid(),
               NEWSUSER, news_uid);
        exit(1);
    }

    /* Drop privileges to read inn.conf. */
    set_user(news_uid, 0);
    if (ReadInnConf() < 0) exit(1);

    /* Regain privileges to increase system limits. */
    set_user(0, news_uid);
    if (innconf->rlimitnofile >= 0)
        if (setfdlimit(innconf->rlimitnofile) < 0)
            syslog(LOG_WARNING, "can't set file descriptor limit to %d: %m",
                   innconf->rlimitnofile);

#if HAVE_SETRLIMIT
    rl.rlim_cur = RLIM_INFINITY;
    rl.rlim_max = RLIM_INFINITY;
# ifdef RLIMIT_DATA
    setrlimit(RLIMIT_DATA, &rl);
# endif
# ifdef RLIMIT_STACK
    setrlimit(RLIMIT_STACK, &rl);
# endif
#endif /* HAVE_SETRLIMIT */

    /* Permanently drop privileges. */
    if (setuid(news_uid) < 0 || getuid() != news_uid) {
        syslog(LOG_ERR, "can't setuid(%d): %m", news_uid);
        exit(1);
    }

    /* Build the argument vector for innfeed. */
    innfeed_argv = NEW(char *, argc + 1);
    innfeed_argv[0] = concat(innconf->pathbin, "/innfeed", (char *) 0);
    for (i = 1; i <= argc; i++)
        innfeed_argv[i] = argv[i];
    innfeed_argv[argc] = NULL;

    /* Set debugging malloc options. */
#ifdef USE_DMALLOC
    putenv(DMALLOC_OPTIONS);
#endif

    /* Exec innfeed. */
    execv(innfeed_argv[0], innfeed_argv);
    syslog(LOG_ERR, "can't exec %s: %m", innfeed_argv[0]);
    _exit(1);

    /* NOTREACHED */
    return 1;
}
