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
ALIGNPTR xmalloc(unsigned int i, const char *file, int line)
{
    POINTER		new;

    while ((new = malloc(i)) == NULL)
	(*xmemfailure)("malloc", i, file, line);
#if 0
    memset (new,0,i) ;
#endif
    return CAST(ALIGNPTR, new);
}
