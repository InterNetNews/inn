/*
 * Samba authenticator.
 * usage: auth_smb <server> [<backup_server>] <domain>
 *
 * Heavily based on:
 * pam_smb -- David Airlie 1998-2000 v1.1.6 <airlied@samba.org>
 * http://www.csn.ul.ie/~airlied
 *
 * Written 2000 October by Krischan Jodies <krischan@jodies.cx>
 * 
 */

#include "config.h"
#include "clibrary.h"
#include "inn/messages.h"

#include "libauth.h"
#include "smbval/valid.h"

int
main(int argc, char *argv[])
{
    struct authinfo *authinfo;
    int result;
    char *server, *backup, *domain;

    message_program_name = "auth_smb";

    if ((argc > 4) || (argc < 3))
        die("wrong number of arguments"
            " (auth_smb <server> [<backup-server>] <domain>");

    authinfo = get_auth();
    if (authinfo == NULL)
        die("no user information provided by nnrpd");

    /* Got a username and password.  Now check to see if they're valid. */
    server = argv[1];
    backup = (argc > 3) ? argv[2] : argv[1];
    domain = (argc > 3) ? argv[3] : argv[2];
    result = Valid_User(authinfo->username, authinfo->password, argv[1],
                        backup, domain);

    /* Analyze the result. */
    switch (result) {
    case NTV_NO_ERROR:
        printf("User:%s\n", authinfo->username);
        exit(0);
        break;
    case NTV_SERVER_ERROR:
        die("server error");
        break;
    case NTV_PROTOCOL_ERROR:
        die("protocol error");
        break;
    case NTV_LOGON_ERROR:
        die("logon error");
        break;
    default:
        die("unknown error");
        break;
    }

    /* Never reached. */
    return 1;
}
