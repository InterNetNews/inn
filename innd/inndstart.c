/*  $Id$
**
**  Open the privileged port, then exec innd.
**
**  inndstart, in a normal INN installation, is installed setuid root and
**  executable only by users in the news group.  Because it is setuid root,
**  it's very important to ensure that it be as simple and secure as
**  possible so that news access can't be leveraged into root access.
**
**  Fighting against this desire, as much of INN's operation as possible
**  should be configurable at run-time using inn.conf, and the news system
**  should be able to an alternate inn.conf by setting INNCONF to the path
**  to that file before starting any programs.  The configuration data
**  therefore can't be trusted.
**
**  Our security model is therefore:
**
**   - The only three operations performed while privileged are determining
**     the UID and GID of NEWSUSER and NEWSGRP, setting system limits, and
**     opening the privileged port we're binding to.
**
**   - We can only be executed by the NEWSUSER and NEWSGRP, both compile-
**     time constants; otherwise, we exit.  Similarly, we will only setuid()
**     to the NEWSUSER.  This is to prevent someone other than the NEWSUSER
**     but still able to execute inndstart for whatever reason from using it
**     to run innd as the news user with bogus configuration information,
**     thereby possibly compromising the news account.
**
**   - The only ports < 1024 that we'll bind to are 119 and 443, or a port
**     given at configure time with --with-innd-port.  This is to prevent
**     the news user from taking over a service such as telnet or POP and
**     potentially gaining access to user passwords.
**
**  This program therefore gives the news user the ability to revoke system
**  file descriptor limits and bind to the news port, and nothing else.
**
**  Note that we do use getpwnam() to determine the UID of NEWSUSER, which
**  potentially opens an exploitable hole on those systems that don't
**  correctly prevent a user running a setuid program from interfering with
**  the running process (replacing system calls, for example, or using
**  things like LD_PRELOAD).  It may be desireable to map those to UIDs at
**  configure time to prevent this attack.
*/
#include "config.h"
#include "clibrary.h"
#include <arpa/inet.h>
#include <syslog.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

/* Some odd systems need sys/time.h included before sys/resource.h. */
#ifdef HAVE_RLIMIT
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif
# include <sys/resource.h>
#endif

/* AIX 4.1 has *rlimit() but not RLIMIT_NOFILE or any equivalent thereof, so
   pretend it doesn't have *rlimit() at all. */
#ifndef RLIMIT_NOFILE
# undef HAVE_RLIMIT
#endif

#include "paths.h"
#include "libinn.h"
#include "macros.h"

/* To run innd under the debugger, uncomment this and fix the path. */
/* #define DEBUGGER "/usr/ucb/dbx" */


