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
#include "qio.h"


/*
**  Find a header in an article.
*/
const char *HeaderFindMem(const char *Article, const int ArtLen, const char *Header, const int HeaderLen)
{
    const char	        *p;

    for (p = Article; ; ) {
	/* Match first character, then colon, then whitespace (don't
	 * delete that line -- meet the RFC!) then compare the rest
	 * of the word. */
	if (p[HeaderLen] == ':'
	 && ISWHITE(p[HeaderLen + 1])
	 && caseEQn(p, Header, (SIZE_T)HeaderLen)) {
	    for (p += HeaderLen; *++p != '\n' && ISWHITE(*p); )
		continue;
	    return p;
	}
	if ((p = memchr(p, '\n', ArtLen - (p - Article))) == NULL || *++p == '\n')
	    return NULL;
    }
}

const char *HeaderFindDisk(const char *file, const char *Header, const int HeaderLen) {
    QIOSTATE            *qp;
    char                *line;

    if ((qp = QIOopen(file)) == NULL)
	return NULL;

    while (line = QIOread(qp)) {
	if (caseEQn(Header, line, HeaderLen)) {
	    QIOclose(qp);
	    return COPY(line);
	}
    }
    QIOclose(qp);
    return NULL;
}
