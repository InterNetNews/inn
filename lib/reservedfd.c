/*  $Revision$
**
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include "macros.h"
#include <fcntl.h>


struct reserved_fd {
    int	fd;
    FILE *fp;
};
STATIC struct reserved_fd *Reserved_fd = (struct reserved_fd *)NULL;
STATIC int Maxfd = -1;

BOOL fdreserve(int fdnum) {
    static int allocated = 0;
    int i, start = allocated;

    if (fdnum == 0) {
	if (Reserved_fd != (struct reserved_fd *)NULL) {
	    for (i = 0 ; i < Maxfd ; i++) {
		(void)close(Reserved_fd[i].fd);
	    }
	    DISPOSE(Reserved_fd);
	    Reserved_fd = (struct reserved_fd *)NULL;
	}
	Maxfd = -1;
	allocated = 0;
	return TRUE;
    }
    if (Reserved_fd == (struct reserved_fd *)NULL) {
	Reserved_fd = NEW(struct reserved_fd, fdnum);
	allocated = fdnum;
    } else {
	if (allocated < fdnum) {
	    RENEW(Reserved_fd, struct reserved_fd, fdnum);
	    allocated = fdnum;
	} else if (Maxfd > fdnum) {
	    for (i = fdnum ; i < Maxfd ; i++) {
		(void)close(Reserved_fd[i].fd);
	    }
	}
    }
    for (i = start ; i < fdnum ; i++) {
	if (((Reserved_fd[i].fd = open("/dev/null", O_RDONLY)) < 0) ||
	    Reserved_fd[i].fd > 255) {
	    for (--i ; i >= 0 ; i--)
		(void)close(Reserved_fd[i].fd);
	    DISPOSE(Reserved_fd);
	    Reserved_fd = (struct reserved_fd *)NULL;
	    allocated = 0;
	    return FALSE;
	}
	Reserved_fd[i].fp = NULL;
    }
    Maxfd = fdnum;
    return TRUE;
}

FILE *Fopen(const char *p, char *type, int index) {
    int	fd;
    int mode;

    if (index > Maxfd)
	return fopen(p, type);
    switch (type[0]) {
    default:
	return NULL;
    case 'r':
	mode = O_RDONLY;
	break;
    case 'w':
	mode = O_WRONLY | O_TRUNC | O_CREAT;
	break;
    case 'a':
	mode = O_WRONLY | O_APPEND | O_CREAT;
	break;
    }
    if (type[1] == '+' || (type[1] == 'b' && type[2] == '+')) {
	mode &= ~(O_RDONLY | O_WRONLY);
	mode |= O_RDWR;
    }
    if ((fd = open(p, mode)) < 0)
	return NULL;
    if (mode == (O_WRONLY | O_APPEND | O_CREAT))
	lseek(fd, 0L, SEEK_END);
    if (dup2(fd, Reserved_fd[index].fd) < 0) {
	(void)close(fd);
	return NULL;
    }
    (void)close(fd);
    return (Reserved_fd[index].fp = fdopen(Reserved_fd[index].fd, type));
}

int Fclose(FILE *fp) {
    int	fd;
    int	i;

    for (i = 0 ; i < Maxfd ; i++) {
	if (Reserved_fd[i].fp == fp)
	    break;
    }
    if (i >= Maxfd) {
	return fclose(fp);
    }
    if ((fd = open("/dev/null", O_RDONLY)) < 0)
	return EOF;
    if (dup2(fd, Reserved_fd[i].fd) < 0) {
	(void)close(fd);
	return EOF;
    }
    (void)close(fd);
    Reserved_fd[i].fp = NULL;
    return 0;
}
