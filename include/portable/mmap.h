/*  $Id$
**
**  Portability wrapper around <sys/mman.h>.
**
**  This header file includes <sys/mman.h> and then sets up various
**  additional defines and macros to permit a uniform API across platforms
**  with varying mmap implementations.
*/

#ifndef PORTABLE_MMAP_H
#define PORTABLE_MMAP_H 1

#include "config.h"
#include <sys/mman.h>

/* Make sure that the symbolic constant for the error return from mmap is
   defined (some platforms don't define it). */
#ifndef MAP_FAILED
# define MAP_FAILED     ((void *) -1)
#endif

/* Solaris 8 (at least) prototypes munmap, msync, and madvise as taking char *
   (actually a caddr_t, which is a typedef for a char *) instead of void * as
   is required by the standard.  This macro adds a cast that silences compiler
   warnings on Solaris 8 without adversely affecting other platforms.  (ISO C
   allows macro definitions of this sort; this macro is not recursive.) */
#define munmap(p, l)            munmap((void *)(p), (l))

/* On some platforms, msync only takes two arguments.  (ANSI C allows macro
   definitions of this sort; this macro is not recursive.) */
#if HAVE_MSYNC_3_ARG
# define msync(p, l, f)         msync((void *)(p), (l), (f))
#else
# define msync(p, l, f)         msync((void *)(p), (l))
#endif

/* Turn calls to madvise into a no-op if that call isn't available. */
#if HAVE_MADVISE
# define madvise(p, l, o)       madvise((void *)(p), (l), (o))
#else
# define madvise(p, l, o)       /* empty */
#endif

/* Some platforms don't flush data written to a memory mapped region until
   msync or munmap; on those platforms, we sometimes need to force an msync
   so that other parts of INN will see the changed data.  Some other
   platforms don't see writes to a file that's memory-mapped until the
   memory mappings have been flushed.

   These platforms can use mmap_flush to schedule a flush to disk and
   mmap_invalidate to force re-reading from disk.  Note that all platforms
   should still periodically call msync so that data is written out in case
   of a crash. */
#if MMAP_NEEDS_MSYNC
# define mmap_flush(p, l)       msync((p), (l), MS_ASYNC)
#else
# define mmap_flush(p, l)       /* empty */
#endif

#if MMAP_MISSES_WRITES
# define mmap_invalidate(p, l)  msync((p), (l), MS_INVALIDATE)
#else
# define mmap_invalidate(p, l)  \
	((innconf->nfsreader) ? msync((p), (l), MS_INVALIDATE) : 0)
#endif

#endif /* PORTABLE_MMAP_H */
