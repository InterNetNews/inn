/*  $Id$
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
**  adding them to INN's library.  If the system doesn't define a SUN_LEN
**  macro, one will be provided.  Also provides some standard #defines.
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

/* SCO OpenServer gets int32_t from here. */
#if HAVE_SYS_BITYPES_H
# include <sys/bitypes.h>
#endif

BEGIN_DECLS

/* Handle defining fseeko and ftello.  If HAVE_FSEEKO is defined, the system
   header files take care of this for us.  Otherwise, see if we're building
   with large file support.  If we are, we have to provide some real fseeko
   and ftello implementation; declare them if they're not already declared and
   we'll use replacement versions in libinn.  If we're not using large files,
   we can safely just use fseek and ftell.

   We'd rather use fseeko and ftello unconditionally, even when we're not
   building with large file support, since they're a better interface.
   Unfortunately, they're available but not declared on some systems unless
   building with large file support, the AC_FUNC_FSEEKO Autoconf function
   always turns on large file support, and our fake declarations won't work on
   some systems (like HP_UX).  This is the best compromise we've been able to
   come up with. */
#if !HAVE_FSEEKO
# if DO_LARGEFILES
#  if !HAVE_DECL_FSEEKO
extern int              fseeko(FILE *, off_t, int);
#  endif
#  if !HAVE_DECL_FTELLO
extern off_t            ftello(FILE *);
#  endif
# else
#  define fseeko(f, o, w) fseek((f), (long)(o), (w))
#  define ftello(f)       ftell(f)
# endif
#endif

/* Provide prototypes for functions not declared in system headers.  Use the
   HAVE_DECL macros for those functions that may be prototyped but
   implemented incorrectly or implemented without a prototype. */
#if !HAVE_ASPRINTF
extern int              asprintf(char **, const char *, ...);
extern int              vasprintf(char **, const char *, va_list);
#endif
#if !HAVE_MKSTEMP
extern int              mkstemp(char *);
#endif
#if !HAVE_DECL_PREAD
extern ssize_t          pread(int, void *, size_t, off_t);
#endif
#if !HAVE_DECL_PWRITE
extern ssize_t          pwrite(int, const void *, size_t, off_t);
#endif
#if !HAVE_SETENV
extern int              setenv(const char *, const char *, int);
#endif
#if !HAVE_SETEUID
extern int              seteuid(uid_t);
#endif
#if !HAVE_DECL_SNPRINTF
extern int              snprintf(char *, size_t, const char *, ...)
    __attribute__((__format__(printf, 3, 4)));
#endif
#if !HAVE_STRERROR
extern const char *     strerror(int);
#endif
#if !HAVE_STRLCAT || !HAVE_DECL_STRLCAT
extern size_t           strlcat(char *, const char *, size_t);
#endif
#if !HAVE_STRLCPY || !HAVE_DECL_STRLCPY
extern size_t           strlcpy(char *, const char *, size_t);
#endif
#if !HAVE_SYMLINK
extern int              symlink(const char *, const char *);
#endif
#if !HAVE_DECL_VSNPRINTF
extern int              vsnprintf(char *, size_t, const char *, va_list);
#endif

END_DECLS

/* "Good enough" replacements for standard functions. */
#if !HAVE_ATEXIT
# define atexit(arg) on_exit((arg), 0)
#endif
#if !HAVE_STRTOUL
# define strtoul(a, b, c) (unsigned long) strtol((a), (b), (c))
#endif

/* This almost certainly isn't necessary, but it's not hurting anything.
   gcc assumes that if SEEK_SET isn't defined none of the rest are either,
   so we certainly can as well. */
#ifndef SEEK_SET
# define SEEK_SET 0
# define SEEK_CUR 1
# define SEEK_END 2
#endif

/* POSIX requires that these be defined in <unistd.h>.  If one of them has
   been defined, all the rest almost certainly have. */
#ifndef STDIN_FILENO
# define STDIN_FILENO   0
# define STDOUT_FILENO  1
# define STDERR_FILENO  2
#endif

/* On some systems, the macros defined by <ctype.h> are only vaild on ASCII
   characters (those characters that isascii() says are ASCII).  This comes
   into play when applying <ctype.h> macros to eight-bit data.  autoconf
   checks for this with as part of AC_HEADER_STDC, so if autoconf doesn't
   think our headers are standard, check isascii() first. */
#if STDC_HEADERS
# define CTYPE(isXXXXX, c) (isXXXXX((unsigned char)(c)))
#else
# define CTYPE(isXXXXX, c) \
    (isascii((unsigned char)(c)) && isXXXXX((unsigned char)(c)))
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
#define PIPE_READ       0
#define PIPE_WRITE      1

/* Used for iterating through arrays.  ARRAY_SIZE returns the number of
   elements in the array (useful for a < upper bound in a for loop) and
   ARRAY_END returns a pointer to the element past the end (ISO C99 makes it
   legal to refer to such a pointer as long as it's never dereferenced). */
#define ARRAY_SIZE(array)       (sizeof(array) / sizeof((array)[0]))
#define ARRAY_END(array)        (&(array)[ARRAY_SIZE(array)])

/* C99 requires va_copy.  Older versions of GCC provide __va_copy.  Per the
   Autoconf manual, memcpy is a generally portable fallback. */
#ifndef va_copy
# ifdef __va_copy
#  define va_copy(d, s)         __va_copy((d), (s))
# else
#  define va_copy(d, s)         memcpy(&(d), &(s), sizeof(va_list))
# endif
#endif

#endif /* !CLIBRARY_H */
