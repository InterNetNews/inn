/*  $Id$
**
**  Check an username and password against Kerberos v5.
**
**  Based on nnrpkrb5auth by Christopher P. Lindsey <lindsey@mallorn.com>
**  See <http://www.mallorn.com/tools/nnrpkrb5auth>
**
**  This program takes a username and password pair from nnrpd and checks
**  checks their validity against a Kerberos v5 KDC by attempting to obtain a
**  TGT.  With the -i <instance> command line option, appends /<instance> to
**  the username prior to authentication.
**
**  Special thanks to Von Welch <vwelch@vwelch.com> for giving me the initial
**  code on which the Kerberos V authentication is based many years ago, and
**  for introducing me to Kerberos back in '96.
**
**  Also, thanks to Graeme Mathieson <graeme@mathie.cx> for his inspiration
**  through the pamckpasswd program.
*/

#include "config.h"
#include "clibrary.h"
#include "libauth.h"
#ifdef HAVE_ET_COM_ERR_H
# include <et/com_err.h>
#else
# include <com_err.h>
#endif

#include <krb5.h>

#include "inn/messages.h"
#include "inn/libinn.h"

/*
**  Check the username and password by attempting to get a TGT.  Returns 1 on
**  success and 0 on failure.  Errors are reported via com_err.
*/
static int
krb5_check_password(const char *principal, const char *password)
{
    krb5_error_code code;
    krb5_context ctx;
    krb5_creds creds;
    krb5_principal princ = NULL;
    krb5_get_init_creds_opt opts;
    bool creds_valid = false;
    int result = 0;

    code = krb5_init_context(&ctx);
    if (code != 0) {
        com_err(message_program_name, code, "initializing krb5 context");
        return 0;
    }
    code = krb5_parse_name(ctx, principal, &princ);
    if (code != 0) {
        com_err(message_program_name, code, "parsing principal name %.100s",
                principal);
        goto cleanup;
    }
    memset(&opts, 0, sizeof(opts));
    krb5_get_init_creds_opt_init(&opts);
    krb5_get_init_creds_opt_set_forwardable(&opts, 0);
    krb5_get_init_creds_opt_set_proxiable(&opts, 0);
    code = krb5_get_init_creds_password(ctx, &creds, princ, (char *) password,
                                        NULL, NULL, 0, NULL, &opts);
    if (code == 0) {
        krb5_verify_init_creds_opt vopts;

        creds_valid = true;
        memset(&opts, 0, sizeof(vopts));
        krb5_verify_init_creds_opt_init(&vopts);
        code = krb5_verify_init_creds(ctx, &creds, princ, NULL, NULL, &vopts);
    }
    if (code == 0)
        result = 1;
    else {
        switch (code) {
        case KRB5KRB_AP_ERR_BAD_INTEGRITY:
            com_err(message_program_name, 0, "bad password for %.100s",
                    principal);
            break;
        case KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN:
            com_err(message_program_name, 0, "unknown user %.100s",
                    principal);
            break;
        default:
            com_err(message_program_name, code,
                    "checking Kerberos password for %.100s", principal);
        }
    }
   
cleanup:
    if (creds_valid)
        krb5_free_cred_contents(ctx, &creds);
    if (princ != NULL)
        krb5_free_principal(ctx, princ);
    krb5_free_context(ctx);
    return result;
}

int
main (int argc, char *argv[])
{
    struct auth_info *authinfo;
    char *new_user;

    message_program_name = "auth_krb5";

    /* Retrieve the username and passwd from nnrpd. */
    authinfo = get_auth_info(stdin);

    /* Must have a username/password, and no '@' in the address.  @ checking
      is there to prevent authentication against another Kerberos realm; there
      should be a -r <realm> command line option to make this check unnecessary
      in the future. */
    if (authinfo == NULL)
        die("no authentication information from nnrpd");
    if (authinfo->username[0] == '\0')
        die("null username");
    if (strchr(authinfo->username, '@') != NULL)
        die("username contains @, not allowed");

    /* May need to prepend instance name if -i option was given. */
    if (argc > 1) {
        if (argc == 3 && strcmp(argv[1], "-i") == 0) {
            new_user = concat(authinfo->username, "/", argv[2], (char *) 0);
            free(authinfo->username);
            authinfo->username = new_user;
        } else {
            die("error parsing command-line options");
        }
    }

    if (krb5_check_password(authinfo->username, authinfo->password)) {
        print_user(authinfo->username);
        exit(0);
    } else {
        die("failure validating password");
    }
}
