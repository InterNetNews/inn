#include "config.h"
#include <errno.h>

#if defined (DO_NEED_STRERROR)

extern int sys_nerr;
extern char *sys_errlist[];

char *strerror (int errnum)
{
    static char buff[30];

    if (errnum >= 0 && errnum < sys_nerr)
      return sys_errlist[errnum];

    (void)sprintf(buff, "Error code %d\n", errnum);
    return buff;
}

#endif /* defined (NEED_STRERROR) */
