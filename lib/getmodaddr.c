/*  $Id$
**
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>

#include "libinn.h"
#include "macros.h"
#include "nntp.h"
#include "paths.h"


static char	GMApathname[256];
static FILE	*GMAfp = NULL;


/*
**  Close the file opened by GMAlistopen.
*/
static void
GMAclose(void)
{
    if (GMAfp) {
	(void)fclose(GMAfp);
	GMAfp = NULL;
    }
    if (GMApathname[0]) {
	(void)unlink(GMApathname);
	GMApathname[0] = '\0';
    }
}

/*
**  Internal library routine.
*/
static FILE *
GMA_listopen(const char *pathname, FILE *FromServer, FILE *ToServer,
	     const char *request)
{
    char	buff[BUFSIZ];
    char	*p;
    int		oerrno;
    FILE	*F;

    (void)unlink(pathname);
    if ((F = fopen(pathname, "w")) == NULL)
	return NULL;

    /* Send a LIST command to and capture the output. */
    if (request == NULL)
	(void)fprintf(ToServer, "list moderators\r\n");
    else
	(void)fprintf(ToServer, "list %s\r\n", request);
    (void)fflush(ToServer);

    /* Get the server's reply to our command. */
    if (fgets(buff, sizeof buff, FromServer) == NULL
     || !EQn(buff, NNTP_LIST_FOLLOWS, STRLEN(NNTP_LIST_FOLLOWS))) {
	oerrno = errno;
	GMAclose();
	errno = oerrno;
	return NULL;
    }

    /* Slurp up the rest of the response. */
    while (fgets(buff, sizeof buff, FromServer) != NULL) {
	if ((p = strchr(buff, '\r')) != NULL)
	    *p = '\0';
	if ((p = strchr(buff, '\n')) != NULL)
	    *p = '\0';
	if (buff[0] == '.' && buff[1] == '\0') {
	    if (ferror(F) || fflush(F) == EOF || fclose(F) == EOF)
		break;
	    return fopen(pathname, "r");
	}
	(void)fprintf(F, "%s\n", buff);
    }

    /* Ran out of input before finding the terminator; quit. */
    oerrno = errno;
    (void)fclose(F);
    GMAclose();
    errno = oerrno;
    return NULL;
}

/*
**  Read the moderators file, looking for a moderator.
*/
char *
GetModeratorAddress(FILE *FromServer, FILE *ToServer, char *group,
		    char *moderatormailer)
{
    static char		address[SMBUF];
    char	        *p;
    char		*save;
    char                *path;
    char		buff[BUFSIZ];
    char		name[SMBUF];

    (void)strcpy(name, group);
    address[0] = '\0';

    if (FromServer==NULL || ToServer==NULL){

        /*
         *  This should be part of nnrpd or the like running on the server.
         *  Open the server copy of the moderators file.
         */
        path = concatpath(innconf->pathetc, _PATH_MODERATORS);
	GMAfp = fopen(path, "r");
        free(path);
    }else{
        /*
         *  Get a local copy of the moderators file from the server.
         */
	(void)sprintf(GMApathname, "%.220s/%s", innconf->pathtmp,
		_PATH_TEMPMODERATORS);
        (void)mktemp(GMApathname);
        GMAfp = GMA_listopen(GMApathname, FromServer, ToServer, "moderators");
	/* Fallback to the local copy if the server doesn't have it */
	if (GMAfp == NULL) {
            path = concatpath(innconf->pathetc, _PATH_MODERATORS);
	    GMAfp = fopen(path, "r");
            free(path);
        }
    }

    if (GMAfp != NULL) {
	while (fgets(buff, sizeof buff, GMAfp) != NULL) {
	    /* Skip blank and comment lines. */
	    if ((p = strchr(buff, '\n')) != NULL)
		*p = '\0';
	    if (buff[0] == '\0' || buff[0] == COMMENT_CHAR)
		continue;

	    /* Snip off the first word. */
	    if ((p = strchr(buff, ':')) == NULL)
		/* Malformed line... */
		continue;
	    *p++ = '\0';

	    /* If it pattern-matches the newsgroup, the second field is a
	     * format for mailing, with periods changed to dashes. */
	    if (uwildmat(name, buff)) {
		for (save = p; ISWHITE(*save); save++)
		    continue;
		for (p = name; *p; p++)
		    if (*p == '.')
			*p = '-';
		(void)sprintf(address, save, name);
		break;
	    }
	}

	(void) GMAclose();
	if (address[0])
	    return address;
    }

    /* If we don't have an address, see if the config file has a default. */
    if ((save = moderatormailer) == NULL)
	return NULL;

    for (p = name; *p; p++)
	if (*p == '.')
	    *p = '-';
    (void)sprintf(address, save, name);
    return address;
}
