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

#include "inn/innconf.h"
#include "inn/messages.h"
#include "libinn.h"
#include "macros.h"

/* Options for debugging malloc. */
#ifdef USE_DMALLOC
# define DMALLOC_OPTIONS \
    "DMALLOC_OPTIONS=debug=0x4e405c3,inter=100,log=innfeed-logfile"
#endif


int
main(int argc, char *argv[])
{
    struct passwd *     pwd;
    struct group *      grp;
    uid_t               news_uid;
    gid_t               news_gid;
    char **             innfeed_argv;
    char *              spawn_path;
    int                 i;

#if HAVE_SETRLIMIT
    struct rlimit       rl;
#endif

    openlog("innfeed", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
    message_handlers_warn(1, message_log_syslog_warning);
    message_handlers_die(1, message_log_syslog_err);

    /* Convert NEWSUSER and NEWSGRP to a UID and GID.  getpwnam() and
       getgrnam() don't set errno normally, so don't print strerror() on
       failure; it probably contains garbage.*/
    pwd = getpwnam(NEWSUSER);
    if (!pwd) die("can't getpwnam(%s)", NEWSUSER);
    news_uid = pwd->pw_uid;
    grp = getgrnam(NEWSGRP);
    if (!grp) die("can't getgrnam(%s)", NEWSGRP);
    news_gid = grp->gr_gid;

    /* Exit if run by another user. */
    if (getuid() != news_uid)
        die("ran by UID %lu, who isn't %s (%lu)", (unsigned long) getuid(),
            NEWSUSER, (unsigned long) news_uid);

    /* Drop privileges to read inn.conf. */
    if (seteuid(news_uid) < 0)
        sysdie("can't seteuid(%lu)", (unsigned long) news_uid);
    if (!innconf_read(NULL))
        exit(1);

    /* Regain privileges to increase system limits. */
    if (seteuid(0) < 0) sysdie("can't seteuid(0)");
    if (innconf->rlimitnofile >= 0)
        if (setfdlimit(innconf->rlimitnofile) < 0)
            syswarn("can't set file descriptor limit to %ld",
                    innconf->rlimitnofile);

    /* These calls will fail on some systems, such as HP-UX 11.00.  On those
       systems, we just blindly assume that the stack and data limits are
       high enough (they generally are). */
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
    if (setuid(news_uid) < 0 || getuid() != news_uid)
        sysdie("can't setuid to %lu", (unsigned long) news_uid);

    /* Check for imapfeed -- continue to use "innfeed" in variable
       names for historical reasons regardless */
    if ((argc > 1) && (strcmp(argv[1],"imapfeed") == 0))
    {
        argc--;
	argv++;
        spawn_path = concat(innconf->pathbin, "/imapfeed", (char *) 0);
    }
    else
        spawn_path = concat(innconf->pathbin, "/innfeed",  (char *) 0);


    /* Build the argument vector for innfeed. */
    innfeed_argv = xmalloc((argc + 1) * sizeof(char *));
    innfeed_argv[0] = spawn_path;
    for (i = 1; i <= argc; i++)
        innfeed_argv[i] = argv[i];
    innfeed_argv[argc] = NULL;

    /* Set debugging malloc options. */
#ifdef USE_DMALLOC
    putenv(DMALLOC_OPTIONS);
#endif

    /* Exec innfeed. */
    execv(innfeed_argv[0], innfeed_argv);
    sysdie("can't exec %s", innfeed_argv[0]);

    /* Not reached. */
    return 1;
}
