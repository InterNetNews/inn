/* $Id$
**
**  Error reporting (possibly fatal).
**
**  Usage:
**
**      extern int cleanup(void);
**      extern void log(int, const char *, va_list, int);
**
**      error_fatal_cleanup = cleanup;
**      error_program_name = argv[0];
**
**      warn("Something horrible happened at %lu", time);
**      syswarn("Couldn't unlink temporary file %s", tmpfile);
**
**      die("Something fatal happened at %lu", time);
**      sysdie("open of %s failed", filename);
**
**      warn_set_handlers(1, log);
**      warn("This now goes through our log function");
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
**
**  Honoring error_program_name and printing to stderr is just the default
**  handler; with warn_set_handlers or die_set_handlers the handlers for
**  warn and die can be changed.  These functions take a count of handlers
**  and then that many function pointers, each one to a function that takes
**  a message length (the number of characters snprintf generates given the
**  format and arguments), a format, an argument list as a va_list, and the
**  applicable errno value (if any).
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <stdarg.h>
#include <syslog.h>

#include "libinn.h"

/* The default handler list. */
static error_handler_t default_handlers[2] = { error_log_stderr, NULL };

/* The list of logging functions currently in effect. */
static error_handler_t *die_handlers  = default_handlers;
static error_handler_t *warn_handlers = default_handlers;

/* If non-NULL, called before exit and its return value passed to exit. */
int (*error_fatal_cleanup)(void) = NULL;

/* If non-NULL, prepended (followed by ": ") to all error messages. */
const char *error_program_name = NULL;


void
warn_set_handlers(int count, ...)
{
    va_list args;
    int i;

    if (warn_handlers != default_handlers)
        free(warn_handlers);
    warn_handlers = xmalloc(sizeof(error_handler_t) * (count + 1));
    va_start(args, count);
    for (i = 0; i < count; i++)
        warn_handlers[i] = (error_handler_t) va_arg(args, error_handler_t);
    va_end(args);
    warn_handlers[count] = NULL;
}

void
die_set_handlers(int count, ...)
{
    va_list args;
    int i;

    if (die_handlers != default_handlers)
        free(die_handlers);
    die_handlers = xmalloc(sizeof(error_handler_t) * (count + 1));
    va_start(args, count);
    for (i = 0; i < count; i++)
        die_handlers[i] = (error_handler_t) va_arg(args, error_handler_t);
    va_end(args);
    die_handlers[count] = NULL;
}


void
error_log_stderr(int len, const char *fmt, va_list args, int err)
{
    len = len;			/* ARGSUSED */
    fflush(stdout);
    if (error_program_name != NULL)
        fprintf(stderr, "%s: ", error_program_name);
    vfprintf(stderr, fmt, args);
    if (err)
        fprintf(stderr, ": %s", strerror(err));
    fprintf(stderr, "\n");
}


static void
error_log_syslog(int pri, int len, const char *fmt, va_list args, int err)
{
    char *buffer;

    buffer = malloc(len + 1);
    if (buffer == NULL) {
        fprintf(stderr, "failed to malloc %u bytes at %s line %d: %s",
                len + 1, __FILE__, __LINE__, strerror(errno));
        exit(1);
    }
    vsnprintf(buffer, len + 1, fmt, args);
    syslog(pri, err ? "%s: %m" : "%s", buffer);
    free(buffer);
}


void
error_log_syslog_warning(int len, const char *fmt, va_list args, int err)
{
    error_log_syslog(LOG_WARNING, len, fmt, args, err);
}


void
error_log_syslog_err(int len, const char *fmt, va_list args, int err)
{
    error_log_syslog(LOG_ERR, len, fmt, args, err);
}


void
warn(const char *format, ...)
{
    va_list args;
    error_handler_t *log;
    int length;

    va_start(args, format);
    length = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (length < 0) return;
    for (log = warn_handlers; *log != NULL; log++) {
        va_start(args, format);
        (**log)(length, format, args, 0);
        va_end(args);
    }
}

void
syswarn(const char *format, ...)
{
    va_list args;
    error_handler_t *log;
    int length;
    int error = errno;

    va_start(args, format);
    length = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (length < 0) return;
    for (log = warn_handlers; *log != NULL; log++) {
        va_start(args, format);
        (**log)(length, format, args, error);
        va_end(args);
    }
}

void
die(const char *format, ...)
{
    va_list args;
    error_handler_t *log;
    int length;

    va_start(args, format);
    length = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (length >= 0)
        for (log = die_handlers; *log != NULL; log++) {
            va_start(args, format);
            (**log)(length, format, args, 0);
            va_end(args);
        }
    exit(error_fatal_cleanup ? (*error_fatal_cleanup)() : 1);
}

void
sysdie(const char *format, ...)
{
    va_list args;
    error_handler_t *log;
    int length;
    int error = errno;

    va_start(args, format);
    length = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (length >= 0)
        for (log = die_handlers; *log != NULL; log++) {
            va_start(args, format);
            (**log)(length, format, args, error);
            va_end(args);
        }
    exit(error_fatal_cleanup ? (*error_fatal_cleanup)() : 1);
}
