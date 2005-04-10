/*  $Id$
**
**  Common code for authenticators and resolvers.
**
**  Collects common code to read information from nnrpd that should be done
**  the same for all authenticators, and common code to get information about
**  the incoming connection.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"

#include "inn/messages.h"
#include "libauth.h"
#include "libinn.h"

#define NAMESTR "ClientAuthname: "
#define PASSSTR "ClientPassword: "

#define CLIHOST "ClientHost: "
#define CLIIP "ClientIP: "
#define CLIPORT "ClientPort: "
#define LOCIP "LocalIP: "
#define LOCPORT "LocalPort: "

/*
**  Main loop.  If res != NULL, expects to get resolver info from nnrpd, and
**  writes it into the struct.  If auth != NULL, expects to get authentication
**  info from nnrpd, and writes it into the struct.
*/
static bool
get_connection_info(FILE *stream, struct res_info *res, struct auth_info *auth)
{
    char buff[SMBUF];
    size_t length;

    /* Zero fields first (anything remaining NULL after is missing data). */
    if (res != NULL) {
        res->clienthostname = NULL;
        res->clientip = NULL;
        res->clientport = NULL;
        res->localip = NULL;
        res->localport = NULL;
    }
    if (auth != NULL) {
        auth->username = NULL;
        auth->password = NULL;
    }

    /* Read input from nnrpd a line at a time, stripping \r\n. */
    while (fgets(buff, sizeof(buff), stream) != NULL) {
        length = strlen(buff);
        if (length == 0 || buff[length - 1] != '\n')
            return false;
        buff[length - 1] = '\0';
        if (length > 1 && buff[length - 2] == '\r')
            buff[length - 2] = '\0';

        /* Parse */
        if (strncmp(buff, ".", 2) == 0)
            break;
        else if (auth != NULL && strncmp(buff, NAMESTR, strlen(NAMESTR)) == 0)
            auth->username = xstrdup(buff + strlen(NAMESTR));
        else if (auth != NULL && strncmp(buff, PASSSTR, strlen(PASSSTR)) == 0)
            auth->password = xstrdup(buff + strlen(PASSSTR));
        else if (res != NULL && strncmp(buff, CLIHOST, strlen(CLIHOST)) == 0)
            res->clienthostname = xstrdup(buff + strlen(CLIHOST));
        else if (res != NULL && strncmp(buff, CLIIP, strlen(CLIIP)) == 0)
            res->clientip = xstrdup(buff + strlen(CLIIP));
        else if (res != NULL && strncmp(buff, CLIPORT, strlen(CLIPORT)) == 0)
            res->clientport = xstrdup(buff + strlen(CLIPORT));
        else if (res != NULL && strncmp(buff, LOCIP, strlen(LOCIP)) == 0)
            res->localip = xstrdup(buff + strlen(LOCIP));
        else if (res != NULL && strncmp(buff, LOCPORT, strlen(LOCPORT)) == 0)
            res->localport = xstrdup(buff + strlen(LOCPORT));
        else {
            debug("libauth: unexpected data from nnrpd: \"%s\"", buff);
        }
    }

    /* If some field is missing, free the rest and error out. */
    if (auth != NULL && (auth->username == NULL || auth->password == NULL)) {
        warn("libauth: requested authenticator data not sent by nnrpd");
        return false;
    }
    if (res != NULL && (res->clienthostname == NULL || res->clientip == NULL
                        || res->clientport == NULL || res->localip == NULL
                        || res->localport == NULL)) {
        warn("libauth: requested resolver data not sent by nnrpd");
        return false;
    }
    return true;
}


/*
**  Free a struct res_info, including all of its members.
*/
void
free_res_info(struct res_info *res)
{
    if (res == NULL)
        return;
    if (res->clientip != NULL)
        free(res->clientip);
    if (res->clientport != NULL)
        free(res->clientport);
    if (res->localip != NULL)
        free(res->localip);
    if (res->localport != NULL)
        free(res->localport);
    if (res->clienthostname != NULL)
        free(res->clienthostname);
    free(res);
}


/*
**  Free a struct auth_info, including all of its members.
*/
void
free_auth_info(struct auth_info *auth)
{
    if (auth == NULL)
        return;
    if (auth->username != NULL)
        free(auth->username);
    if (auth->password != NULL)
        free(auth->password);
    free(auth);
}


/*
**  Read resolver information from nnrpd, returning an allocated struct on
**  success.
*/
struct res_info *
get_res_info(FILE *stream)
{
    struct res_info *res = xmalloc(sizeof(struct res_info));

    if (get_connection_info(stream, res, NULL))
        return res;
    free_res_info(res);
    return NULL;
}


/*
**  Read authenticator information from nnrpd, returning an allocated struct
**  on success.
*/
struct auth_info *
get_auth_info(FILE *stream)
{
    struct auth_info *auth = xmalloc(sizeof(struct auth_info));

    if (get_connection_info(stream, NULL, auth))
        return auth;
    free_auth_info(auth);
    return NULL;
}


/*
**  Print the User: result on standard output in the format expected by
**  nnrpd.  The string passed in should be exactly the user, with no
**  extraneous leading or trailing whitespace.
*/
void
print_user(const char *user)
{
    printf("User:%s\r\n", user);
}
