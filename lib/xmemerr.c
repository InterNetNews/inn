/*  $Revision$
**
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <errno.h>


/*
**  Memory failure handler; print an error and exit.
*/
STATIC int
xmemerr(what, i, file, line)
    char		*what;
    unsigned int	i;
    const char		*file;
    int			line;
{
    /* We want large values to show up as negative, hence %d. */
    (void)fprintf(stderr, "%s:%d Can\'t %s %d bytes, %s",
                  file, line, what, i, strerror(errno));
    exit(1); 
    /* NOTREACHED */
}

int (*xmemfailure)() = xmemerr;
