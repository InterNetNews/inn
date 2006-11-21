/*  $Id$
**
*/

#include "config.h"
#include "clibrary.h"
#include <fcntl.h>

#include "inn/libinn.h"


static FILE **Reserved_fd = NULL;
static int Maxfd = -1;

bool
fdreserve(int fdnum)
{
    static int allocated = 0;
    int i, start = allocated;

    if (fdnum <= 0) {
	if (Reserved_fd != NULL) {
	    for (i = 0 ; i < Maxfd ; i++) {
		fclose(Reserved_fd[i]);
	    }
	    free(Reserved_fd);
	    Reserved_fd = NULL;
	}
	Maxfd = -1;
	allocated = 0;
	return true;
    }
    if (Reserved_fd == NULL) {
	Reserved_fd = xmalloc(fdnum * sizeof(FILE *));
	allocated = fdnum;
    } else {
	if (allocated < fdnum) {
            Reserved_fd = xrealloc(Reserved_fd, fdnum * sizeof(FILE *));
	    allocated = fdnum;
	} else if (Maxfd > fdnum) {
	    for (i = fdnum ; i < Maxfd ; i++) {
		fclose(Reserved_fd[i]);
	    }
	}
    }
    for (i = start ; i < fdnum ; i++) {
	if (((Reserved_fd[i] = fopen("/dev/null", "r")) == NULL)){
	    for (--i ; i >= 0 ; i--)
		fclose(Reserved_fd[i]);
	    free(Reserved_fd);
	    Reserved_fd = NULL;
	    allocated = 0;
	    Maxfd = -1;
	    return false;
	}
    }
    Maxfd = fdnum;
    return true;
}

FILE *
Fopen(const char *p, const char *type, int xindex)
{
    FILE *nfp;
    if (p == NULL || *p == '\0')
	return NULL;
    if (xindex < 0 || xindex > Maxfd || Reserved_fd[xindex] == NULL)
	return fopen(p, type);
    if ((nfp = freopen(p, type, Reserved_fd[xindex])) == NULL) {
	Reserved_fd[xindex] = freopen("/dev/null", "r", Reserved_fd[xindex]);
	return NULL;
    }
    return (Reserved_fd[xindex] = nfp);
}

int
Fclose(FILE *fp)
{
    int	i;

    if (fp == NULL)
	return 0;
    for (i = 0 ; i < Maxfd ; i++) {
	if (Reserved_fd[i] == fp)
	    break;
    }
    if (i >= Maxfd)
	return fclose(fp);
    Reserved_fd[i] = freopen("/dev/null", "r", Reserved_fd[i]);
    return 0;
}
