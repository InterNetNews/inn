/*  $Revision$
**
**  DBZ compatibility routines, for use if DBZ isn't patched
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"


/* LINTLIBRARY */
int
dbzwritethrough(value)
    int		value;
{
    return value;
}


long
dbztagmask(size)
    long	size;
{
    return size;
}
