/*  $Revision$
**
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <errno.h>
#include "libinn.h"
#include "macros.h"


/*
**  Allocate some memory or call the memory failure handler.
*/
ALIGNPTR xmalloc(unsigned int i)
{
    POINTER		new;

    while ((new = malloc(i)) == NULL)
	(*xmemfailure)("malloc", i);
    return CAST(ALIGNPTR, new);
}
