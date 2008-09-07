/*  $Id$
**
*/

#include "config.h"
#include "clibrary.h"
#include "inn/libinn.h"
#include "nntp.h"


/*
**  Send a string of one or more lines down a stdio FILE using RFC 3977
**  conventions.  Return -1 on error.
*/
int NNTPsendarticle(char *p, FILE *F, bool Terminate)
{
    char *next;

    for (; p && *p; next[-1] = '\n', p = next) {
	/* Get pointer to next line.  Truncate long lines. */
	if ((next = strchr(p, '\n')) != NULL)
	    *next++ = '\0';

	/* Write line. */
	if (*p == '.' && putc('.', F) == EOF)
	    return -1;
	if (fprintf(F, "%s\r\n", p) == EOF)
	    return -1;

	/* Done? */
	if (next == NULL)
	    break;
    }

    if (Terminate && fprintf(F, ".\r\n") == EOF)
	return -1;

    return fflush(F) == EOF || ferror(F) ? -1 : 0;
}
