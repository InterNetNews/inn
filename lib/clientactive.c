/*  $Id$
**
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>

#include "inn/innconf.h"
#include "libinn.h"
#include "macros.h"
#include "nntp.h"
#include "paths.h"


static char	*CApathname;
static FILE	*CAfp;


/*
**  Get a copy of the active file for a client host to use, locally or
**  remotely.
*/
FILE *
CAopen(FILE *FromServer, FILE *ToServer)
{
    char *path;

    /* Use a local (or NFS-mounted) copy if available.  Make sure we don't
     * try to delete it when we close it. */
    path = concatpath(innconf->pathdb, _PATH_CLIENTACTIVE);
    CAfp = fopen(path, "r");
    free(path);
    if (CAfp != NULL) {
	CApathname = NULL;
	return CAfp;
    }

    /* Use the active file from the server */
    return CAlistopen(FromServer, ToServer, (char *)NULL);
}


/*
**  Internal library routine.
*/
FILE *
CA_listopen(char *pathname, FILE *FromServer, FILE *ToServer,
            const char *request)
{
    char	buff[BUFSIZ];
    char	*p;
    int		oerrno;
    FILE	*F;

    F = fopen(pathname, "w");
    if (F == NULL)
	return NULL;

    /* Send a LIST command to and capture the output. */
    if (request == NULL)
	(void)fprintf(ToServer, "list\r\n");
    else
	(void)fprintf(ToServer, "list %s\r\n", request);
    (void)fflush(ToServer);

    /* Get the server's reply to our command. */
    if (fgets(buff, sizeof buff, FromServer) == NULL
     || !EQn(buff, NNTP_LIST_FOLLOWS, STRLEN(NNTP_LIST_FOLLOWS))) {
	oerrno = errno;
	/* Only call CAclose() if opened through CAopen() */
	if (strcmp(CApathname, pathname) == 0)
            CAclose();
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
    CAclose();
    errno = oerrno;
    return NULL;
}


/*
**  Use the NNTP list command to get a file from a server.  Default is
**  the active file, otherwise ask for whatever is in the request param.
*/
FILE *
CAlistopen(FILE *FromServer, FILE *ToServer, const char *request)
{
    int fd, oerrno;

    /* Gotta talk to the server -- see if we can. */
    if (FromServer == NULL || ToServer == NULL) {
	errno = EBADF;
	return NULL;
    }

    CApathname = concatpath(innconf->pathtmp, _PATH_TEMPACTIVE);
    fd = mkstemp(CApathname);
    if (fd < 0) {
        oerrno = errno;
        free(CApathname);
        CApathname = 0;
        errno = oerrno;
        return NULL;
    }
    close(fd);
    return CAfp = CA_listopen(CApathname, FromServer, ToServer, request);
}



/*
**  Close the file opened by CAopen or CAlistopen.
*/
void
CAclose(void)
{
    if (CAfp) {
	(void)fclose(CAfp);
	CAfp = NULL;
    }
    if (CApathname != NULL) {
	unlink(CApathname);
	CApathname = NULL;
    }
}
