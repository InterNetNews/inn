/*
 * Standard system includes and portability adjustments.
 *
 * Declarations of routines and variables in the C library.  Including this
 * file is the equivalent of including all of the following headers,
 * portably:
 *
 *     #include <inttypes.h>
 *     #include <limits.h>
 *     #include <stdarg.h>
 *     #include <stdbool.h>
 *     #include <stddef.h>
 *     #include <stdio.h>
 *     #include <stdlib.h>
 *     #include <stdint.h>
 *     #include <string.h>
 *     #include <strings.h>
 *     #include <sys/types.h>
 *     #include <unistd.h>
 *
 * Missing functions are provided via #define or prototyped if available from
 * the portable helper library.  Also provides some standard #defines.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2014, 2016, 2018, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2006-2011, 2013-2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#ifndef PORTABLE_SYSTEM_H
#define PORTABLE_SYSTEM_H 1

/* Make sure we have our configuration information. */
#include "config.h"

/* BEGIN_DECL and __attribute__. */
#include "portable/macros.h"

/* A set of standard ANSI C headers.  We don't care about pre-ANSI systems. */
#if HAVE_INTTYPES_H
#    include <inttypes.h>
#endif
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#if HAVE_STDINT_H
#    include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_STRINGS_H
#    include <strings.h>
#endif
#include <sys/types.h>
#if HAVE_UNISTD_H
#    include <unistd.h>
#endif

/* SCO OpenServer gets int32_t from here. */
#if HAVE_SYS_BITYPES_H
#    include <sys/bitypes.h>
#endif

/* Get the bool type. */
#include "portable/stdbool.h"

/* In case uint32_t and associated limits weren't defined. */
#ifndef UINT32_MAX
#    define UINT32_MAX 4294967295UL
#endif

/* Windows provides snprintf under a different name. */
#ifdef _WIN32
#    define snprintf _snprintf
#endif

/* Define sig_atomic_t if it's not available in signal.h. */
#ifndef HAVE_SIG_ATOMIC_T
typedef int sig_atomic_t;
#endif

/* Windows does not define ssize_t. */
#ifndef HAVE_SSIZE_T
typedef ptrdiff_t ssize_t;
#endif

/*
 * POSIX requires that these be defined in <unistd.h>.  If one of them has
 * been defined, all the rest almost certainly have.
 */
#ifndef STDIN_FILENO
#    define STDIN_FILENO  0
#    define STDOUT_FILENO 1
#    define STDERR_FILENO 2
#endif

/*
 * This almost certainly isn't necessary, but it's not hurting anything.
 * gcc assumes that if SEEK_SET isn't defined none of the rest are either,
 * so we certainly can as well.
 */
#ifndef SEEK_SET
#    define SEEK_SET 0
#    define SEEK_CUR 1
#    define SEEK_END 2
#endif

/*
 * C99 requires va_copy.  Older versions of GCC provide __va_copy.  Per the
 * Autoconf manual, memcpy is a generally portable fallback.
 */
#ifndef va_copy
#    ifdef __va_copy
#        define va_copy(d, s) __va_copy((d), (s))
#    else
#        define va_copy(d, s) memcpy(&(d), &(s), sizeof(va_list))
#    endif
#endif

/*
 * If explicit_bzero is not available, fall back on memset.  This does NOT
 * provide any of the security guarantees of explicit_bzero and will probably
 * be optimized away by the compiler.  It just ensures that code will compile
 * and function on systems without explicit_bzero.
 */
#if !HAVE_EXPLICIT_BZERO
#    define explicit_bzero(s, n) memset((s), 0, (n))
#endif

BEGIN_DECLS

/*
 * Handle defining fseeko and ftello.  If HAVE_FSEEKO is defined, the system
 * header files take care of this for us.  Otherwise, see if we're building
 * with large file support.  If we are, we have to provide some real fseeko
 * and ftello implementation; declare them if they're not already declared and
 * we'll use replacement versions in libinn.  If we're not using large files,
 * we can safely just use fseek and ftell.
 *
 * We'd rather use fseeko and ftello unconditionally, even when we're not
 * building with large file support, since they're a better interface.
 * Unfortunately, they're available but not declared on some systems unless
 * building with large file support, the AC_FUNC_FSEEKO Autoconf function
 * always turns on large file support, and our fake declarations won't work on
 * some systems (like HP_UX).  This is the best compromise we've been able to
 * come up with.
 */
#if !HAVE_FSEEKO
#    if DO_LARGEFILES
#        if !HAVE_DECL_FSEEKO
extern int fseeko(FILE *, off_t, int);
#        endif
#        if !HAVE_DECL_FTELLO
extern off_t ftello(FILE *);
#        endif
#    else
#        define fseeko(f, o, w) fseek((f), (long) (o), (w))
#        define ftello(f)       ftell(f)
#    endif
#endif

/* Other prototypes. */
#if !HAVE_DECL_PREAD
extern ssize_t pread(int, void *, size_t, off_t);
#endif
#if !HAVE_DECL_PWRITE
extern ssize_t pwrite(int, const void *, size_t, off_t);
#endif
#if !HAVE_SYMLINK
extern int symlink(const char *, const char *);
#endif

/*
 * Provide prototypes for functions not declared in system headers.  Use the
 * HAVE_DECL macros for those functions that may be prototyped but implemented
 * incorrectly or implemented without a prototype.
 */
#if !HAVE_ASPRINTF
extern int asprintf(char **, const char *, ...)
    __attribute__((__format__(printf, 2, 3)));
extern int vasprintf(char **, const char *, va_list)
    __attribute__((__format__(printf, 2, 0)));
#endif
#if !HAVE_DECL_SNPRINTF
extern int snprintf(char *, size_t, const char *, ...)
    __attribute__((__format__(printf, 3, 4)));
#endif
#if !HAVE_DECL_VSNPRINTF
extern int vsnprintf(char *, size_t, const char *, va_list)
    __attribute__((__format__(printf, 3, 0)));
#endif
#if !HAVE_MKSTEMP
extern int mkstemp(char *);
#endif
#if !HAVE_DECL_REALLOCARRAY
extern void *reallocarray(void *, size_t, size_t);
#endif
#if !HAVE_SETENV
extern int setenv(const char *, const char *, int);
#endif
#if !HAVE_SETEUID
extern int seteuid(uid_t);
#endif
#if !HAVE_DECL_STRLCAT
extern size_t strlcat(char *, const char *, size_t);
#endif
#if !HAVE_DECL_STRLCPY
extern size_t strlcpy(char *, const char *, size_t);
#endif
#if !HAVE_GETPAGESIZE
extern int getpagesize(void);
#endif
#if !HAVE_STRCASECMP
extern int strcasecmp(const char *, const char *);
extern int strncasecmp(const char *, const char *, size_t);
#endif
#if !HAVE_STRSPN
extern size_t strspn(const char *, const char *);
#endif
#if !HAVE_STRTOK
extern char *strtok(char *, const char *);
#endif

END_DECLS

#endif /* !PORTABLE_SYSTEM_H */
