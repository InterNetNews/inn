/*  $Id$
**
**  Send a LIST command to an NNTP server and print the results.
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/nntp.h"
#include "inn/qio.h"
#include "inn/vector.h"
#include "inn/libinn.h"
#include "inn/paths.h"

static const char usage[] = "\
Usage: getlist [-A] [-h host] [-p port] [list [pattern [types]]]\n\
\n\
getlist obtains a list from an NNTP server and prints it out.  By default,\n\
the active file is retrieved, but any list that the NNTP server supports\n\
can be requested.  The pattern is passed to the remote server as a filter\n\
on the returned values.  If the list parameter is \"active\", a third\n\
parameter can be given, listing the acceptable group types.  Only groups of\n\
that type (y and m being the most common) are returned.\n";


/*
**  Print out an appropriate error message for a bad NNTP response code and
**  exit.
*/
static void
die_nntp_code(enum nntp_code code, const char *line)
{
    if (code == 0)
        die("unexpected server response: %s", line);
    else
        die("unexpected server response: %03d %s", code, line);
}


/*
**  Print out an appropriate error message for a bad NNTP status and exit.
*/
static void
die_nntp_status(enum nntp_status status)
{
    switch (status) {
    case NNTP_READ_LONG:
        die("line from server too long for 128KB buffer");
    case NNTP_READ_EOF:
        die("server closed connection unexpectedly");
    case NNTP_READ_TIMEOUT:
        die("read from server timed out");
    default:
        sysdie("cannot read response from server");
    }
}


/*
**  Send a command and pattern to the remote server and make sure we get an
**  appropriate response.
*/
static void
send_list(struct nntp *nntp, const char *command, const char *pattern)
{
    bool okay;
    enum nntp_status status;
    enum nntp_code code;
    char *line;

    if (pattern != NULL)
        okay = nntp_send_line(nntp, "LIST %s %s", command, pattern);
    else
        okay = nntp_send_line(nntp, "LIST %s", command);
    if (!okay)
        sysdie("cannot send LIST command to server");
    status = nntp_read_response(nntp, &code, &line);
    if (status != NNTP_READ_OK)
        die_nntp_status(status);
    if (code != NNTP_OK_LIST)
        die_nntp_code(code, line);
}


/*
**  Print out the results of a LIST command.  Used for generic list commands
**  or active commands without a type parameter.
*/
static void
print_list(struct nntp *nntp)
{
    enum nntp_status status;
    char *line;

    status = nntp_read_line(nntp, &line);
    while (status == NNTP_READ_OK) {
        if (strcmp(line, ".") == 0)
            break;
        printf("%s\n", line);
        status = nntp_read_line(nntp, &line);
    }
    if (status != NNTP_READ_OK)
        die_nntp_status(status);
}


/*
**  Print out the results of a LIST ACTIVE command, limited to particular
**  group types.
*/
static void
print_active(struct nntp *nntp, const char *types)
{
    enum nntp_status status;
    char *line;
    struct cvector *group = NULL;

    status = nntp_read_line(nntp, &line);
    while (status == NNTP_READ_OK) {
        if (strcmp(line, ".") == 0)
            break;
        group = cvector_split_space(line, group);
        if (group->count != 4) {
            warn("malformed line from server: %s", line);
            continue;
        }
        if (strchr(types, group->strings[3][0]) != NULL)
            printf("%s %s %s %s\n", group->strings[0], group->strings[1],
                   group->strings[2], group->strings[3]);
        status = nntp_read_line(nntp, &line);
    }
    if (status != NNTP_READ_OK)
        die_nntp_status(status);
}


