/*  $Revision$
**
**  tracking database
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include "nnrpd.h"
#include "mydir.h"
#include <sys/time.h>
#include <string.h>

#define MAX_LEN 80

/* TrackClient determines whether or not
   we are interested in tracking the activities
   of the currently connected host. We have to
   rely on an external process to set up the
   entries in the database though which makes
   this only as reliable as the process that
   sets this up...
*/

/* Format of the input line is <host>:<username>
*/

int TrackClient(char *client, char *user)
{
	int RARTon;
	char dbfile[180];
	FILE *fd;
	char line[MAX_LEN],*p,*pp,*lp;

	strcpy(dbfile, _PATH_NEWSLIB);
	strcat(dbfile, "/hosts.track");

	RARTon=FALSE;
	strcpy(user,"");

	if ((fd=fopen(dbfile,"r"))!=NULL) {
		while((fgets(line,(MAX_LEN - 1),fd))!=NULL) {
			if ((p=strchr(line,' ')) != NULL) *p='\0';
			if ((p=strchr(line,'\n')) != NULL) *p='\0';
			if ((p=strchr(line,':')) != NULL) 
				*p++='\0';
			else 
				p=NULL;
			
			pp=line;
			if ((lp=strchr(pp,'*')) != NULL) {
				pp=++lp;
			}
			if (strstr(client,pp)!=NULL) {
				RARTon=TRUE;
				if (p != NULL) 
					strcpy(user,p);
				break;
			}
		}
		fclose(fd);
	} else {
		RARTon=FALSE;
		syslog(L_NOTICE, "No logging for %s - can't read %s", ClientHost, dbfile);
	}

	return RARTon;
}



