/*  $Revision$
**
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <sys/stat.h>


/*
**  Call mknod(2) to make a fifo.
*/
int
mkfifo(path, mode)
    char	*path;
    int		mode;
{
    return mknod(path, S_IFIFO | mode, 0);
}