/*
**  Get the username and password for a remote site from the local password
**  file.  username and password will be set to newly allocated strings on
**  success.
*/
static bool
get_authinfo(const char *server, char **username, char **password)
{
    char *path, *line;
    QIOSTATE *passwords;
    struct cvector *info = NULL;

    path = concatpath(innconf->pathetc, INN_PATH_NNTPPASS);
    passwords = QIOopen(path);
    if (passwords == NULL) {
        if (errno != ENOENT)
            warn("cannot open %s", path);
        return false;
    }
    free(path);
    while ((line = QIOread(passwords)) != NULL) {
        if (line[0] == '\0' || line[0] == '#')
            continue;
        info = cvector_split(line, ':', info);
        if (info->count > 4 || info->count < 3)
            continue;
        if (info->count == 4 && strcmp(info->strings[3], "authinfo") != 0)
            continue;
        if (strcasecmp(info->strings[0], server) != 0)
            continue;
        *username = xstrdup(info->strings[1]);
        *password = xstrdup(info->strings[2]);
        return true;
    }
    return false;
}


/*
**  Send AUTHINFO information to a remote site.  Returns true if successful,
**  false on failure.  Problems sending to the remote site (as opposed to just
**  having the remote site reject the authentication) are fatal.
*/
static bool
send_authinfo(struct nntp *nntp, const char *username, const char *password)
{
    enum nntp_status status;
    enum nntp_code code;
    char *line;

    if (!nntp_send_line(nntp, "AUTHINFO USER %s", username))
        sysdie("cannot send AUTHINFO USER to remote server");
    status = nntp_read_response(nntp, &code, &line);
    if (status != NNTP_READ_OK)
        die_nntp_status(status);
    if (code == NNTP_OK_AUTHINFO)
        return true;
    if (code != NNTP_CONT_AUTHINFO)
        return false;
    if (!nntp_send_line(nntp, "AUTHINFO PASS %s", password))
        sysdie("cannot send AUTHINFO PASS to remote server");
    status = nntp_read_response(nntp, &code, &line);
    if (status != NNTP_READ_OK)
        die_nntp_status(status);
    return (code == NNTP_OK_AUTHINFO);
}


int
main(int argc, char *argv[])
{
    struct nntp *nntp;
    const char *host = NULL;
    const char *list = "active";
    const char *pattern = NULL;
    const char *types = NULL;
    enum nntp_status status;
    enum nntp_code response;
    char *line;
    unsigned short port = NNTP_PORT;
    bool authinfo = false;
    int option;

    message_program_name = "getlist";
    if (!innconf_read(NULL))
        exit(1);
    host = innconf->server;

    /* Parse options. */
    while ((option = getopt(argc, argv, "Ah:p:")) != EOF) {
        switch (option) {
        case 'A':
            authinfo = true;
            break;
        case 'h':
            host = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            if (port <= 0)
                die("%s is not a valid port number", optarg);
            break;
        default:
            exit(1);
        }
    }
    argc -= optind;
    argv += optind;

    /* Read optional arguments. */
    if (argc > 3)
        die("too many arguments");
    if (argc >= 1)
        list = argv[0];
    if (argc >= 2)
        pattern = argv[1];
    if (argc == 3)
        types = argv[2];
    if (strcmp(list, "active") != 0 && types != NULL)
        die("group types can only be specified with a list type of active");

    /* Connect to the server. */
    if (host == NULL)
        sysdie("cannot get server name");
    nntp = nntp_connect(host, port, 128 * 1024, 10 * 60);
    if (nntp == NULL)
        sysdie("cannot connect to server %s:%hu", host, port);
    status = nntp_read_response(nntp, &response, &line);
    if (status != NNTP_READ_OK)
        die_nntp_status(status);
    if (response < 200 || response > 201)
        die_nntp_code(response, line);

    /* Authenticate if desired. */
    if (authinfo) {
        char *username, *password;

        if (get_authinfo(host, &username, &password)) {
            if (!send_authinfo(nntp, username, password))
                warn("server %s did not accept authentication", host);
            free(username);
            free(password);
        } else
            warn("no authentication information found for %s", host);
    }

    /* Get and display the data. */
    send_list(nntp, list, pattern);
    if (types != NULL)
        print_active(nntp, types);
    else
        print_list(nntp);

    /* Be polite and say goodbye; it gives the server a chance to shut the
       connection down cleanly. */
    if (nntp_send_line(nntp, "QUIT"))
        status = nntp_read_response(nntp, &response, &line);
    nntp_free(nntp);
    exit(0);
}
