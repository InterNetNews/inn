/*  $Revision$
**
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <errno.h>
#include "libinn.h"
#include "macros.h"


/*
**  Allocate some memory or call the memory failure handler.
*/
ALIGNPTR
xmalloc(i)
    unsigned int	i;
{
    POINTER		new;

    while ((new = malloc(i)) == NULL)
	(*xmemfailure)("malloc", i);
#if 0
    memset (new,0,i) ;
#endif
    return CAST(ALIGNPTR, new);
}
