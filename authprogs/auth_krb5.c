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
#include <com_err.h>

/* krb5_get_in_tkt_with_password is deprecated. */
#define KRB5_DEPRECATED 1
#include <krb5.h>

#include "inn/messages.h"
#include "libinn.h"

/*
 * Default life of the ticket we are getting. Since we are just checking
 * to see if the user can get one, it doesn't need a long lifetime.
 */
#define KRB5_DEFAULT_LIFE    60 * 5 /* 5 minutes */


/*
**  Check the username and password by attempting to get a TGT.  Returns 1 on
**  success and 0 on failure.  Errors are reported via com_err.
*/
static int
krb5_check_password (char *principal_name, char *password)
{
   krb5_context      kcontext;
   krb5_creds        creds;
   krb5_principal    user_principal;
   krb5_data         *user_realm;
   krb5_principal    service_principal;
   krb5_timestamp    now;
   krb5_address      **addrs = (krb5_address **) NULL;   /* Use default */
   long              lifetime = KRB5_DEFAULT_LIFE;
   int               options = 0;

   krb5_preauthtype  *preauth = NULL;

   krb5_error_code   code;

   /* Our return code - 1 is success */
   int                result = 0;
   
   /* Initialize our Kerberos state */
   code = krb5_init_context (&kcontext);
   if (code) {
       com_err (message_program_name, code, "initializing krb5 context");
       return 0;
   }
   
#ifdef HAVE_KRB5_INIT_ETS
   /* Initialize krb5 error tables */    
   krb5_init_ets (kcontext);
#endif

   /* Get current time */
   code = krb5_timeofday (kcontext, &now);
   if (code) {
       com_err (message_program_name, code, "getting time of day");
       return 0;
   }

   /* Set up credentials to be filled in */
   memset (&creds, 0, sizeof(creds));

   /* From here on, goto cleanup to exit */

   /* Parse the username into a krb5 principal */
   if (!principal_name) {
       com_err (message_program_name, 0, "passed NULL principal name");
       goto cleanup;
   }

   code = krb5_parse_name (kcontext, principal_name, &user_principal);
   if (code) {
       com_err (message_program_name, code,
                "parsing user principal name %.100s", principal_name);
       goto cleanup;
   }

   creds.client = user_principal;

   /* Get the user's realm for building service principal */
   user_realm = krb5_princ_realm (kcontext, user_principal);
   
   /*
    * Build the service name into a principal. Right now this is
    * a TGT for the user's realm.
    */
   code = krb5_build_principal_ext (kcontext,
               &service_principal,
               user_realm->length,
               user_realm->data,
               KRB5_TGS_NAME_SIZE,
               KRB5_TGS_NAME,
               user_realm->length,
               user_realm->data,
               0 /* terminator */);
   if (code) {
       com_err(message_program_name, code, "building service principal name");
       goto cleanup;
   }

   creds.server = service_principal;

   creds.times.starttime = 0;   /* Now */
   creds.times.endtime = now + lifetime;
   creds.times.renew_till = 0;   /* Unrenewable */

   /* DO IT */
   code = krb5_get_in_tkt_with_password (kcontext,
               options,
               addrs,
               NULL,
               preauth,
               password,
               0,
               &creds,
               0);
   
   /* We are done with password at this point... */

   if (code) {   
      /* FAILURE - Parse a few common errors here */
      switch (code) {
      case KRB5KRB_AP_ERR_BAD_INTEGRITY:
         com_err (message_program_name, 0, "bad password for %.100s",
                  principal_name);
         break;
      case KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN:
         com_err (message_program_name, 0, "unknown user \"%.100s\"",
                  principal_name);
         break;
      default:
         com_err (message_program_name, code,
                  "checking Kerberos password for %.100s", principal_name);
      }
      result = 0;
   } else {
      /* SUCCESS */
      result = 1;
   }
   
   /* Cleanup */
 cleanup:
   krb5_free_cred_contents (kcontext, &creds);

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
      should be a -r <realm> commandline option to make this check unnecessary
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
        printf("User:%s\r\n", authinfo->username);
        exit(0);
    } else {
        die("failure validating password");
    }
}
