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

#if HAVE_ALLOCA_H
# include <alloca.h>
#elif defined __GNUC__
# define alloca __builtin_alloca
#elif defined _AIX
# define alloca __alloca
#elif defined _MSC_VER
# include <malloc.h>
# define alloca _alloca
#else
# include <stddef.h>
# ifdef  __cplusplus
extern "C"
# endif
void *alloca (size_t);
#endif

/* Call this in the main loop to do garbage collection. */
#ifdef C_ALLOCA
# define alloca_free()  alloca(0)
#else
# define alloca_free()  /* empty */
#endif

#endif /* !PORTABLE_ALLOCA_H */
