/*  $Id$
**
*/

#include "config.h"
#include "clibrary.h"
#include "inn/libinn.h"


#define LPAREN	'('
#define RPAREN	')'


/*
**  Clean up a from line, making the following transformations:
**	address			address
**	address (stuff)		address
**	stuff <address>		address
*/
void HeaderCleanFrom(char *from)
{
    char	        *p;
    char	        *end;
    int			len;

    if ((len = strlen(from)) == 0)
	return;
    /* concatenate folded header */
    for (p = end = from ; p < from + len ;) {
	if (*p == '\n') {
	    if ((p + 1 < from + len) && ISWHITE(p[1])) {
		if ((p - 1 >= from) && (p[-1] == '\r')) {
		    end--;
		    *end = p[1];
		    p += 2;
		} else {
		    *end = p[1];
		    p++;
		}
	    } else {
		*end = '\0';
		break;
	    }
	} else
	    *end++ = *p++;
    }
    if (end != from)
	*end = '\0';

    /* Do pretty much the equivalent of sed's "s/(.*)//g"; */
    while ((p = strchr(from, LPAREN)) && (end = strchr(p, RPAREN))) {
	while (*++end)
	    *p++ = *end;
	*p = '\0';
    }

    /* Do pretty much the equivalent of sed's "s/\".*\"//g"; */
    while ((p = strchr(from, '"')) && (end = strchr(p, '"'))) {
	while (*++end)
	    *p++ = *end;
	*p = '\0';
    }

    /* Do the equivalent of sed's "s/.*<\(.*\)>/\1/" */
    if ((p = strrchr(from, '<')) && (end = strrchr(p, '>'))) {
	while (++p < end)
	    *from++ = *p;
	*from = '\0';
    }

    /* drop white spaces */
    if ((len = strlen(from)) == 0)
	return;
    for (p = end = from ; p < from + len ;) {
	if (ISWHITE(*p)) {
	    p++;
	    continue;
	}
	*end++ = *p++;
    }
    if (end != from)
	*end = '\0';
}
