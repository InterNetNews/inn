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
#include "libauth.h"
#include "smbval/valid.h"

int
main(int argc, char *argv[])
{
   char uname[SMBUF], pass[SMBUF];
   int result;

   if ( (argc > 4) || (argc < 3) ){
       fprintf(stderr,"auth_smb: wrong number of arguments (auth_smb <server> [<backupserver>] <domain>)\n");
       exit(1);
   }
   result = get_auth(uname, pass);
   if (result != 0)
       exit(result);

   /* got username and password, check if they're valid */
   if (argc > 3)
       result = Valid_User(uname, pass , argv[1], argv[2], argv[3]);
   else
       result = Valid_User(uname, pass , argv[1], argv[1], argv[2]);
   switch (result) {
    case 0: printf("User:%s\n",uname);
	    exit(0);
    case 1: fprintf(stderr, "auth_smb: server error\n");
	    exit(1);
    case 2: fprintf(stderr, "auth_smb: protocol error\n");
            exit(1);
    case 3: fprintf(stderr, "auth_smb: logon error\n");
	    exit(1);
   }
   fprintf(stderr, "auth_smb: unknown error\n");
   exit(1);   
}

