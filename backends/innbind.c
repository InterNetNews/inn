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
#ifdef HAVE_STREAMS_SENDFD
# include <stropts.h>
#endif
#include <syslog.h>

#include "inn/messages.h"
#include "inn/vector.h"
#include "inn/libinn.h"

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

/* Holds the information about a network socket to bind. */
struct binding {
    int fd;
    int family;
    char *address;
    unsigned short port;
};


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
**  Parse a command-line argument into a struct binding.  The command line
**  argument is four comma-separated values:  the file descriptor, the family,
**  the listening address, and the port number.  The caller is responsible for
**  freeing the address attribute of the supplied binding struct, although if
**  a binding struct is passed in for use and has a non-NULL address, it will
**  be freed first.
*/
static void
parse_argument(const char *string, struct binding *binding)
{
    struct vector *spec;
    long value;

    /* Do the initial parse and allocate our data structures. */
    spec = vector_split(string, ',', NULL);
    if (spec->count != 4)
        die("invalid command-line argument %s", string);

    /* Get the file descriptor, address family, and port. */
    if (!convert_string(spec->strings[0], &value))
        die("invalid file descriptor %s in %s", spec->strings[0], string);
    binding->fd = value;
    if (!convert_string(spec->strings[1], &value))
        die("invalid protocol family %s in %s", spec->strings[1], string);
    binding->family = value;
    if (binding->address != NULL)
        free(binding->address);
    binding->address = xstrdup(spec->strings[2]);
    if (!convert_string(spec->strings[3], &value))
        die("invalid port number %s in %s", spec->strings[3], string);
    if (value == 0)
        die("port may not be zero in %s", string);
    binding->port = value;

    /* Done.  Clean up. */
    vector_free(spec);
}    


/*
**  Bind an IPv4 address, given the file descriptor, string giving the
**  address, and the port.  The fourth argument is the full binding
**  specification for error reporting.  Returns true on success, false if
**  binding failed due to permission denied.  Die on any other failure.
*/
static bool
bind_ipv4(int fd, const char *address, unsigned short port, const char *spec)
{
    struct sockaddr_in server;
    struct in_addr addr;

    memset(&server, '\0', sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if (!inet_aton(address, &addr))
        die("invalid IPv4 address %s in %s", address, spec);
    server.sin_addr = addr;
    sin_set_length(&server);
    if (bind(fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        if (errno == EACCES)
            return false;
        else
            sysdie("cannot bind socket for %s", spec);
    }
    return true;
}


/*
**  Bind an IPv6 address, given the file descriptor, string giving the
**  address, and the port.  The fourth argument is the full binding
**  specification for error reporting.  Returns true on success, false if
**  binding failed due to permission denied.  Die on any other failure.
*/
#ifdef HAVE_INET6
static bool
bind_ipv6(int fd, const char *address, unsigned short port, const char *spec)
{
    struct sockaddr_in6 server;
    struct in6_addr addr;

    memset(&server, '\0', sizeof(server));
    server.sin6_family = AF_INET6;
    server.sin6_port = htons(port);
    if (inet_pton(AF_INET6, address, &addr) < 1)
        die("invalid IPv6 address %s in %s", address, spec);
    server.sin6_addr = addr;
    sin6_set_length(&server);
    if (bind(fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        if (errno == EACCES)
            return false;
        else
            sysdie("cannot bind socket for %s", spec);
    }
    return true;
}
#endif /* HAVE_INET6 */


/*
**  Given a given a struct binding, bind that file descriptor.  Also takes the
**  command-line argument for error reporting.  Returns true on success, false
**  if binding failed due to permission denied.  Die on any other failure.
*/
static bool
bind_address(struct binding *binding, const char *spec)
{
    int fd = binding->fd;
    unsigned short port = binding->port;
    int type;
    socklen_t length;

    /* Make sure that we're allowed to bind to that port. */
    if (port < 1024 && port != 119 && port != 433 && port != INND_PORT)
        die("cannot bind to restricted port %hu in %s", port, spec);

    /* Sanity check on the socket. */
    length = sizeof(type);
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &length) < 0)
        sysdie("cannot get socket options for file descriptor %d", fd);
    if (type != SOCK_STREAM)
        die("invalid file descriptor %d: not SOCK_STREAM", fd);

    /* Based on the address family, parse either an IPv4 or IPv6 address. */
    if (binding->family == AF_INET)
        return bind_ipv4(fd, binding->address, port, spec);
#ifdef HAVE_INET6
    else if (binding->family == AF_INET6)
        return bind_ipv6(fd, binding->address, port, spec);
#endif
    else
        die("unknown protocol family %d in %s", binding->family, spec);
}


/*
**  Given a struct binding, create a socket for it and fill in the file
**  descriptor.  Also takes the command-line argument for error reporting.
**  Dies on any failure.
*/
static void
create_socket(struct binding *binding, const char *spec)
{
    int fd;
#ifdef SO_REUSEADDR
    int flag;
#endif

    /* Create the socket. */
    if (binding->family == AF_INET)
        fd = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
#ifdef HAVE_INET6
    else if (binding->family == AF_INET6)
        fd = socket(PF_INET6, SOCK_STREAM, IPPROTO_IP);
#endif
    else
        die("unknown protocol family %d in %s", binding->family, spec);
    if (fd < -1)
        sysdie("cannot create socket for %s", spec);

    /* Mark it reusable if possible. */
#ifdef SO_REUSEADDR
    flag = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0)
        sysdie("cannot mark socket reusable for %s", spec);
#endif

    /* Fill in the struct. */
    binding->fd = fd;
}


/*
**  Given a file descriptor, attempt to pass it via stdout.  Die on any error.
**  Currently, file descriptor passing is only implemented for systems that
**  use STREAMS.
*/
#ifdef HAVE_STREAMS_SENDFD
static void
send_fd(int fd)
{
    if (isastream(STDOUT_FILENO) != 1)
        die("cannot pass file descriptor: stdout is not a stream");
    if (ioctl(STDOUT_FILENO, I_SENDFD, fd) < 0)
        sysdie("cannot pass file descriptor");
}
#else /* !HAVE_STREAMS_SENDFD */
static void
send_fd(int fd UNUSED)
{
    die("cannot pass file descriptor: STREAMS not supported");
}
#endif /* !HAVE_STREAMS_SENDFD */


int
main(int argc, char *argv[])
{
    struct passwd *pwd;
    uid_t real_uid;
    int i;
    bool done;
    struct binding binding = { 0, 0, NULL, 0 };
    bool force_sendfd = false;

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

    /* If the first argument is -p, force creation of the socket and file
       descriptor passing rather than even attempting to bind the socket
       first. */
    if (argc > 1 && strcmp(argv[1], "-p") == 0) {
        force_sendfd = true;
        argc--;
        argv++;
    }
    if (argc < 2)
        die("no addresses specified");

    /* Walk the argument list and try to bind each argument.  For each
       successful bind, print "ok\n" to stdout.  For each bind that fails with
       permission denied, print "no\n" to stdout and then immediately attempt
       to create a new file descriptor and pass it back over stdout. */
    for (i = 1; i < argc; i++) {
        parse_argument(argv[i], &binding);
        done = false;
        if (!force_sendfd)
            done = bind_address(&binding, argv[i]);
        if (done)
            write(STDOUT_FILENO, "ok\n", 3);
        else {
            write(STDOUT_FILENO, "no\n", 3);
            create_socket(&binding, argv[i]);
            if (!bind_address(&binding, argv[i]))
                sysdie("cannot bind socket for %s", argv[i]);
            send_fd(binding.fd);
        }
    }
    exit(0);
}
