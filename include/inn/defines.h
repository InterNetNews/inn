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

/* Make available the bool type.  INN internally uses TRUE and FALSE instead
   in a lot of places, so make them available as well. */
#if INN_HAVE_STDBOOL_H
# include <stdbool.h>
# undef TRUE
# undef FALSE
# define TRUE   true
# define FALSE  false

#else

/* We don't have stdbool.h and therefore need to hack something up
   ourselves.  These methods are taken from Perl with some modifications. */

# undef true
# undef false
# define true   (1)
# define false  (0)

/* Lots of INN still uses TRUE and FALSE. */
# undef TRUE
# undef FALSE
# define TRUE   true
# define FALSE  false

/* C++ has a native bool type, and our true and false will work with it. */
# ifdef __cplusplus
#  define HAS_BOOL 1
# endif

/* The NeXT dynamic loader headers will not build with the bool macro, so
   use an enum instead (which appears to work). */
# if !defined(HAS_BOOL) && (defined(NeXT) || defined(__NeXT__))
#  undef FALSE
#  undef TRUE
typedef enum bool { FALSE = 0, TRUE = 1 } bool;
#  define HAS_BOOL 1
# endif /* NeXT || __NeXT__ */

# if !HAS_BOOL
#  define bool int
#  define HAS_BOOL 1
# endif

#endif /* __STDC_VERSION__ < 199901L */

#endif /* !INN_DEFINES_H */
