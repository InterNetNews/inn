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
**  Reallocate some memory or call the memory failure handler.
*/
ALIGNPTR xrealloc(char *p, unsigned int i, const char *file, int line)
{
    POINTER		new;

    while ((new = realloc((POINTER)p, i)) == NULL)
	(*xmemfailure)("remalloc", i, file, line);
    return CAST(ALIGNPTR, new);
}
