/*  $Id$
**
**  Replacement implementation of alloca.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  Taken largely from the methods in the Autoconf documentation.
**
**  This is the magic required to get alloca to work properly on various
**  systems, including systems that have to use the replacement that we
**  provide as part of libinn.  It also defines alloca_free(), which calls
**  alloca(0) to do garbage collection only if it's necessary.
*/

#ifndef PORTABLE_ALLOCA_H
#define PORTABLE_ALLOCA_H 1

#include "config.h"

/* AIX requires this to come before anything except comments and preprocessor
   directives. */
#ifndef __GNUC__
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifdef _AIX
 #pragma alloca
#  else
#   ifndef alloca /* predefined by HP cc +Olibcalls */
void *alloca (unsigned int);
#   endif
#  endif
# endif
#endif

/* Call this in the main loop to do garbage collection. */
#ifdef C_ALLOCA
# define alloca_free()  alloca(0)
#else
# define alloca_free()  /* empty */
#endif

#endif /* !PORTABLE_ALLOCA_H */
