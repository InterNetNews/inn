/*  $Revision$
**
**  Here be declarations of routines and variables in the C library.
**  Including this file is the equivalent of including all of the following
**  headers, portably:
**
**      #include <sys/types.h>
**      #include <stdio.h>
**      #include <stdlib.h>
**      #include <stddef.h>
**      #include <stdint.h>
**      #include <string.h>
**      #include <limits.h>
**      #include <unistd.h>
**
**  Missing functions are provided via #define or prototyped if we'll be
**  adding them to INN's library.  vfork.h is included if it exists.  If
**  the system doesn't define a SUN_LEN macro, one will be provided.  Also
**  provides some standard #defines and typedefs (TRUE, FALSE, STDIN,
**  STDOUT, STDERR, PIPE_READ, PIPE_WRITE).
**
**  This file also does some additional things that it shouldn't be doing
**  any more; those are all below the LEGACY comment.  Those will eventually
**  be removed; don't depend on them continuing to remain in this file.
*/

#ifndef CLIBRARY_H
#define CLIBRARY_H

/* Make sure we have our configuration information. */
#include "config.h"

/* Any system that doesn't have these loses. */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

/* Tell C++ not to mangle prototypes. */
#ifdef __cplusplus
extern "C" {
#endif

/* The autoconf manual advises, if STDC_HEADERS isn't set, don't even try to
   include the right bizarre combination of headers to get the right
   prototypes, just prototype the stuff you have to (anything returning
   something other than an int).  Don't bother trying to get rid of compiler
   warnings on non-ANSI systems; it's not worth the trouble. */
#ifdef STDC_HEADERS
# include <string.h>
#else
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
extern char *strcat();
extern char *strncat();
extern char *strchr();
extern char *strrchr();
extern char *strcpy();
extern char *strncpy();
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n)  bcopy((s), (d), (n))
# endif
extern void *memchr();
extern void *memmove();
extern void *memset();
#endif /* !STDC_HEADERS */

#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

#ifdef HAVE_STDDEF_H
# include <stddef.h>
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_VFORK_H
# include <vfork.h>
#endif

/* Provide prototypes for functions we're replacing. */
#ifndef HAVE_PREAD
extern ssize_t pread(int fd, void *buf, size_t nbyte, OFFSET_T offset);
#endif
#ifndef HAVE_PWRITE
extern ssize_t pwrite(int fd, void *buf, size_t nbyte, OFFSET_T offset);
#endif
#ifndef HAVE_INET_NTOA
extern char *inet_ntoa();
#endif
#ifndef HAVE_STRERROR
extern char *strerror();
#endif
#ifndef HAVE_STRNCASECMP
extern int strncasecmp();
#endif
#ifndef HAVE_STRSPN
extern size_t strspn();
#endif

/* "Good enough" replacements for standard functions. */
#ifndef HAVE_ATEXIT
# define atexit(arg) on_exit((arg), 0)
#endif
#ifndef HAVE_STRTOUL
# define strtoul(a, b, c) strtol((a), (b), (c))
#endif

/* Large file support.  Use the off_t versions, if available. */
#ifdef HAVE_FTELLO
# define ftell ftello
#endif
#ifdef HAVE_FSEEKO
# define fseek fseeko
#endif

/* readv(), writev(), and friends want to know how many elements can be in a
   struct iovec.  This should be IOV_MAX but some systems may lack that
   #define, so if it isn't defined figure it out from the system type. */
#ifndef IOV_MAX
# ifdef UIO_MAXIOV
#  define IOV_MAX UIO_MAXIOV
# else
   /* FreeBSD 3.0 or above. */
#  ifdef __FreeBSD__
#   include <osreldate.h>
#   if (__FreeBSD_version >= 222000)
#    define IOV_MAX 1024
#   endif
#  endif
   /* BSDI. */
#  ifdef __bsdi__
#   define IOV_MAX 1024
#  endif
# endif /* !UIO_MAXIOV */
#endif /* !IOV_MAX */

/* HP-UX, Solaris, NEC, and IRIX all limit to 16, which is the smallest
   known limit, so if still not set fall back on that. */
#ifndef IOV_MAX
# define IOV_MAX 16
#endif

/* mmap() flags.  This really doesn't belong in this header file; it should
   be moved to a header file specifically for mmap-related things. */
#ifdef MAP_FILE
#define MAP__ARG (MAP_FILE | MAP_SHARED)
#else
#define MAP__ARG (MAP_SHARED)
#endif

