/*  $Revision$
**
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <errno.h>

/*
**  Keep writing until everything has been written or we get an error.
*/
int
xwrite(fd, p, i)
    register int	fd;
    register char	*p;
    register int	i;
{
    register int	c;

    for ( ; i; p += c, i -= c)
	if ((c = write(fd, (POINTER)p, (SIZE_T)i)) <= 0)
	    return -1;
    return 0;
}
