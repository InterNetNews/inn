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
**   - The only ports < 1024 that we'll bind to are 119 and 433, or a port
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
**  things like LD_PRELOAD).
*/

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <syslog.h>

#ifdef HAVE_INET6
# include <netdb.h>
#endif

#include "inn/innconf.h"
#include "inn/messages.h"
#include "libinn.h"
#include "macros.h"
#include "paths.h"

/* Fake up a do-nothing setgroups for Cygwin. */
#if !HAVE_SETGROUPS
# define setgroups(n, list)     0
#endif

/* To run innd under the debugger, uncomment this and fix the path. */
/* #define DEBUGGER "/usr/ucb/dbx" */


int
main(int argc, char *argv[])
{
    struct passwd *pwd;
    struct group *grp;
    uid_t news_uid, real_uid;
    gid_t news_gid;
    int snum = 0;
    int port, s[MAX_SOCKETS + 1], i, j;
#ifdef HAVE_INET6
    struct in6_addr address6;
    bool addr6_specified = false;
#endif
    struct in_addr address;
    bool addr_specified = false;
    char *p;
    char **innd_argv;
    char pflag[SMBUF];
#ifdef PURIFY
    char *innd_env[11];
#else
    char *innd_env[9];
#endif

    /* Set up the error handlers.  Always print to stderr, and for warnings
       also syslog with a priority of LOG_ERR.  For fatal errors, also
       syslog with a priority of LOG_CRIT.  These priority levels are a
       little high, but they're chosen to match innd. */
    openlog("inndstart", LOG_CONS, LOG_INN_PROG);
    message_handlers_warn(2, message_log_stderr, message_log_syslog_err);
    message_handlers_die(2, message_log_stderr, message_log_syslog_crit);
    message_program_name = "inndstart";

    /* Convert NEWSUSER and NEWSGRP to a UID and GID.  getpwnam() and
       getgrnam() don't set errno normally, so don't print strerror() on
       failure; it probably contains garbage.*/
    pwd = getpwnam(NEWSUSER);
    if (!pwd)
        die("can't getpwnam(%s)", NEWSUSER);
    news_uid = pwd->pw_uid;
    grp = getgrnam(NEWSGRP);
    if (!grp)
        die("can't getgrnam(%s)", NEWSGRP);
    news_gid = grp->gr_gid;

    /* Exit if run by any other user or group. */
    real_uid = getuid();
    if (real_uid != news_uid)
        die("must be run by user %s (%lu), not %lu", NEWSUSER,
                (unsigned long)news_uid, (unsigned long)real_uid);

    /* Drop all supplemental groups and drop privileges to read inn.conf.
       setgroups() can only be invoked by root, so if inndstart isn't setuid
       root this is where we fail. */
    if (setgroups(1, &news_gid) < 0)
        syswarn("can't setgroups (is inndstart setuid root?)");
    if (seteuid(news_uid) < 0)
        sysdie("can't seteuid to %lu", (unsigned long)news_uid);
    if (!innconf_read(NULL))
        exit(1);

    /* Check for a bind address specified in inn.conf.  "any" or "all" will
       cause inndstart to bind to INADDR_ANY. */
    address.s_addr = htonl(INADDR_ANY);
    p = innconf->bindaddress;
    if (p && strcmp(p, "all") != 0 && strcmp(p, "any") != 0) {
        if (!inet_aton(p, &address))
            die("invalid bindaddress in inn.conf (%s)", p);
    }
#ifdef HAVE_INET6
    address6 = in6addr_any;
    p = innconf->bindaddress6;
    if (p && strcmp(p, "all") != 0 && strcmp(p, "any") != 0) {
	if (inet_pton(AF_INET6, p, &address6) < 1)
	    die("invalid bindaddress6 in inn.conf (%s)", p);
    }
#endif

    /* Parse our command-line options.  The only options we take are -P,
       which specifies what port number to bind to, and -I, which specifies
       what IP address to bind to.  Both override inn.conf.  Support both
       "-P <port>" and "-P<port>".  All other options are passed through to
       innd. */
    port = innconf->port;
    for (i = 1; i < argc; i++) {
        if (strncmp("-P", argv[i], 2) == 0) {
            if (strlen(argv[i]) > 2) {
                port = atoi(&argv[i][2]);
            } else {
                i++;
                if (argv[i] == NULL)
                    die("missing port after -P");
                port = atoi(argv[i]);
            }
            if (port == 0)
                die("invalid port %s (must be a number)", argv[i]);
#ifdef HAVE_INET6
        } else if (strncmp("-6", argv[i], 2) == 0) {
            if (strlen(argv[i]) > 2) {
                p = &argv[i][2];
            } else {
                i++;
                if (argv[i] == NULL)
                    die("missing address after -6");
                p = argv[i];
            }
            if (inet_pton(AF_INET6, p, &address6) < 1)
                die("invalid address %s", p);
	    addr6_specified = true;
#endif
        } else if (strncmp("-I", argv[i], 2) == 0) {
            if (strlen(argv[i]) > 2) {
                p = &argv[i][2];
            } else {
                i++;
                if (argv[i] == NULL)
                    die("missing address after -I");
                p = argv[i];
            }
            if (!inet_aton(p, &address))
                die("invalid address %s", p);
	    addr_specified = true;
        }
    }
            
    /* Make sure that the requested port is legitimate. */
    if (port < 1024 && port != 119
#ifdef INND_PORT
        && port != INND_PORT
#endif
        && port != 433)
        die("can't bind to restricted port %d", port);

    /* Now, regain privileges so that we can change system limits and bind
       to our desired port. */
    if (seteuid(0) < 0)
        sysdie("can't seteuid to 0");

    /* innconf->rlimitnofile <= 0 says to leave it alone. */
    if (innconf->rlimitnofile > 0 && setfdlimit(innconf->rlimitnofile) < 0)
        syswarn("can't set file descriptor limit to %ld",
                innconf->rlimitnofile);

    /* Create a socket and name it. */
#ifdef HAVE_INET6
    if( ! (addr_specified || addr6_specified) ) {
	struct addrinfo hints, *addr, *ressave;
	char service[16];
	int error;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = PF_UNSPEC;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(service, sizeof(service), "%d", port);
	error = getaddrinfo(NULL, service, &hints, &addr);
	if (error < 0)
	    die("getaddrinfo: %s", gai_strerror(error));

	for (ressave = addr; addr; addr = addr->ai_next) {
	    if ((i = socket(addr->ai_family, addr->ai_socktype,
			    addr->ai_protocol)) < 0)
		continue; /* ignore */
#ifdef SO_REUSEADDR
	    j = 1;
	    if (setsockopt(i, SOL_SOCKET, SO_REUSEADDR, (char *)&j,
			sizeof j) < 0)
		syswarn("can't set SO_REUSEADDR");
#endif
	    if (bind(i, addr->ai_addr, addr->ai_addrlen) < 0) {
		j = errno;
		close(i);
		errno = j;
		continue; /* ignore */
	    }
	    s[snum++] = i;
	    if (snum == MAX_SOCKETS)
		break;
	}
	freeaddrinfo(ressave);

	if (snum == 0)
	    sysdie("can't bind socket");
    } else {
	if ( addr6_specified ) {
	    struct sockaddr_in6 server6;

	    s[snum] = socket(PF_INET6, SOCK_STREAM, 0);
	    if (s[snum] < 0)
		sysdie("can't open inet6 socket");
#ifdef SO_REUSEADDR
	    i = 1;
	    if (setsockopt(s[snum], SOL_SOCKET, SO_REUSEADDR, (char *)&i,
			sizeof i) < 0)
		syswarn("can't set SO_REUSEADDR");
#endif
	    memset(&server6, 0, sizeof server6);
	    server6.sin6_port = htons(port);
	    server6.sin6_family = AF_INET6;
	    server6.sin6_addr = address6;
#ifdef HAVE_SOCKADDR_LEN
	    server6.sin6_len = sizeof server6;
#endif
	    if (bind(s[snum], (struct sockaddr *)&server6, sizeof server6) < 0)
		sysdie("can't bind inet6 socket");
	    snum++;
	}
	if ( addr_specified )
#endif /* HAVE_INET6 */
	{
	    struct sockaddr_in server;

	    s[snum] = socket(PF_INET, SOCK_STREAM, 0);
	    if (s[snum] < 0)
		sysdie("can't open inet socket");
#ifdef SO_REUSEADDR
	    i = 1;
	    if (setsockopt(s[snum], SOL_SOCKET, SO_REUSEADDR, (char *) &i,
			sizeof i) < 0)
		syswarn("can't set SO_REUSEADDR");
#endif
	    memset(&server, 0, sizeof server);
	    server.sin_port = htons(port);
	    server.sin_family = AF_INET;
#ifdef HAVE_SOCKADDR_LEN
	    server.sin_len = sizeof server;
#endif
	    server.sin_addr = address;
	    if (bind(s[snum], (struct sockaddr *)&server, sizeof server) < 0)
		sysdie("can't bind inet socket");
	    snum++;
	}
#ifdef HAVE_INET6
    }
#endif
    s[snum] = -1;

    /* Now, permanently drop privileges. */
    if (setgid(news_gid) < 0 || getgid() != news_gid)
        sysdie("can't setgid to %lu", (unsigned long)news_gid);
    if (setuid(news_uid) < 0 || getuid() != news_uid)
        sysdie("can't setuid to %lu", (unsigned long)news_uid);

    /* Build the argument vector for innd.  Pass -p<port> to innd to tell it
       what port we just created and bound to for it. */
    innd_argv = xmalloc((1 + argc + 1) * sizeof(char *));
    i = 0;
    strcpy(pflag, "-p ");
    for (j = 0; s[j] > 0; j++) {
	char temp[16];

	snprintf(temp, sizeof(temp), "%d,", s[j]);
	strcat(pflag, temp);
    }
    /* chop off the trailing , */
    j = strlen(pflag) - 1;
    pflag[j] = '\0';
#ifdef DEBUGGER
    innd_argv[i++] = DEBUGGER;
    innd_argv[i++] = concatpath(innconf->pathbin, "innd");
    innd_argv[i] = 0;
    printf("When starting innd, use -d %s\n", s, pflag);
#else /* DEBUGGER */
    innd_argv[i++] = concatpath(innconf->pathbin, "innd");
    innd_argv[i++] = pflag;

    /* Don't pass along -p, -P, or -I.  Check the length of the argument
       string, and if it == 2 (meaning there's nothing after the -p or -P or
       -I), skip the next argument too, to support leaving a space between
       the argument and the value. */
    for (j = 1; j < argc; j++) {
        if (argv[j][0] == '-' && strchr("pP6I", argv[j][1])) {
            if (strlen(argv[j]) == 2)
                j++;
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
       stop things from working that should.  We have to pass BIND_INADDR so
       that it's set for programs, such as innfeed, that innd may spawn. */
    innd_env[0] = concat("PATH=", innconf->pathbin, ":", innconf->pathetc,
                         ":/bin:/usr/bin:/usr/ucb", (char *) 0);
    innd_env[1] = concat( "TMPDIR=", innconf->pathtmp,  (char *) 0);
    innd_env[2] = concat(  "SHELL=", _PATH_SH,          (char *) 0);
    innd_env[3] = concat("LOGNAME=", NEWSMASTER,        (char *) 0);
    innd_env[4] = concat(   "USER=", NEWSMASTER,        (char *) 0);
    innd_env[5] = concat(   "HOME=", innconf->pathnews, (char *) 0);
    i = 6;
    p = getenv("BIND_INADDR");
    if (p != NULL)
        innd_env[i++] = concat("BIND_INADDR=", p, (char *) 0);
    p = getenv("TZ");
    if (p != NULL)
        innd_env[i++] = concat("TZ=", p, (char *) 0);
#ifdef PURIFY
    /* you have to compile with `purify cc -DPURIFY' to get this */
    p = getenv("DISPLAY");
    if (p != NULL)
        innd_env[i++] = concat("DISPLAY=", p, (char *) 0);
    p = getenv("PURIFYOPTIONS");
    if (p != NULL)
        innd_env[i++] = concat("PURIFYOPTIONS=", p, (char *) 0);
#endif
    innd_env[i] = 0;

    /* Go exec innd. */
    execve(innd_argv[0], innd_argv, innd_env);
    sysdie("can't exec %s", innd_argv[0]);

    /* Not reached. */
    return 1;
}
