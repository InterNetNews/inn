/*  $Revision$
**
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <ctype.h>
#include "libinn.h"
#include "macros.h"


/*
**  Find a header in an article.
*/
const char *HeaderFind(const char *Article, const char *Header, const int size)
{
    const char	        *p;

    for (p = Article; ; ) {
	/* Match first character, then colon, then whitespace (don't
	 * delete that line -- meet the RFC!) then compare the rest
	 * of the word. */
	if (p[size] == ':'
	 && ISWHITE(p[size + 1])
	 && caseEQn(p, Header, (SIZE_T)size)) {
	    for (p += size; *++p != '\n' && ISWHITE(*p); )
		continue;
	    return p;
	}
	if ((p = strchr(p, '\n')) == NULL || *++p == '\n')
	    return NULL;
    }
}
