/*  $Id$
**
**  Portability wrapper around <sys/wait.h>.
**
**  This header includes <sys/wait.h> if it's available, and then makes sure
**  that the standard wait macros are defined and defines them if they
**  aren't.
*/

#ifndef PORTABLE_WAIT_H
#define PORTABLE_WAIT_H 1

#include "config.h"

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

/* Per the autoconf documentation, just always check to see if the various
   macros are defined and define them ourselves if they aren't.  These
   definitions are based on the approach taken by BSDI. */
#ifndef WCOREDUMP
# define WCOREDUMP(status)      ((unsigned)(status) & 0x80)
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(status)    (((unsigned)(status) >> 8) & 0xff)
#endif
#ifndef WTERMSIG
# define WTERMSIG(status)       ((unsigned)(status) & 0x7f)
#endif
#ifndef WIFEXITED
# define WIFEXITED(status)      (((unsigned)(status) & 0xff) == 0)
#endif
#ifndef WIFSTOPPED
# define WIFSTOPPED(status)     (((unsigned)(status) & 0xff) == 0x7f)
#endif
#ifndef WIFSIGNALED
# define WIFSIGNALED(status)    (!WIFSTOPPED(status) && !WIFEXITED(status))
#endif

#endif /* PORTABLE_WAIT_H */