#ifdef HAVE_RLIMIT
/*
**  Set the limit on the number of open files we can have.  I don't
**  like having to do this.
*/
static void
set_descriptor_limit(int n)
{
    struct rlimit rl;

    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        syslog(L_ERROR, "can't getrlimit(NOFILE): %m");
        return;
    }
    rl.rlim_cur = n;
    if (setrlimit(RLIMIT_NOFILE, &rl) < 0) {
        syslog(L_ERROR, "can't setrlimit(NOFILE, %d): %m", n);
        return;
    }
}
#endif /* HAVE_RLIMIT */


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
    uid_t               real_uid;
    gid_t               real_gid;
    struct stat         Sb;
    int                 port;
    unsigned long       address;
    int                 s;
    struct sockaddr_in  server;
    int                 i;
    int                 j;
    char *              p;
    char **             innd_argv;
    char                pflag[SMBUF];
    char *              innd_env[9];

    openlog("inndstart", L_OPENLOG_FLAGS, LOG_INN_PROG);

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

    /* Exit if run by any other user or group. */
    real_uid = getuid();
    if (real_uid != news_uid) {
        syslog(L_FATAL, "ran by UID %d, who isn't %s (%d)", real_uid,
               NEWSUSER, news_uid);
        fprintf(stderr, "inndstart must be run by user %s\n", NEWSUSER);
        exit(1);
    }
    real_gid = getgid();
    if (real_gid != news_gid) {
        syslog(L_FATAL, "ran by GID %d, who isn't %s (%d)", real_gid,
               NEWSGRP, news_gid);
        fprintf(stderr, "inndstart must be run by group %s\n", NEWSGRP);
        exit(1);
    }

    /* Drop all supplemental groups and drop privileges to read inn.conf. */
    if (setgroups(1, &news_gid) < 0) syslog(L_ERROR, "can't setgroups: %m");
    set_user(news_uid, 0);
    if (ReadInnConf() < 0) exit(1);

    /* Ensure that pathrun exists and that it has the right ownership. */
    if (stat(innconf->pathrun, &Sb) < 0) {
        syslog(L_FATAL, "can't stat pathrun (%s): %m", innconf->pathrun);
        fprintf(stderr, "Can't stat pathrun (%s): %s\n", innconf->pathrun,
                strerror(errno));
        exit(1);
    }
    if (!S_ISDIR(Sb.st_mode)) {
        syslog(L_FATAL, "pathrun (%s) not a directory", innconf->pathrun);
        fprintf(stderr, "pathrun (%s) not a directory\n", innconf->pathrun);
        exit(1);
    }
    if (Sb.st_uid != news_uid) {
        syslog(L_FATAL, "pathrun (%s) owned by user %d, not %s (%d)",
               innconf->pathrun, Sb.st_uid, NEWSUSER, news_uid);
        fprintf(stderr, "pathrun (%s) must be owned by user %s\n",
                innconf->pathrun, NEWSUSER);
        exit(1);
    }
    if (Sb.st_gid != news_gid) {
        syslog(L_FATAL, "pathrun (%s) owned by group %d, not %s (%d)",
               innconf->pathrun, Sb.st_gid, NEWSGRP, news_gid);
        fprintf(stderr, "pathrun (%s) must be owned by group %s\n",
                innconf->pathrun, NEWSGRP);
        exit(1);
    }

    /* Check for a bind address specified in inn.conf.  "any" or "all" will
       cause inndstart to bind to INADDR_ANY. */
    address = htonl(INADDR_ANY);
    p = innconf->bindaddress;
    if (p && !EQ(p, "all") && !EQ(p, "any")) {
        address = inet_addr(p);
        if (address == (unsigned long) -1) {
            syslog(L_FATAL, "invalid bindaddress in inn.conf (%s)", p);
            exit(1);
        }
    }

    /* Parse our command-line options.  The only options we take are -P,
       which specifies what port number to bind to, and -I, which specifies
       what IP address to bind to.  Both override inn.conf.  Support both
       "-P <port>" and "-P<port>".  All other options are passed through to
       innd. */
    port = innconf->port;
    for (i = 1; i < argc; i++) {
        if (EQn("-P", argv[i], 2)) {
            if (strlen(argv[i]) > 2) {
                port = atoi(&argv[i][2]);
            } else {
                i++;
                if (argv[i] == NULL) {
                    syslog(L_FATAL, "missing port after -P");
                    fprintf(stderr, "Missing port after -P\n");
                    exit(1);
                }
                port = atoi(argv[i]);
            }
            if (port == 0) {
                syslog(L_FATAL, "invalid port %s", argv[i]);
                fprintf(stderr, "Invalid port %s (must be numeric)\n",
                        argv[i]);
                exit(1);
            }
        } else if (EQn("-I", argv[i], 2)) {
            if (strlen(argv[i]) > 2) {
                address = atol(&argv[i][2]);
            } else {
                i++;
                if (argv[i] == NULL) {
                    syslog(L_FATAL, "missing address after -I");
                    fprintf(stderr, "Missing address after -I\n");
                    exit(1);
                }
                address = atoi(argv[i]);
            }
            if (address == 0) {
                syslog(L_FATAL, "invalid address %s", argv[i]);
                fprintf(stderr, "Invalid address %s\n", argv[i]);
                exit(1);
            }
            address = htonl(address);
        }
    }
            
    /* Make sure that the requested port is legitimate. */
    if (port < 1024 && port != 119
#ifdef INND_PORT
        && port != INND_PORT
#endif
        && port != 433) {
        syslog(L_FATAL, "tried to bind to port %d", port);
        fprintf(stderr, "Can't bind to restricted port\n");
        exit(1);
    }

    /* Now, regain privileges so that we can change system limits and bind
       to our desired port. */
    set_user(0, news_uid);

    /* innconf->rlimitnofile <= 0 says to leave it alone. */
