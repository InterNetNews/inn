/*  $Id$
**
**  The default username/password authenticator.
**
**  This program is intended to be run by nnrpd and handle usernames and
**  passwords.  It can authenticate against a regular flat file (the type
**  managed by htpasswd), a DBM file, the system password file or shadow file,
**  or PAM.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/messages.h"
#include "inn/qio.h"
#include "inn/vector.h"
#include "libinn.h"

#include "libauth.h"

#if HAVE_CRYPT_H
# include <crypt.h>
#endif
#include <fcntl.h>
#include <pwd.h>

#if HAVE_DBM
# if HAVE_NDBM_H
#  include <ndbm.h>
# elif HAVE_DB1_NDBM_H
#  include <db1/ndbm.h>
# endif
# define OPT_DBM "d:"
#else
# define OPT_DBM ""
#endif

#if HAVE_GETSPNAM
# include <shadow.h>
# define OPT_SHADOW "s"
#else
# define OPT_SHADOW ""
#endif

#if HAVE_PAM
# include <security/pam_appl.h>
#endif


/*
**  The PAM conversation function.
**
**  Since we already have all the information and can't ask the user
**  questions, we can't quite follow the real PAM protocol.  Instead, we just
**  return the password in response to every question that PAM asks.  There
**  appears to be no generic way to determine whether the message in question
**  is indeed asking for the password....
**
**  This function allocates an array of struct pam_response to return to the
**  PAM libraries that's never freed.  For this program, this isn't much of an
**  issue, since it will likely only be called once and then the program will
**  exit.
**
**  appdata_ptr contains the password we were given.
*/
#if HAVE_PAM
static int
pass_conv(int num_msg, const struct pam_message **msgm UNUSED,
          struct pam_response **response, void *appdata_ptr)
{
    int i;

    *response = malloc(num_msg * sizeof(struct pam_response));
    if (*response == NULL)
        return PAM_CONV_ERR;
    for (i = 0; i < num_msg; i++) {
        (*response)[i].resp = strdup((char *)appdata_ptr);
        (*response)[i].resp_retcode = 0;
    }
    return PAM_SUCCESS;
}
#endif /* HAVE_PAM */


/*
**  Authenticate a user via PAM.
**
**  Attempts to authenticate a user with PAM, returning true if the user
**  successfully authenticates and false otherwise.  Note that this function
**  doesn't attempt to handle any remapping of the authenticated user by the
**  PAM stack, but just assumes that the authenticated user was the same as
**  the username given.
**
**  Right now, all failures are handled via die.  This may be worth revisiting
**  in case we want to try other authentication methods if this fails for a
**  reason other than the system not having PAM support.
*/
#if !HAVE_PAM
static bool
auth_pam(char *username UNUSED, char *password UNUSED)
{
    return false;
}
#else
static bool
auth_pam(const char *username, char *password)
{
    pam_handle_t *pamh;
    struct pam_conv conv;
    int status;

    conv.conv = pass_conv;
    conv.appdata_ptr = password;
    status = pam_start("nnrpd", username, &conv, &pamh);
    if (status != PAM_SUCCESS)
        die("pam_start failed: %s", pam_strerror(pamh, status));
    status = pam_authenticate(pamh, PAM_SILENT);
    if (status != PAM_SUCCESS)
        die("pam_authenticate failed: %s", pam_strerror(pamh, status));
    status = pam_acct_mgmt(pamh, PAM_SILENT);
    if (status != PAM_SUCCESS)
        die("pam_acct_mgmt failed: %s", pam_strerror(pamh, status));
    status = pam_end(pamh, status);
    if (status != PAM_SUCCESS)
        die("pam_end failed: %s", pam_strerror(pamh, status));

    /* If we get to here, the user successfully authenticated. */
    return true;
}
#endif /* HAVE_PAM */


/*
**  Try to get a password out of a dbm file.  The dbm file should have the
**  username for the key and the crypted password as the value.  The crypted
**  password, if found, is returned as a newly allocated string; otherwise,
**  NULL is returned.
*/
#if !HAVE_DBM
static char *
password_dbm(char *user UNUSED, const char *file UNUSED)
{
    return NULL;
}
#else
static char *
password_dbm(char *name, const char *file)
{
    datum key, value;
    DBM *database;
    char *password;

    database = dbm_open(file, O_RDONLY, 0600);
    if (database == NULL)
        return NULL;
    key.dptr = name;
    key.dsize = strlen(name);
    value = dbm_fetch(database, key);
    if (value.dptr == NULL) {
        dbm_close(database);
        return NULL;
    }
    password = xmalloc(value.dsize + 1);
    strlcpy(password, value.dptr, value.dsize + 1);
    dbm_close(database);
    return password;
}
#endif /* HAVE_DBM */


