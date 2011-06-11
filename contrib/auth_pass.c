/*
 *      auth_pass.c  ( $Revision$ )
 *
 * Abstract: 
 *
 *	This module is the complete source for a sample "authinfo generic" 
 *	program.  This program takes a user's login name and password
 *	(supplied either as arguments or as responses to prompts) and
 *	validates them against the contents of the password database.  
 *
 *	If the user properly authenticates themselves, a nnrp.auth style
 *	record indicating the user's authenticated login and permitting
 *	reading and posting to all groups is output on stderr (for reading by
 *	nnrpd) and the program exits with a 0 status.  If the user fails to
 *	authenticate, then a record with the attempted login name and no
 *	access is output on stderr and a non-zero exit status is returned.
 *
 * Exit statuses:
 *	0       Successfully authenticated.
 *	1	getpeername() failed, returned a bad address family, or 
 *		gethostbyaddr() failed.
 *	2	Entry not found in password file.
 *	3	No permission to read passwords, or password field is '*'.
 *	4	Bad password match.
 *
 * Environment:
 *	Run by nnrpd with stdin/stdout connected to the reader and stderr
 *	connected back to nnrpd.  This program will need to be run as suid
 *	root on systems where passwords are stored in a file readable only by
 *	root. 
 *
 * Written 1996 July 6 by Douglas Wade Needham (dneedham@oucsace.cs.ohiou.edu).
 *	
 */

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include <netdb.h>
#include <pwd.h>


main(int argc, char** argv)
/*+
 * Abstract:
 *	Main routine of the program, implementing all prompting, validation, 
 *	and status returns.
 *
 * Arguments:
 *	argc		Argument count.
 *	argv		Null terminated argument vector.
 *
 * Returns:
 *      Exits according to program status values.
 *
 * Variables:
 *	hp		Pointer to host entry.
 *	length		General integer variable
 *	password	Password given by user.
 *	peername	Hostname of the peer.
 *	pwd		Pointer to entry from passwd file.
 *	sin		Socket address structure.
 *	username	User's login name.
 */
{
    struct hostent *	hp;
    int			length;
    char		password[256];
    char		peername[1024];
    struct passwd *	pwd;
    struct sockaddr_in	sin;
    char		username[32];

    /*
     * Get the user name and password if needed.  
     */
    if (argc<2) {
        fprintf(stdout, "Username: "); fflush(stdout);
        fgets(username, sizeof(username), stdin);
    } else {
        strlcpy(username, argv[1], sizeof(username));
    }
    if (argc<3) {
        fprintf(stdout, "Password: "); fflush(stdout);
        fgets(password, sizeof(password), stdin);
    } else {
        strlcpy(password, argv[2], sizeof(password));
    }
    
    /*
     *  Strip CR's and NL's from the end.
     */
    length = strlen(username)-1;
    while (username[length] == '\r' || username[length] == '\n') {
        username[length--] = '\0';
    }
    length = strlen(password)-1;
    while (password[length] == '\r' || password[length] == '\n') {
        password[length--] = '\0';
    }

    /*
     *  Get the hostname of the peer.
     */
    length = sizeof(sin);
    if (getpeername(0, (struct sockaddr *)&sin, &length) < 0) {
        if (!isatty(0)) {
            fprintf(stderr, "cant getpeername()::%s:+:!*\n", username);
            exit(1);
        }
        strlcpy(peername, "localhost", sizeof(peername));
    } else if (sin.sin_family != AF_INET) {
        fprintf(stderr, "Bad address family %ld::%s:+:!*\n",
                (long)sin.sin_family, username);
        exit(1);
    } else if ((hp = gethostbyaddr((char *)&sin.sin_addr, sizeof(sin.sin_addr), AF_INET)) == NULL) {
        strlcpy(peername, inet_ntoa(sin.sin_addr), sizeof(peername));
    } else {
        strlcpy(peername, hp->h_name, sizeof(peername));
    }
   
    /*
     *  Get the user name in the passwd file.
     */
    if ((pwd = getpwnam(username)) == NULL) {

        /*
         *  No entry in the passwd file.
         */
        fprintf(stderr, "%s::%s:+:!*\n", peername, username);
        exit(2);
    }

    /*
     *  Make sure we managed to read in the password.
     */
    if (strcmp(pwd->pw_passwd, "*")==0) {

        /*
         *  No permission to read passwords.
         */
        fprintf(stderr, "%s::%s:+:!*\n", peername, username);
        exit(3);
    }

    /*
     *  Verify the password.
     */
    if (strcmp(pwd->pw_passwd, crypt(password, pwd->pw_passwd))!=0) {

        /*
         * Password was invalid.
         */
        fprintf(stderr, "%s::%s:+:!*\n", peername, username);
        exit(4);
    }

    /*
     *  We managed to authenticate the user.
     */
    fprintf(stderr, "%s:RP:%s:+:*\n", peername, username);
    exit(0);
}