#ifdef HAVE_RLIMIT
    if (innconf->rlimitnofile > 0)
        set_descriptor_limit(innconf->rlimitnofile);
#endif

    /* Create a socket and name it.  innconf->bindaddress controls what
       address we bind as, defaulting to INADDR_ANY. */
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        syslog(L_FATAL, "can't open socket: %m");
        exit(1);
    }
#ifdef SO_REUSEADDR
    i = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &i, sizeof i) < 0)
        syslog(L_ERROR, "can't setsockopt: %m");
#endif
    memset(&server, 0, sizeof server);
    server.sin_port = htons(port);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = address;
    if (bind(s, (struct sockaddr *) &server, sizeof server) < 0) {
        syslog(L_FATAL, "can't bind: %m");
        exit(1);
    }

    /* Now, permanently drop privileges. */
    if (setgid(news_gid) < 0 || getgid() != news_gid) {
        syslog(L_FATAL, "can't setgid(%d): %m", news_gid);
        exit(1);
    }
    if (setuid(news_uid) < 0 || getuid() != news_uid) {
        syslog(L_FATAL, "can't setuid(%d): %m", news_uid);
        exit(1);
    }

    /* Build the argument vector for innd.  Pass -p<port> to innd to tell it
       what port we just created and bound to for it. */
    innd_argv = NEW(char *, 1 + argc + 1);
    i = 0;
#ifdef DEBUGGER
    innd_argv[i++] = DEBUGGER;
    innd_argv[i++] = cpcatpath(innconf->pathbin, "innd");
    innd_argv[i] = 0;
    printf("When starting innd, use -dp%d\n", s);
#else /* DEBUGGER */
    sprintf(pflag, "-p%d", s);
    innd_argv[i++] = cpcatpath(innconf->pathbin, "innd");
    innd_argv[i++] = pflag;

    /* Don't pass along -p, -P, or -I.  Check the length of the argument
       string, and if it's == 2 (meaning there's nothing after the -p or -P
       or -I), skip the next argument too, to support leaving a space
       between the argument and the value. */
    for (j = 1; j < argc; j++) {
        if (argv[j][0] == '-' && strchr("pPI", argv[j][1])) {
            if (strlen(argv[j]) == 2) j++;
        } else {
            innd_argv[i++] = argv[j++];
        }
    }
    innd_argv[i] = 0;
#endif /* !DEBUGGER */

    /* Set up the environment.  Note that we're trusting BIND_INADDR and TZ;
       everything else is either from inn.conf or from configure.
       (BIND_INADDR is apparently needed by Linux?)  These should be
       sanity-checked before being propagated, but that requires knowledge
       of the range of possible values.  Just limiting their length doesn't
       necessarily do anything to prevent exploits and may stop things from
       working that should.  */
    innd_env[0] = concat("PATH=", innconf->pathbin, ":", innconf->pathetc,
                         ":/bin:/usr/bin:/usr/ucb", (char *) 0);
    innd_env[1] = concat( "TMPDIR=", innconf->pathtmp,  (char *) 0);
    innd_env[2] = concat(  "SHELL=", _PATH_SH,          (char *) 0);
    innd_env[3] = concat("LOGNAME=", NEWSMASTER,        (char *) 0);
    innd_env[4] = concat(   "USER=", NEWSMASTER,        (char *) 0);
    innd_env[5] = concat(   "HOME=", innconf->pathnews, (char *) 0);
    i = 6;
    p = getenv("BIND_INADDR");
    if (p != NULL) innd_env[i++] = concat("BIND_INADDR=", p, (char *) 0);
    p = getenv("TZ");
    if (p != NULL) innd_env[i++] = concat("TZ=", p, (char *) 0);
    innd_env[i] = 0;

    /* Go exec innd. */
    execve(innd_argv[0], innd_argv, innd_env);
    syslog(L_FATAL, "can't exec %s: %m", innd_argv[0]);
    fprintf(stderr, "Can't exec %s: %s", innd_argv[0], strerror(errno));
    _exit(1);

    /* NOTREACHED */
    return 1;
}
