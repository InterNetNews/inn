/*  $Id$
**
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>

#include "inn/innconf.h"
#include "libinn.h"
#include "nntp.h"
#include "paths.h"


static char	*GMApathname = NULL;
static FILE	*GMAfp = NULL;


/*
**  Close the file opened by GMAlistopen.
*/
static void
GMAclose(void)
{
    if (GMAfp) {
	fclose(GMAfp);
	GMAfp = NULL;
    }
    if (GMApathname != NULL) {
	unlink(GMApathname);
        free(GMApathname);
	GMApathname = NULL;
    }
}

/*
**  Internal library routine.
*/
static FILE *
GMA_listopen(int fd, FILE *FromServer, FILE *ToServer, const char *request)
{
    char	buff[BUFSIZ];
    char	*p;
    int		oerrno;
    FILE	*F;

    F = fdopen(fd, "r+");
    if (F == NULL)
        return NULL;

    /* Send a LIST command to and capture the output. */
    if (request == NULL)
	fprintf(ToServer, "list moderators\r\n");
    else
	fprintf(ToServer, "list %s\r\n", request);
    fflush(ToServer);

    /* Get the server's reply to our command. */
    if (fgets(buff, sizeof buff, FromServer) == NULL
     || strncmp(buff, NNTP_LIST_FOLLOWS, strlen(NNTP_LIST_FOLLOWS)) != 0) {
	oerrno = errno;
        fclose(F);
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
	    if (ferror(F) || fflush(F) == EOF || fseeko(F, 0, SEEK_SET) != 0)
		break;
	    return F;
	}
	fprintf(F, "%s\n", buff);
    }

    /* Ran out of input before finding the terminator; quit. */
    oerrno = errno;
    fclose(F);
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
    int                 fd;

    strcpy(name, group);
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
        GMApathname = concatpath(innconf->pathtmp, _PATH_TEMPMODERATORS);
        fd = mkstemp(GMApathname);
        if (fd >= 0)
            GMAfp = GMA_listopen(fd, FromServer, ToServer, "moderators");
        else
            GMAfp = NULL;

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
	    if (buff[0] == '\0' || buff[0] == '#')
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
		snprintf(address, sizeof(address), save, name);
		break;
	    }
	}

	 GMAclose();
	if (address[0])
	    return address;
    }

    /* If we don't have an address, see if the config file has a default. */
    if ((save = moderatormailer) == NULL)
	return NULL;

    for (p = name; *p; p++)
	if (*p == '.')
	    *p = '-';
    snprintf(address, sizeof(address), save, name);
    return address;
}
