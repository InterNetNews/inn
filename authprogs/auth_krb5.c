/*
**  Check an username and password against Kerberos.
**
**  Based on nnrpkrb5auth by Christopher P. Lindsey <lindsey@mallorn.com>
**  See <http://www.mallorn.com/tools/nnrpkrb5auth>
**
**  This program takes a username and password pair from nnrpd and checks
**  their validity against a Kerberos KDC by attempting to obtain a TGT.
**  With the -i <instance> command line option, appends /<instance> to
**  the username prior to authentication.
**
**  Special thanks to Von Welch <vwelch@vwelch.com> for giving me the initial
**  code on which the Kerberos V authentication is based many years ago, and
**  for introducing me to Kerberos back in '96.
**
**  Also, thanks to Graeme Mathieson <graeme@mathie.cx> for his inspiration
**  through the pamckpasswd program.
*/

#include "portable/system.h"

#include "libauth.h"

#if defined(HAVE_KRB5_H)
#    include <krb5.h>
#elif defined(HAVE_KERBEROSV5_KRB5_H)
#    include <kerberosv5/krb5.h>
#else
#    include <krb5/krb5.h>
#endif

/* Figure out what header files to include for error reporting. */
#if !defined(HAVE_KRB5_GET_ERROR_MESSAGE) && !defined(HAVE_KRB5_GET_ERR_TEXT)
#    if !defined(HAVE_KRB5_GET_ERROR_STRING)
#        if defined(HAVE_IBM_SVC_KRB5_SVC_H)
#            include <ibm_svc/krb5_svc.h>
#        elif defined(HAVE_ET_COM_ERR_H)
#            include <et/com_err.h>
#        elif defined(HAVE_KERBEROSV5_COM_ERR_H)
#            include <kerberosv5/com_err.h>
#        else
#            include <com_err.h>
#        endif
#    endif
#endif

#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/xmalloc.h"

/*
 * This string is returned for unknown error messages.  We use a static
 * variable so that we can be sure not to free it.
 */
#if !defined(HAVE_KRB5_GET_ERROR_MESSAGE) \
    || !defined(HAVE_KRB5_FREE_ERROR_MESSAGE)
static const char error_unknown[] = "unknown error";
#endif


#ifndef HAVE_KRB5_GET_ERROR_MESSAGE
/*
 * Given a Kerberos error code, return the corresponding error.  Prefer the
 * Kerberos interface if available since it will provide context-specific
 * error information, whereas the error_message() call will only provide a
 * fixed message.
 */
const char *
krb5_get_error_message(krb5_context ctx UNUSED, krb5_error_code code UNUSED)
{
    const char *msg;

#    if defined(HAVE_KRB5_GET_ERROR_STRING)
    msg = krb5_get_error_string(ctx);
#    elif defined(HAVE_KRB5_GET_ERR_TEXT)
    msg = krb5_get_err_text(ctx, code);
#    elif defined(HAVE_KRB5_SVC_GET_MSG)
    krb5_svc_get_msg(code, (char **) &msg);
#    else
    msg = error_message(code);
#    endif
    if (msg == NULL)
        return error_unknown;
    else
        return msg;
}
#endif /* !HAVE_KRB5_GET_ERROR_MESSAGE */


#ifndef HAVE_KRB5_FREE_ERROR_MESSAGE
/*
 * Free an error string if necessary.  If we returned a static string, make
 * sure we don't free it.
 *
 * This code assumes that the set of implementations that have
 * krb5_free_error_message is a subset of those with krb5_get_error_message.
 * If this assumption ever breaks, we may call the wrong free function.
 */
void
krb5_free_error_message(krb5_context ctx UNUSED, const char *msg)
{
    if (msg == error_unknown)
        return;
#    if defined(HAVE_KRB5_GET_ERROR_STRING)
    krb5_free_error_string(ctx, (char *) msg);
#    elif defined(HAVE_KRB5_SVC_GET_MSG)
    krb5_free_string(ctx, (char *) msg);
#    endif
}
#endif /* !HAVE_KRB5_FREE_ERROR_MESSAGE */


/*
**  Report a Kerberos error to standard error.
*/
static void __attribute__((__format__(printf, 3, 4)))
warn_krb5(krb5_context ctx, krb5_error_code code, const char *format, ...)
{
    const char *k5_msg;
    char *message;
    va_list args;

    k5_msg = krb5_get_error_message(ctx, code);
    va_start(args, format);
    xvasprintf(&message, format, args);
    va_end(args);
    if (k5_msg == NULL)
        warn("%s", message);
    else
        warn("%s: %s", message, k5_msg);
    free(message);
    if (k5_msg != NULL)
        krb5_free_error_message(ctx, k5_msg);
}


/*
**  Check the username and password by attempting to get a TGT.  Returns 1 on
**  success and 0 on failure.
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
        warn_krb5(NULL, code, "cannot initialize Kerberos");
        return 0;
    }
    code = krb5_parse_name(ctx, principal, &princ);
    if (code != 0) {
        warn_krb5(ctx, code, "cannot parse principal %.100s", principal);
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
            warn("bad password for %.100s", principal);
            break;
        case KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN:
            warn("unknown user %.100s", principal);
            break;
        default:
            warn_krb5(ctx, code, "Kerberos authentication for %.100s failed",
                      principal);
            break;
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
main(int argc, char *argv[])
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
            xasprintf(&new_user, "%s/%s", authinfo->username, argv[2]);
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
