/*  $Id$
**
**  The default username/password authenticator.
*/

#include "config.h"
#include "clibrary.h"

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
#endif

#if HAVE_GETSPNAM
# include <shadow.h>
#endif

#if HAVE_PAM
# include <security/pam_appl.h>
#endif

#if HAVE_PAM
/*
 * PAM conversation function.  Since we can't very well ask the user for
 * a password interactively, this function returns the password to every
 * question PAM asks.  There appears to be no generic way to determine
 * whether the message in question is indeed asking for the password which
 * is less than ideal...
 *
 * NOTE: This function allocates an array of 'struct pam_response' which
 * needs to be free()'d later on.  For this program though, it's not exactly
 * an issue because it will get called once and the program will exit...
 *
 * appdata_ptr contains the password we were given.
 */
static int pass_conv (int num_msg, const struct pam_message **msgm UNUSED,
		struct pam_response **response, void *appdata_ptr)
{
    int i;

    *response = (struct pam_response *)malloc (num_msg *
		    sizeof(struct pam_response));
    if (*response == NULL)
        return PAM_CONV_ERR;

    for (i = 0; i < num_msg; i++) {
	/* Construct the response */
	(*response)[i].resp = strdup((char *)appdata_ptr);
	(*response)[i].resp_retcode = 0;
    }
    return PAM_SUCCESS;
}

static struct pam_conv conv = {
    pass_conv,
    NULL
};
#endif /* HAVE_PAM */

#if HAVE_GETSPNAM
static char *
GetShadowPass(char *user)
{
    static struct spwd *spwd;

    if ((spwd = getspnam(user)) != NULL)
	return(spwd->sp_pwdp);
    return(0);
}
#endif /* HAVE_GETSPNAM */

static char *
GetPass(char *user)
{
    static struct passwd *pwd;

    if ((pwd = getpwnam(user)) != NULL)
	return(pwd->pw_passwd);
    return(0);
}

static char *
GetFilePass(char *name, char *file)
{
    FILE *pwfile;
    char buf[SMBUF];
    char *colon, *iter;
    int found;
    static char pass[SMBUF];

    pwfile = fopen(file, "r");
    if (!pwfile)
	return(0);
    found = 0;
    while ((!found) && fgets(buf, sizeof(buf), pwfile)) {
	if (*buf == '#')			/* ignore comment lines */
	    continue;
	buf[strlen(buf)-1] = 0;			/* clean off the \n */
	if ((colon = strchr(buf, ':'))) {	/* correct format */
	    *colon = 0;
	    if (strcmp(buf, name))
		continue;
	    iter = colon+1;			/* user matches */
	    if ((colon = strchr(iter, ':')) != NULL)
		*colon = 0;
	    strncpy(pass, iter, sizeof(pass));
            pass[sizeof(pass) - 1] = '\0';
	    fclose(pwfile);
	    return(pass);
	}
    }
    fclose(pwfile);
    return(0);
}

#if HAVE_DBM
static char *
GetDBPass(char *name, char *file)
{
    datum key;
    datum val;
    DBM *D;
    static char pass[SMBUF];

    D = dbm_open(file, O_RDONLY, 0600);
    if (!D)
        return(0);
    key.dptr = name;
    key.dsize = strlen(name);
    val = dbm_fetch(D, key);
    if (!val.dptr) {
        dbm_close(D);
        return(0);
    }
    if ((size_t) val.dsize > sizeof(pass) - 1)
        return NULL;
    strncpy(pass, val.dptr, val.dsize);
    pass[val.dsize] = 0;
    dbm_close(D);
    return(pass);
}
#endif /* HAVE_DBM */

int
main(int argc, char *argv[])
{
    int opt;
    int do_shadow, do_file, do_db;
    char *fname;
    struct authinfo *authinfo;
    char *rpass;

    do_shadow = do_file = do_db = 0;
    fname = 0;
#if HAVE_GETSPNAM
# if HAVE_DBM
    while ((opt = getopt(argc, argv, "sf:d:")) != -1) {
# else
    while ((opt = getopt(argc, argv, "sf:")) != -1) {
# endif
#else
# if HAVE_DBM
    while ((opt = getopt(argc, argv, "f:d:")) != -1) {
# else
    while ((opt = getopt(argc, argv, "f:")) != -1) {
# endif
#endif
	/* only allow one of the three possibilities */
	if (do_shadow || do_file || do_db)
	    exit(1);
	switch (opt) {
	  case 's':
	    do_shadow = 1;
	    break;
	  case 'f':
	    fname = optarg;
	    do_file = 1;
	    break;
#if HAVE_DBM
	  case 'd':
	    fname = optarg;
	    do_db = 1;
	    break;
#endif
	}
    }
    if (argc != optind)
	exit(2);

    authinfo = get_auth();
    if (authinfo == NULL) {
	fprintf(stderr, "ckpasswd: internal error.\n");
	exit(1);
    }
    if (authinfo->username[0] == '\0') {
	fprintf(stderr, "ckpasswd: null username.\n");
	exit(1);
    }

    /* got username and password, check if they're valid */
#if HAVE_GETSPNAM
    if (do_shadow) {
	if ((rpass = GetShadowPass(authinfo->username)) == (char*) 0)
	    rpass = GetPass(authinfo->username);
    } else
#endif
    if (do_file)
	rpass = GetFilePass(authinfo->username, fname);
    else
#if HAVE_DBM
    if (do_db)
	rpass = GetDBPass(authinfo->username, fname);
    else
#endif
#if HAVE_PAM
    {
        pam_handle_t *pamh;
	int res;
	
	conv.appdata_ptr = authinfo->password;
        res = pam_start ("nnrpd", authinfo->username, &conv, &pamh);
	if (res != PAM_SUCCESS) {
            fprintf (stderr, "Failed: pam_start(): %s\n",
			    pam_strerror(pamh, res));
            exit (1);
        }

        if ((res = pam_authenticate (pamh, 0)) != PAM_SUCCESS) {
            fprintf (stderr, "Failed: pam_authenticate(): %s\n",
			    pam_strerror(pamh, res));
            exit (1);
        }

        if ((res = pam_acct_mgmt (pamh, 0)) != PAM_SUCCESS) {
	    fprintf (stderr, "Failed: pam_acct_mgmt(): %s\n",
			    pam_strerror (pamh, res));
	    exit (1);
        }

        if ((res = pam_end (pamh, res) != PAM_SUCCESS)) {
            fprintf (stderr, "Failed: pam_end(): %s\n",
			    pam_strerror (pamh, res));
	    exit (1);
        }

	/* If it gets this far, the user has been successfully authenticated. */
        fprintf (stdout, "User:%s\n", authinfo->username);
        exit (0);
    }
#else /* HAVE_PAM */
	rpass = GetPass(authinfo->username);
#endif /* HAVE_PAM */

    if (!rpass) {
	fprintf(stderr, "ckpasswd: user %s does not exist.\n",
                authinfo->username);
	exit(1);
    }
    if (strcmp(rpass, crypt(authinfo->password, rpass)) == 0) {
	printf("User:%s\n", authinfo->username);
	exit(0);
    }
    fprintf(stderr, "ckpasswd: user %s password doesn't match.\n",
            authinfo->username);
    exit(1);
}
