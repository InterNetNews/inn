/*  $Id$
**
**  The default username/password authenticator.
*/

#include "libauth.h"

#include <fcntl.h>
#include <pwd.h>

#ifdef HAVE_CRYPT_H
# include <crypt.h>
#endif

#ifdef HAVE_NDBM_H
# include <ndbm.h>
#else
# ifdef HAVE_DB1_NDBM_H
#  include <db1/ndbm.h>
# endif
#endif

#ifdef HAVE_GETSPNAM
# include <shadow.h>
#endif

#if HAVE_GETSPNAM
char *
GetShadowPass(char *user)
{
    static struct spwd *spwd;

    if ((spwd = getspnam(user)) != NULL)
	return(spwd->sp_pwdp);
    return(0);
}
#endif

char *
GetPass(char *user)
{
    static struct passwd *pwd;

    if ((pwd = getpwnam(user)) != NULL)
	return(pwd->pw_passwd);
    return(0);
}

char *
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
	if (*buf != '#') {			/* ignore comment lines */
	    buf[strlen(buf)-1] = 0;		/* clean off the \n */
	    if ((colon = strchr(buf, ':'))) {	/* correct format */
		*colon = 0;
		if (!strcmp(buf, name))
    		    found = 1;
	    }
	}
    }
    fclose(pwfile);
    if (!found)
	return(0);
    iter = colon+1;
    if (colon = strchr(iter, ':'))
	*colon = 0;
    strcpy(pass, iter);
    return(pass);
}

#if defined(HAVE_NDBM_H) || defined(HAVE_DB1_NDBM_H)
char *
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
    strncpy(pass, val.dptr, val.dsize);
    pass[val.dsize] = 0;
    dbm_close(D);
    return(pass);
}
#endif

int
main(int argc, char *argv[])
{
    extern int optind;
    extern char *optarg;
    int opt;
    int do_shadow, do_file, do_db;
    char *fname;
    char uname[SMBUF], pass[SMBUF];
    char buff[SMBUF];
    char *rpass;

    do_shadow = do_file = do_db = 0;
    fname = 0;
#if HAVE_GETSPNAM
#if defined(HAVE_NDBM_H) || defined(HAVE_DB1_NDBM_H)
    while ((opt = getopt(argc, argv, "sf:d:")) != -1) {
#else
    while ((opt = getopt(argc, argv, "sf:")) != -1) {
#endif
#else
#if defined(HAVE_NDBM_H) || defined(HAVE_DB1_NDBM_H)
    while ((opt = getopt(argc, argv, "f:d:")) != -1) {
#else
    while ((opt = getopt(argc, argv, "f:")) != -1) {
#endif
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
#if defined(HAVE_NDBM_H) || defined(HAVE_DB1_NDBM_H)
	  case 'd':
	    fname = optarg;
	    do_db = 1;
	    break;
#endif
	}
    }
    if (argc != optind)
	exit(2);

    if (get_auth(uname,pass) != 0) {
	fprintf(stderr, "ckpasswd: internal error.\n");
	exit(1);
    }

    /* got username and password, check if they're valid */
#if HAVE_GETSPNAM
    if (do_shadow) {
	if ((rpass = GetShadowPass(uname)) == (char*) 0)
	    rpass = GetPass(uname);
    } else
#endif
    if (do_file)
	rpass = GetFilePass(uname, fname);
    else
#if defined(HAVE_NDBM_H) || defined(HAVE_DB1_NDBM_H)
    if (do_db)
	rpass = GetDBPass(uname, fname);
    else
#endif
	rpass = GetPass(uname);

    if (!rpass) {
	fprintf(stderr, "ckpasswd: user %s does not exist.\n", uname);
	exit(1);
    }
    if (strcmp(rpass, crypt(pass, rpass)) == 0) {
	printf("User:%s\n", uname);
	exit(0);
    }
    fprintf(stderr, "ckpasswd: user %s password doesn't match.\n", uname);
    exit(1);
}