/* This almost certainly isn't necessary, but it's not hurting anything. */
#ifndef SEEK_SET
# define SEEK_SET 0
#endif
#ifndef SEEK_END
# define SEEK_END 2
#endif

/* On some systems, the macros defined by <ctype.h> are only vaild on ASCII
   characters (those characters that isascii() says are ASCII).  This comes
   into play when applying <ctype.h> macros to eight-bit data.  autoconf
   checks for this with as part of AC_HEADER_STDC, so if autoconf doesn't
   think our headers are standard, check isascii() first. */
#ifdef STDC_HEADERS
# define CTYPE(isXXXXX, c) (isXXXXX((c)))
#else
# define CTYPE(isXXXXX, c) (isascii((c)) && isXXXXX((c)))
#endif

/* POSIX.1g requires <sys/un.h> to define a SUN_LEN macro for determining
   the real length of a struct sockaddr_un, but it's not available
   everywhere yet.  If autoconf couldn't find it, define our own.  This
   definition is from 4.4BSD by way of Stevens, Unix Network Programming
   (2nd edition), vol. 1, pg. 917. */
#ifndef HAVE_SUN_LEN
# define SUN_LEN(sun) \
    (sizeof(*(sun)) - sizeof((sun)->sun_path) + strlen((sun)->sun_path))
#endif

/* Self-documenting names for pretty much universal constants. */
#ifndef TRUE
# define TRUE                   1
#endif
#ifndef FALSE
# define FALSE                  0
#endif
#define STDIN                   0
#define STDOUT                  1
#define STDERR                  2

/* Used to name the elements of the array passed to pipe(). */
#define PIPE_READ               0
#define PIPE_WRITE              1


/*
**  LEGACY
**
**  Everything below this point is here so that parts of INN that haven't
**  been tweaked to use more standard constructs don't break.  Don't count
**  on any of this staying around, and if you're knee-deep in a file that
**  uses any of this, please consider fixing it.
*/

/* bool really isn't portable, and we can't just use autoconf magic to fix
   it up because that doesn't give the false and true constants which
   similarly aren't portable.  No good solution for this except to go back
   to using 0 and 1 or using lots of magic to #define false and true. */
#if ! defined (DO_NEED_BOOL) && ! defined (DONT_NEED_BOOL)
#define DO_NEED_BOOL 1
#endif

/* All occurrances of these typedefs anywhere should be replaced by their
   ANSI/ISO/standard C definitions given in these typedefs.  autoconf magic
   will make sure that everything except void works fine, and void we're
   just assuming works. */
typedef void *          POINTER;
typedef const void *    CPOINTER;
typedef size_t          SIZE_T;
typedef uid_t           UID_T;
typedef gid_t           GID_T;
typedef pid_t           PID_T;

/* Some functions like accept() and getsockopt() take a pointer to a size_t
   on some platforms and a pointer to an int on others.  Just always using
   size_t should work most everywhere. */
#define	ARGTYPE         size_t

/* These are in C9X, and autoconf makes sure they exist, so again occurances
   of the typedefs should be replaced by the standard type. */
typedef int32_t         INT32_T;
typedef uint32_t        U_INT32_T;

/* autoconf deals with these; just use them directly. */
typedef caddr_t         MMAP_PTR;

/* Return type of signal handlers.  autoconf defines RETSIGTYPE for this
   purpose; use that instead. */
#define SIGHANDLER      RETSIGTYPE

/* There used to be lots of portability here to handle systems that have
   fd_set but not the FD_SET() family of macros.  Since then, various other
   parts of INN have started using fd_set directly without complaints.  The
   intersection between those systems and systems with ANSI C compilers is
   probably now empty, so see if this breaks anyone. */
#define FDSET           fd_set

/* #define instead of typedef for old broken compilers.  We just assume void
   exists now.  Replace FREEVAL with void where you see it. */
#define FREEVAL         void

/* This needs to be moved into libinn.h, since we don't guarantee to provide
   getopt() functionality. */
extern int              optind;
extern char             *optarg;

/* We shouldn't be using HAVE_UNION_WAIT; until we get rid of it, make sure
   to turn it off for HP-UX since it breaks things. */
#if (defined(__hpux) || defined(__hpux__)) && ! defined(HPUX)
# define HPUX
#endif
#if defined(HPUX) && defined(HAVE_UNION_WAIT)
# undef HAVE_UNION_WAIT
#endif

/* Use SUN_LEN instead of AF_UNIX_SOCKSIZE. */
#define AF_UNIX_SOCKSIZE(sun) SUN_LEN(&(sun))

#ifdef __cplusplus
}
#endif

#endif /* !CLIBRARY_H */
