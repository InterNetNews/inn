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
	if ((p = memchr(p, '\n', ArtLen - (p - Article))) == NULL
	    || ++p >= Article + ArtLen || *p == '\n'
	    || (*p == '\r' && (++p >= Article + ArtLen || *p == '\n')))
	    return NULL;
    }
}

/* 
 * Find end of current header
 * just consider '\n', not '\r\n' for the non-wireformatted case
 * if found but it's equal to EndOfData, return NULL, since it's impossible
 * to see the next data
 */
char *FindEndOfHeader(const char *Body, const char *EndOfData)
{
  char *p, *q;

  for (p = (char *)Body ; p < EndOfData ; p = ++q) {
    if ((q = memchr(p, '\n', EndOfData - p)) == NULL) {
      return NULL;
    }
    if ((q < EndOfData) && (!ISWHITE(*(q + 1)))) {
      return(q);
    }
  }
  return NULL;
}
