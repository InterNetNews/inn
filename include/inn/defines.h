/*  $Id$
**
**  Portable defines used by other INN header files.
**
**  In order to make the libraries built by INN usable by other software,
**  INN needs to install several header files.  Installing autoconf-
**  generated header files, however, is a bad idea, since the defines will
**  conflict with other software that uses autoconf.
**
**  This header contains common definitions, such as internal typedefs and
**  macros, common to INN's header files but not based on autoconf probes.
**  As such, it's limited in what it can do; if compiling software against
**  INN's header files on a system not supporting basic ANSI C features
**  (such as const) or standard types (like size_t), the software may need
**  to duplicate the tests that INN itself performs, generate a config.h,
**  and make sure that config.h is included before any INN header files.
*/

#ifndef INN_DEFINES_H
#define INN_DEFINES_H 1

#include <inn/system.h>

/* BEGIN_DECLS is used at the beginning of declarations so that C++
   compilers don't mangle their names.  END_DECLS is used at the end. */
#undef BEGIN_DECLS
#undef END_DECLS
#ifdef __cplusplus
# define BEGIN_DECLS    extern "C" {
# define END_DECLS      }
#else
# define BEGIN_DECLS    /* empty */
# define END_DECLS      /* empty */
#endif

/* __attribute__ is available in gcc 2.5 and later, but only with gcc 2.7
   could you use the __format__ form of the attributes, which is what we use
   (to avoid confusion with other macros). */
#ifndef __attribute__
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
#  define __attribute__(spec)   /* empty */
# endif
#endif

/* Used for unused parameters to silence gcc warnings. */
#define UNUSED  __attribute__((__unused__))

/* Make available the bool type. */
#if INN_HAVE_STDBOOL_H
# include <stdbool.h>
#else
# if !INN_HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
typedef unsigned char _Bool;
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif

/* Tell Perl that we have a bool type. */
#ifndef HAS_BOOL
# define HAS_BOOL 1
#endif

#endif /* !INN_DEFINES_H */
