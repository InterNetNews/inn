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
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <syslog.h>

/* BSDI needs <netinet/in.h> before <arpa/inet.h>. */
#include <netinet/in.h>
#include <arpa/inet.h>

#include "paths.h"
#include "libinn.h"
#include "macros.h"

/* To run innd under the debugger, uncomment this and fix the path. */
/* #define DEBUGGER "/usr/ucb/dbx" */

/* Error handling.  We unfortunately can't just define a syslog error
   handler and use the standard error handling routines since an attacker
   can manufacture a situation where the wrong size buffer would be
   allocated for the syslog message.  Instead, use a macro so that the
   arguments can be passed to syslog directly.  These functions must be
   called with *two* sets of parentheses:  DIE((LOG_ERR, "Error message")).
   DIE logs and exits; WARN just logs. */
#define DIE(args)       do { syslog args; eprintf args; exit(1); } while (0)
#define WARN(args)      do { syslog args; eprintf args; } while (0)


/*
**  This function is needed by the above macros; it takes syslog-style
**  arguments (including an initial priority) and ignores the priority,
**  using vfprintf to print the rest to stderr.
*/
static void
eprintf(int priority UNUSED, const char *format, ...)
{
    va_list args;

    fprintf(stderr, "inndstart: ");
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}


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
set_user(uid_t euid, uid_t ruid)
{
#if HAVE_SETEUID
    if (seteuid(euid) < 0)
        DIE((LOG_ERR, "seteuid(%d) failed: %s", euid, strerror(errno)));
#elif HAVE_SETREUID
# ifdef _POSIX_SAVED_IDS
    ruid = -1;
# endif
    if (setreuid(ruid, euid) < 0)
        DIE((LOG_ERR, "setreuid(%d, %d) failed: %s", ruid, euid,
             strerror(errno)));
#endif /* HAVE_SETREUID */
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
    struct in_addr      address;
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
    if (!pwd) DIE((LOG_ERR, "getpwnam(%s) failed", NEWSUSER));
    news_uid = pwd->pw_uid;
    grp = getgrnam(NEWSGRP);
    if (!grp) DIE((LOG_ERR, "getgrnam(%s) failed", NEWSGRP));
    news_gid = grp->gr_gid;

    /* Exit if run by any other user or group. */
    real_uid = getuid();
    if (real_uid != news_uid)
        DIE((LOG_ERR, "must be run by user %s (%d), not %d", NEWSUSER,
             news_uid, real_uid));

    /* Drop all supplemental groups and drop privileges to read inn.conf. */
    if (setgroups(1, &news_gid) < 0)
        WARN((LOG_WARNING, "can't setgroups: %s", strerror(errno)));
    set_user(news_uid, 0);
    if (ReadInnConf() < 0) exit(1);

    /* Ensure that pathrun exists and that it has the right ownership. */
    if (stat(innconf->pathrun, &Sb) < 0)
        DIE((LOG_ERR, "can't stat pathrun(%s): %s", innconf->pathrun,
             strerror(errno)));
    if (!S_ISDIR(Sb.st_mode))
        DIE((LOG_ERR, "pathrun (%s) is not a directory", innconf->pathrun));
    if (Sb.st_uid != news_uid)
        DIE((LOG_ERR, "pathrun (%s) owned by user %d, not %s (%d)",
             innconf->pathrun, Sb.st_uid, NEWSUSER, news_uid));
    if (Sb.st_gid != news_gid)
        DIE((LOG_ERR, "pathrun (%s) owned by group %d, not %s (%d)",
             innconf->pathrun, Sb.st_gid, NEWSGRP, news_gid));

    /* Check for a bind address specified in inn.conf.  "any" or "all" will
       cause inndstart to bind to INADDR_ANY. */
    address.s_addr = htonl(INADDR_ANY);
    p = innconf->bindaddress;
    if (p && !EQ(p, "all") && !EQ(p, "any")) {
        if (!inet_aton(p, &address))
            DIE((LOG_ERR, "invalid bindaddress in inn.conf (%s)", p));
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
                if (argv[i] == NULL)
                    DIE((LOG_ERR, "missing port after -P"));
                port = atoi(argv[i]);
            }
            if (port == 0)
                DIE((LOG_ERR, "invalid port %s (must be a number)",
                     argv[i]));
        } else if (EQn("-I", argv[i], 2)) {
            if (strlen(argv[i]) > 2) {
                p = &argv[i][2];
            } else {
                i++;
                if (argv[i] == NULL)
                    DIE((LOG_ERR, "missing address after -I"));
                p = argv[i];
            }
            if (!inet_aton(p, &address))
                DIE((LOG_ERR, "invalid address %s", p));
        }
    }
            
    /* Make sure that the requested port is legitimate. */
    if (port < 1024 && port != 119
#ifdef INND_PORT
        && port != INND_PORT
#endif
        && port != 433)
        DIE((LOG_ERR, "can't bind to restricted port %d", port));

    /* Now, regain privileges so that we can change system limits and bind
       to our desired port. */
    set_user(0, news_uid);

    /* innconf->rlimitnofile <= 0 says to leave it alone. */
    if (innconf->rlimitnofile > 0)
        if (setfdlimit(innconf->rlimitnofile) < 0)
            DIE((LOG_ERR, "can't set file descriptor limit to %d: %s",
                 innconf->rlimitnofile, strerror(errno)));

    /* Create a socket and name it.  innconf->bindaddress controls what
       address we bind as, defaulting to INADDR_ANY. */
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) DIE((LOG_ERR, "can't open socket: %s", strerror(errno)));
#ifdef SO_REUSEADDR
    i = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &i, sizeof i) < 0)
        WARN((LOG_WARNING, "can't set SO_REUSEADDR: %s", strerror(errno)));
#endif
    memset(&server, 0, sizeof server);
    server.sin_port = htons(port);
    server.sin_family = AF_INET;
    server.sin_addr = address;
    if (bind(s, (struct sockaddr *) &server, sizeof server) < 0)
        DIE((LOG_ERR, "can't bind: %s", strerror(errno)));

    /* Now, permanently drop privileges. */
    if (setgid(news_gid) < 0 || getgid() != news_gid)
        DIE((LOG_ERR, "can't setgid to %d: %s", news_gid, strerror(errno)));
    if (setuid(news_uid) < 0 || getuid() != news_uid)
        DIE((LOG_ERR, "can't setuid to %d: %s", news_uid, strerror(errno)));

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
            continue;
        } else {
            innd_argv[i++] = argv[j];
        }
    }
    innd_argv[i] = 0;
#endif /* !DEBUGGER */

    /* Set up the environment.  Note that we're trusting BIND_INADDR and TZ;
       everything else is either from inn.conf or from configure.  These
       should be sanity-checked before being propagated, but that requires
       knowledge of the range of possible values.  Just limiting their
       length doesn't necessarily do anything to prevent exploits and may
       stop things from working that should.  */
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
    syslog(LOG_ERR, "can't exec %s: %m", innd_argv[0]);
    fprintf(stderr, "inndstart: can't exec %s: %s", innd_argv[0],
            strerror(errno));
    _exit(1);

    /* NOTREACHED */
    return 1;
}
