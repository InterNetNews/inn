/*  $Id$
**
**  Portability wrapper around <time.h> and <sys/time.h>.
**
**  This header includes <time.h> and <sys/time.h> as applicable, handling
**  systems where one can't include both headers (per the autoconf manual).
*/

#ifndef PORTABLE_TIME_H
#define PORTABLE_TIME_H 1

#include "config.h"

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#endif /* PORTABLE_TIME_H */
