/*  $Id$
**
**  Error reporting (possibly fatal).
**
**  Usage:
** 
**       extern int cleanup(void);
** 
**       error_fatal_cleanup = cleanup;
**       error_program_name = argv[0];
** 
**       warn("Something horrible happened at %lu", time);
**       syswarn("Couldn't unlink temporary file %s", tmpfile);
** 
**       die("Something fatal happened at %lu", time);
**       sysdie("open of %s failed", filename);
** 
**  warn prints an error followed by a newline to stderr.  die does the same
**  but then exits, normally with a status of 1.  The sys* versions do the
**  same, but append a colon, a space, and the results of strerror(errno) to
**  the end of the message.  All functions accept printf-style formatting
**  strings and arguments.
** 
**  If error_fatal_cleanup is non-NULL, it is called before exit by die and
**  sysdie and its return value is used as the argument to exit.  It is a
**  pointer to a function taking no arguments and returning an int.
** 
**  If error_program_name is non-NULL, the string it points to, followed by
**  a colon and a space, is prepended to all error messages.  It is a const
**  char *.
*/

#include "config.h"
#include "libinn.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#if STDC_HEADERS
# include <string.h>
#endif

/* If non-NULL, called before exit and its return value passed to exit. */
int (*error_fatal_cleanup)(void) = NULL;

/* If non-NULL, prepended (followed by ": ") to all error messages. */
const char *error_program_name = NULL;

void
warn(const char *format, ...)
{
    va_list args;

    fflush(stdout);
    if (error_program_name != NULL)
        fprintf(stderr, "%s: ", error_program_name);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void
syswarn(const char *format, ...)
{
    va_list args;
    int error = errno;

    fflush(stdout);
    if (error_program_name != NULL)
        fprintf(stderr, "%s: ", error_program_name);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, ": %s\n", strerror(error));
}

void
die(const char *format, ...)
{
    va_list args;

    fflush(stdout);
    if (error_program_name != NULL)
        fprintf(stderr, "%s: ", error_program_name);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(error_fatal_cleanup != NULL ? (*error_fatal_cleanup)() : 1);
}

void
sysdie(const char *format, ...)
{
    va_list args;
    int error = errno;

    fflush(stdout);
    if (error_program_name != NULL)
        fprintf(stderr, "%s: ", error_program_name);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, ": %s\n", strerror(error));
    exit(error_fatal_cleanup != NULL ? (*error_fatal_cleanup)() : 1);
}
