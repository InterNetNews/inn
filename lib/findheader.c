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
	if (HeaderLen+1<Article+ArtLen-p
	 && p[HeaderLen] == ':'
	 && ISWHITE(p[HeaderLen + 1])
	 && caseEQn(p, Header, (SIZE_T)HeaderLen)) {
	    p += HeaderLen + 2;
	    while (1) {
		for (; p < Article + ArtLen && ISWHITE(*p); p++)
		    continue;
		if (p == Article+ArtLen)
		    return NULL;
		else {
		    if (*p != '\r' && *p != '\n')
			return p;
		    else {
			/* handle multi-lined header */
			if (++p == Article + ArtLen)
			    return NULL;
			if (ISWHITE(*p))
			    continue;
			if (p[-1] == '\r' && *p== '\n') {
			    if (++p == Article + ArtLen)
				return NULL;
			    if (ISWHITE(*p))
				continue;
			    return NULL;
			}
			return NULL;
		    }
		}
	    }
	}
	if ((p = memchr(p, '\n', ArtLen - (p - Article))) == NULL ||
	    ++p >= Article + ArtLen || *p == '\n' || (*p == '\r' &&
	    ++p >= Article + ArtLen || *p == '\n'))
	    return NULL;
    }
}

const char *HeaderFindDisk(const char *file, const char *Header, const int HeaderLen) {
    QIOSTATE            *qp;
    char                *line;
    char                *p;

    if ((qp = QIOopen(file)) == NULL)
	return NULL;

    while ((line = QIOread(qp)) != NULL) {
	if (caseEQn(Header, line, HeaderLen)) {
	    line = COPY(line);
	    /* append contiguous line */
	    while ((p = QIOread(qp)) != NULL && ISWHITE(*p)) {
		RENEW(line, char, strlen(line) + strlen(p) + 1);
		strcat(line, p);
	    }
	    QIOclose(qp);
	    return line;
	}
	if (strlen(line) == 0) {
	    /* end-of-header */
	    QIOclose(qp);
	    return NULL;
	}
    }
    QIOclose(qp);
    return NULL;
}
