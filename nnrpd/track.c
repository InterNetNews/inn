/*  $Revision$
**
**  User and post tracking database.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "nnrpd.h"

#define MAX_LEN 180

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
	FILE *fd;
	char line[MAX_LEN],*p,*pp,*lp;
        char *dbfile;

        dbfile = concatpath(innconf->pathetc, "nnrpd.track");

	RARTon=FALSE;
	strcpy(user, "unknown");

	if ((fd=fopen(dbfile,"r"))!=NULL) {
		while((fgets(line,(MAX_LEN - 1),fd))!=NULL) {
			if (line[0] == '#' || line[0] == '\n') continue;
			if ((p=strchr(line,' ')) != NULL) *p='\0';
			if ((p=strchr(line,'\n')) != NULL) *p='\0';
			if ((p=strchr(line,':')) != NULL) {
				*p++='\0';
			} else {
				p=NULL;
			}
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
		syslog(L_NOTICE, "%s No logging - can't read %s", ClientHost, dbfile);
	}

        free(dbfile);
	return RARTon;
}
