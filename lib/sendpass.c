/*  $Id$
**
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <errno.h>

#include "inn/innconf.h"
#include "libinn.h"
#include "macros.h"
#include "nntp.h"
#include "paths.h"


/*
**  Send authentication information to an NNTP server.
*/
int NNTPsendpassword(char *server, FILE *FromServer, FILE *ToServer)
{
    FILE	        *F;
    char	        *p;
    char                *path;
    char		buff[SMBUF];
    char		input[SMBUF];
    char		*user;
    char		*pass;
    char		*style;
    int			oerrno;

    /* What server are we interested in?  Default to the campus one. */
    if (server == NULL
     && (server = innconf->server) == NULL)
	return -1;

    /* Open the password file; coarse check on errno, but good enough. */
    path = concatpath(innconf->pathetc, _PATH_NNTPPASS);
    F = fopen(path, "r");
    free(path);
    if (F == NULL)
	return errno == EPERM ? -1 : 0;

    /* Scan the file, skipping blank and comment lines. */
    while (fgets(buff, sizeof buff, F) != NULL) {
	if ((p = strchr(buff, '\n')) != NULL)
	    *p = '\0';
	if (buff[0] == '\0' || buff[0] == '#')
	    continue;

	/* Parse the line. */
	if ((user = strchr(buff, ':')) == NULL)
	    continue;
	*user++ = '\0';
	if ((pass = strchr(user, ':')) == NULL)
	    continue;
	*pass++ = '\0';
	if ((style = strchr(pass, ':')) != NULL) {
	    *style++ = '\0';
	    if (!EQ(style, "authinfo")) {
		errno = EDOM;
		break;
	    }
	}

	if (!caseEQ(server, buff))
	    continue;

	if (*user) {
	    /* Send the first part of the command, get a reply. */
	    fprintf(ToServer, "authinfo user %s\r\n", user);
	    if (fflush(ToServer) == EOF || ferror(ToServer))
		break;
	    if (fgets(input, sizeof input, FromServer) == NULL
	     || atoi(input) != NNTP_AUTH_NEXT_VAL)
		break;
	}

	if (*pass) {
	    /* Send the second part of the command, get a reply. */
	    fprintf(ToServer, "authinfo pass %s\r\n", pass);
	    if (fflush(ToServer) == EOF || ferror(ToServer))
		break;
	    if (fgets(input, sizeof input, FromServer) == NULL
	     || atoi(input) != NNTP_AUTH_OK_VAL)
		break;
	}

	/* Authenticated. */
	fclose(F);
	return 0;
    }

    /* End of file without finding a password, that's okay. */
    if (feof(F)) {
	fclose(F);
	return 0;
    }

    /* Save errno, close the file, fail. */
    oerrno = errno;
    fclose(F);
    errno = oerrno;
    return -1;
}
