/*  $Revision$
**
**  Here be declarations of routines and variables in the C library.
**  Including this file is the equivalent of including all of the following
**  headers, portably:
**
**      #include <sys/types.h>
**      #include <stdarg.h>
**      #include <stdio.h>
**      #include <stdlib.h>
**      #include <stddef.h>
**      #include <stdint.h>
**      #include <string.h>
**      #include <unistd.h>
**
**  Missing functions are provided via #define or prototyped if we'll be
**  adding them to INN's library.  vfork.h is included if it exists.  If
**  the system doesn't define a SUN_LEN macro, one will be provided.  Also
**  provides some standard #defines and typedefs.
**
**  This file also does some additional things that it shouldn't be doing
**  any more; those are all below the LEGACY comment.  Those will eventually
**  be removed; don't depend on them continuing to remain in this file.
*/

#ifndef CLIBRARY_H
#define CLIBRARY_H 1

/* Make sure we have our configuration information. */
#include "config.h"

/* Assume stdarg is available; don't bother with varargs support any more.
   We need this to be able to declare vsnprintf. */
#include <stdarg.h>

/* This is the same method used by autoconf as of 2000-07-29 for including
   the basic system headers with the addition of handling of strchr,
   strrchr, and memcpy.  Note that we don't attempt to declare any of the
   functions; the number of systems left without ANSI-compatible function
   prototypes isn't high enough to be worth the trouble.  */
#include <stdio.h>
#include <sys/types.h>
#if STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
# if !HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
# if !HAVE_MEMCPY
#  define memcpy(d, s, n)  bcopy((s), (d), (n))
# endif
#endif
#if HAVE_STRING_H
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
#else
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_VFORK_H
# include <vfork.h>
#endif

/* SCO OpenServer gets int32_t from here. */
#if HAVE_SYS_BITYPES_H
# include <sys/bitypes.h>
#endif

BEGIN_DECLS

/* Provide prototypes for functions not declared in system headers.  Use the
   NEED_DECLARATION macros for those functions that may be prototyped but
   implemented incorrectly. */
#if !HAVE_FSEEKO
extern int              fseeko(FILE *, off_t, int);
#endif
#if !HAVE_FTELLO
extern off_t            ftello(FILE *);
#endif
#if !HAVE_HSTRERROR
extern const char *     hstrerror(int);
#endif
#if !HAVE_INET_ATON
extern int              inet_aton(const char *, struct in_addr *);
#endif
#if NEED_DECLARATION_INET_NTOA
extern const char *     inet_aton(const struct in_addr);
#endif
#if !HAVE_PREAD
extern ssize_t          pread(int, void *, size_t, off_t);
#endif
#if !HAVE_PWRITE
extern ssize_t          pwrite(int, const void *, size_t, off_t);
#endif
#if !HAVE_SETEUID
extern int              seteuid(uid_t);
#endif
#if NEED_DECLARATION_SNPRINTF
extern int              snprintf(char *, size_t, const char *, ...)
    __attribute__((__format__(3, 4)));
#endif
#if !HAVE_STRERROR
extern const char *     strerror(int);
#endif
#if !HAVE_SETENV
extern int              setenv(const char *, const char *, int);
#endif
#if NEED_DECLARATION_VSNPRINTF
extern int              vsnprintf(char *, size_t, const char *, va_list);
#endif

/* "Good enough" replacements for standard functions. */
#ifndef HAVE_ATEXIT
# define atexit(arg) on_exit((arg), 0)
#endif
#ifndef HAVE_STRTOUL
# define strtoul(a, b, c) strtol((a), (b), (c))
#endif

/* mmap() flags.  This really doesn't belong in this header file; it should
   be moved to a header file specifically for mmap-related things. */
#ifdef MAP_FILE
# define MAP__ARG (MAP_FILE | MAP_SHARED)
#else
# define MAP__ARG (MAP_SHARED)
#endif

/* This almost certainly isn't necessary, but it's not hurting anything.
   gcc assumes that if SEEK_SET isn't defined none of the rest are either,
   so we certainly can as well. */
#ifndef SEEK_SET
# define SEEK_SET 0
# define SEEK_CUR 1
# define SEEK_END 2
#endif

/* On some systems, the macros defined by <ctype.h> are only vaild on ASCII
   characters (those characters that isascii() says are ASCII).  This comes
   into play when applying <ctype.h> macros to eight-bit data.  autoconf
   checks for this with as part of AC_HEADER_STDC, so if autoconf doesn't
   think our headers are standard, check isascii() first. */
#if STDC_HEADERS
# define CTYPE(isXXXXX, c) (isXXXXX((c)))
#else
# define CTYPE(isXXXXX, c) (isascii((c)) && isXXXXX((c)))
#endif

/* POSIX.1g requires <sys/un.h> to define a SUN_LEN macro for determining
   the real length of a struct sockaddr_un, but it's not available
   everywhere yet.  If autoconf couldn't find it, define our own.  This
   definition is from 4.4BSD by way of Stevens, Unix Network Programming
   (2nd edition), vol. 1, pg. 917. */
#if !HAVE_SUN_LEN
# define SUN_LEN(sun) \
    (sizeof(*(sun)) - sizeof((sun)->sun_path) + strlen((sun)->sun_path))
#endif

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
   on some platforms and a pointer to an int on others (and socklen_t on
   others).  Just always use socklen_t and let autoconf take care of it. */
#define	ARGTYPE         socklen_t

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

/* This needs to be moved into libinn.h, since we don't guarantee to provide
   getopt() functionality. */
extern int              optind;
extern char             *optarg;

/* POSIX requires STDIN_FILENO, STDOUT_FILENO, and STDERR_FILENO; we should
   be using those instead. */
#define STDIN                   0
#define STDOUT                  1
#define STDERR                  2

END_DECLS

#endif /* !CLIBRARY_H */
