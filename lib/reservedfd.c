/*  $Revision$
**
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include "macros.h"
#include <fcntl.h>


STATIC FILE **Reserved_fd = NULL;
STATIC int Maxfd = -1;

BOOL fdreserve(int fdnum) {
    static int allocated = 0;
    int i, start = allocated;

    if (fdnum <= 0) {
	if (Reserved_fd != NULL) {
	    for (i = 0 ; i < Maxfd ; i++) {
		(void)fclose(Reserved_fd[i]);
	    }
	    DISPOSE(Reserved_fd);
	    Reserved_fd = NULL;
	}
	Maxfd = -1;
	allocated = 0;
	return TRUE;
    }
    if (Reserved_fd == NULL) {
	Reserved_fd = NEW(FILE *, fdnum);
	allocated = fdnum;
    } else {
	if (allocated < fdnum) {
	    RENEW(Reserved_fd, FILE *, fdnum);
	    allocated = fdnum;
	} else if (Maxfd > fdnum) {
	    for (i = fdnum ; i < Maxfd ; i++) {
		(void)fclose(Reserved_fd[i]);
	    }
	}
    }
    for (i = start ; i < fdnum ; i++) {
	if (((Reserved_fd[i] = fopen("/dev/null", "r")) == NULL)){
	    for (--i ; i >= 0 ; i--)
		(void)fclose(Reserved_fd[i]);
	    DISPOSE(Reserved_fd);
	    Reserved_fd = NULL;
	    allocated = 0;
	    Maxfd = -1;
	    return FALSE;
	}
    }
    Maxfd = fdnum;
    return TRUE;
}

FILE *Fopen(const char *p, char *type, int index) {
    FILE *nfp;
    if (p == NULL || *p == NULL)
	return NULL;
    if (index < 0 || index > Maxfd || Reserved_fd[index] == NULL)
	return fopen(p, type);
    if ((nfp = freopen(p, type, Reserved_fd[index])) == NULL) {
	Reserved_fd[index] = freopen("/dev/null", "r", Reserved_fd[index]);
	return NULL;
    }
    return (Reserved_fd[index] = nfp);
}

int Fclose(FILE *fp) {
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
