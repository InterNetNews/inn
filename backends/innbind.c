/*  $Id$
**
**  Helper program to bind a socket to a low-numbered port.
**
**  Written by Russ Allbery <rra@stanford.edu>
*/

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include <errno.h>
#include <pwd.h>
#include <syslog.h>

#include "inn/messages.h"
#include "inn/vector.h"
#include "libinn.h"

/* Macros to set the len attribute of sockaddrs. */
#if HAVE_STRUCT_SOCKADDR_SA_LEN
# define sin_set_length(s)      ((s)->sin_len  = sizeof(struct sockaddr_in))
# define sin6_set_length(s)     ((s)->sin6_len = sizeof(struct sockaddr_in6))
#else
# define sin_set_length(s)      /* empty */
# define sin6_set_length(s)     /* empty */
#endif

/* INND_PORT is the additional port specified at configure time to which the
   news user should be allowed to bind.  If it's not set, set it to 119 (which
   will cause it to have no effect).  I hate #ifdef in code, can you tell? */
#ifndef INND_PORT
# define INND_PORT 119
#endif


/*
**  Convert a string to a number with error checking, returning true if the
**  number was parsed correctly and false otherwise.  Stores the converted
**  number in the second argument.  Equivalent to calling strtol, but with the
**  base always fixed at 10, with checking of errno, ensuring that all of the
**  string is consumed, and checking that the resulting number is positive.
*/
static bool
convert_string(const char *string, long *result)
{
    char *end;

    if (*string == '\0')
        return false;
    errno = 0;
    *result = strtol(string, &end, 10);
    if (errno != 0 || *end != '\0' || *result < 0)
        return false;
    return true;
}


/*
**  Bind an IPv4 address, given the file descriptor, string giving the
**  address, and the port.  The fourth argument is the full binding
**  specification for error reporting.  Dies on any failure.
*/
static void
bind_ipv4(int fd, const char *address, unsigned short port, const char *spec)
{
    struct sockaddr_in server;
    struct in_addr addr;

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if (!inet_aton(address, &addr))
        die("invalid IPv4 address %s in %s", address, spec);
    server.sin_addr = addr;
    sin_set_length(&server);
    if (bind(fd, (struct sockaddr *) &server, sizeof(server)) < 0)
        sysdie("cannot bind socket for %s", spec);
}


/*
**  Bind an IPv6 address, given the file descriptor, string giving the
**  address, and the port.  The fourth argument is the full binding
**  specification for error reporting.  Dies on any failure.
*/
#ifdef HAVE_INET6
static void
bind_ipv6(int fd, const char *address, unsigned short port, const char *spec)
{
    struct sockaddr_in6 server;
    struct in6_addr addr;

    server.sin6_family = AF_INET6;
    server.sin6_port = htons(port);
    if (inet_pton(AF_INET6, address, &addr) < 1)
        die("invalid IPv6 address %s in %s", address, spec);
    server.sin6_addr = addr;
    sin6_set_length(&server);
    if (bind(fd, (struct sockaddr *) &server, sizeof(server)) < 0)
        sysdie("cannot bind socket for %s", spec);
}
#endif /* HAVE_INET6 */


/*
**  Given a command line argument, which consists of a comma-separated quad of
**  file descriptor number, protocol family, address, and port (sufficient
**  generality to handle both IPv4 and IPv6), and bind it.  Dies on any
**  failure.
*/
static void
bind_address(const char *string)
{
    struct vector *spec;
    int family, fd, type;
    unsigned short port;
    long value;
    socklen_t length;

    /* Do the initial parse and allocate our data structures. */
    spec = vector_split(string, ',', NULL);
    if (spec->count != 4)
        die("invalid command-line argument %s", string);

    /* Get the file descriptor, address family, and port. */
    if (!convert_string(spec->strings[0], &value))
        die("invalid file descriptor %s in %s", spec->strings[0], string);
    fd = value;
    if (!convert_string(spec->strings[1], &value))
        die("invalid protocol family %s in %s", spec->strings[1], string);
    family = value;
    if (!convert_string(spec->strings[3], &value))
        die("invalid port number %s in %s", spec->strings[3], string);
    if (value == 0)
        die("port may not be zero in %s", string);
    port = value;

    /* Make sure that we're allowed to bind to that port. */
    if (port < 1024 && port != 119 && port != 433 && port != INND_PORT)
        die("cannot bind to restricted port %hu in %s", port, string);

    /* Sanity check on the socket. */
    length = sizeof(type);
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &length) < 0)
        sysdie("cannot get socket options for file descriptor %d", fd);
    if (type != SOCK_STREAM)
        die("invalid file descriptor %d: not SOCK_STREAM", fd);

    /* Based on the address family, parse either an IPv4 or IPv6 address. */
    if (family == AF_INET)
        bind_ipv4(fd, spec->strings[2], port, string);
#ifdef HAVE_INET6
    else if (family == AF_INET6)
        bind_ipv6(fd, spec->strings[2], port, string);
#endif
    else
        die("unknown protocol family %s in %s", spec->strings[1], string);

    /* Done.  Clean up. */
    vector_free(spec);
}


int
main(int argc, char *argv[])
{
    struct passwd *pwd;
    uid_t real_uid;
    int i;

    /* Set up the error handlers.  Errors go to stderr and to syslog with a
       priority of LOG_CRIT.  This priority level is too high, but it's chosen
       to match innd. */
    openlog("innbind", LOG_CONS, LOG_INN_PROG);
    message_handlers_die(2, message_log_stderr, message_log_syslog_crit);
    message_program_name = "innbind";

    /* If we're running privileged (effective and real UIDs are different),
       convert NEWSUSER to a UID and exit if run by another user.  Don't do
       this if we're not running privileged to make installations that don't
       need privileged ports easier and to make testing easier. */
    real_uid = getuid();
    if (real_uid != geteuid()) {
        pwd = getpwnam(NEWSUSER);
        if (pwd == NULL)
            die("cannot get UID for %s", NEWSUSER);
        if (real_uid != pwd->pw_uid)
            die("must be run by user %s (%lu), not %lu", NEWSUSER,
                (unsigned long) pwd->pw_uid, (unsigned long) real_uid);
    }

    /* Process command-line options. */
    for (i = 1; i < argc; i++)
        bind_address(argv[i]);
    exit(0);
}