/*
**  Try to get a password out of the system /etc/shadow file.  The crypted
**  password, if found, is returned as a newly allocated string; otherwise,
**  NULL is returned.
*/
#if !HAVE_GETSPNAM
static char *
password_shadow(const char *user UNUSED)
{
    return NULL;
}
#else
static char *
password_shadow(const char *user)
{
    struct spwd *spwd;

    spwd = getspnam(user);
    if (spwd != NULL)
        return xstrdup(spwd->sp_pwdp);
    return NULL;
}
#endif /* HAVE_GETSPNAM */


/*
**  Try to get a password out of a file.  The crypted password, if found, is
**  returned as a newly allocated string; otherwise, NULL is returned.
*/
static char *
password_file(const char *username, const char *file)
{
    QIOSTATE *qp;
    char *line, *password;
    struct cvector *info = NULL;

    qp = QIOopen(file);
    if (qp == NULL)
        return NULL;
    for (line = QIOread(qp); line != NULL; line = QIOread(qp)) {
        if (*line == '#' || *line == '\n')
            continue;
        info = cvector_split(line, ':', info);
        if (info->count < 2 || strcmp(info->strings[0], username) != 0)
            continue;
        password = xstrdup(info->strings[1]);
        QIOclose(qp);
        cvector_free(info);
        return password;
    }
    if (QIOtoolong(qp))
        die("line too long in %s", file);
    if (QIOerror(qp))
        sysdie("error reading %s", file);
    QIOclose(qp);
    cvector_free(info);
    return NULL;
}


/*
**  Try to get a password out of the system password file.  The crypted
**  password, if found, is returned as a newly allocated string; otherwise,
**  NULL is returned.
*/
static char *
password_system(const char *username)
{
    struct passwd *pwd;

    pwd = getpwnam(username);
    if (pwd != NULL)
        return xstrdup(pwd->pw_passwd);
    return NULL;
}


/*
**  Main routine.
**
**  We handle the variences between systems with #if blocks above, so that
**  this code can look fairly clean.
*/
int
main(int argc, char *argv[])
{
    enum authtype { AUTH_NONE, AUTH_SHADOW, AUTH_FILE, AUTH_DBM };

    int opt;
    enum authtype type = AUTH_NONE;
    const char *filename = NULL;
    struct authinfo *authinfo = NULL;
    char *password = NULL;

    message_program_name = "ckpasswd";

    while ((opt = getopt(argc, argv, "f:u:p:" OPT_DBM OPT_SHADOW)) != -1) {
        switch (opt) {
        case 'd':
            if (type != AUTH_NONE)
                die("only one of -s, -f, or -d allowed");
            type = AUTH_DBM;
            filename = optarg;
            break;
        case 'f':
            if (type != AUTH_NONE)
                die("only one of -s, -f, or -d allowed");
            type = AUTH_FILE;
            filename = optarg;
            break;
        case 's':
            if (type != AUTH_NONE)
                die("only one of -s, -f, or -d allowed");
            type = AUTH_SHADOW;
            break;
        case 'u':
            if (authinfo == NULL) {
                authinfo = xmalloc(sizeof(struct authinfo));
                authinfo->password = NULL;
            }
            authinfo->username = optarg;
            break;
        case 'p':
            if (authinfo == NULL) {
                authinfo = xmalloc(sizeof(struct authinfo));
                authinfo->username = NULL;
            }
            authinfo->password = optarg;
            break;
        default:
            exit(1);
        }
    }
    if (argc != optind)
	die("extra arguments given");
    if (authinfo != NULL && authinfo->username == NULL)
        die("-u option is required if -p option is given");
    if (authinfo != NULL && authinfo->password == NULL)
        die("-p option is required if -u option is given");

    /* Unless a username or password was given on the command line, assume
       we're being run by nnrpd. */
    if (authinfo == NULL)
        authinfo = get_auth();
    if (authinfo == NULL)
        die("no authentication information from nnrpd");
    if (authinfo->username[0] == '\0')
        die("null username");

    /* Run the appropriate authentication routines. */
    switch (type) {
    case AUTH_SHADOW:
        password = password_shadow(authinfo->username);
        if (password == NULL)
            password = password_system(authinfo->username);
        break;
    case AUTH_FILE:
        password = password_file(authinfo->username, filename);
        break;
    case AUTH_DBM:
        password = password_dbm(authinfo->username, filename);
        break;
    case AUTH_NONE:
        if (auth_pam(authinfo->username, authinfo->password)) {
            printf("User:%s\n", authinfo->username);
            exit(0);
        }
        password = password_system(authinfo->username);
        break;
    }

    if (password == NULL)
        die("user %s unknown", authinfo->username);
    if (strcmp(password, crypt(authinfo->password, password)) != 0)
        die("invalid password for user %s", authinfo->username);

    /* The password matched. */
    printf("User:%s\n", authinfo->username);
    exit(0);
}
