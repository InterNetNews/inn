/*  $Id$
**
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "libinn.h"
#include "macros.h"


/*
**  Read a big amount, looping until it is all done.  Return TRUE if
**  successful.
*/
int xread(int fd, char *p, off_t i)
{
    int	                count;

    for ( ; i; p += count, i -= count)
	if ((count = read(fd, p, i)) <= 0)
	    return -1;
    return 0;
}


/*
**  Read an already-open file into memory.
*/
char *ReadInDescriptor(int fd, struct stat *Sbp)
{
    struct stat	mystat;
    char	*p;
    int		oerrno;

    if (Sbp == NULL)
	Sbp = &mystat;

    /* Get the size, and enough memory. */
    if (fstat(fd, Sbp) < 0) {
	oerrno = errno;
	close(fd);
	errno = oerrno;
	return NULL;
    }
    p = xmalloc(Sbp->st_size + 1);

    /* Slurp, slurp. */
    if (xread(fd, p, Sbp->st_size) < 0) {
	oerrno = errno;
	free(p);
	close(fd);
	errno = oerrno;
	return NULL;
    }

    /* Terminate the string; terminate the routine. */
    p[Sbp->st_size] = '\0';
    return p;
}


/*
**  Read a file into allocated memory.  Optionally fill in the stat(2) data.
**  Return a pointer to the file contents, or NULL on error.
*/
char *ReadInFile(const char *name, struct stat *Sbp)
{
    char	*p;
    int		fd;

    if ((fd = open(name, O_RDONLY)) < 0)
	return NULL;

    p = ReadInDescriptor(fd, Sbp);
    close(fd);
    return p;
}
