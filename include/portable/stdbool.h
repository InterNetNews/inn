/* $Id$
 *
 * Portability wrapper around <stdbool.h>.
 *
 * Provides the bool and _Bool types and the true and false constants,
 * following the C99 specification, on hosts that don't have stdbool.h.  This
 * logic is based heavily on the example in the Autoconf manual.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 *
 * The authors hereby relinquish any claim to any copyright that they may have
 * in this work, whether granted under contract or by operation of law or
 * international treaty, and hereby commit to the public, at large, that they
 * shall not, at any time in the future, seek to enforce any copyright in this
 * work against any person or entity, or prevent any person or entity from
 * copying, publishing, distributing or creating derivative works of this
 * work.
 */

#ifndef PORTABLE_STDBOOL_H
#define PORTABLE_STDBOOL_H 1

/*
 * Allow inclusion of config.h to be skipped, since sometimes we have to use a
 * stripped-down version of config.h with a different name.
 */
#ifndef CONFIG_H_INCLUDED
# include "config.h"
#endif

#if INN_HAVE_STDBOOL_H
# include <stdbool.h>
#else
# if INN_HAVE__BOOL
#  define bool _Bool
# else
#  ifdef __cplusplus
typedef bool _Bool;
#  elif _WIN32
#   include <windef.h>
#   define bool BOOL
#  else
typedef unsigned char _Bool;
#   define bool _Bool
#  endif
# endif
# define false 0
# define true  1
# define __bool_true_false_are_defined 1
#endif

/*
 * If we define bool and don't tell Perl, it will try to define its own and
 * fail.  Only of interest for programs that also include Perl headers.
 */
#ifndef HAS_BOOL
# define HAS_BOOL 1
#endif

#endif /* !PORTABLE_STDBOOL_H */
