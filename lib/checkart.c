/*  $Revision$
**
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include "nntp.h"


/*
**  See if an article is longer than the NNTP line-length limits.
*/
int NNTPcheckarticle(char *p)
{
    char	        *next;

    for (; p && *p; p = next) {
	if ((next = strchr(p, '\n')) == NULL)
	    break;
	if (next - p > NNTP_STRLEN)
	    return -1;
    }
    return 0;
}
