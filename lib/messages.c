/* $Id$
**
**  Message and error reporting (possibly fatal).
**
**  Usage:
**
**      extern int cleanup(void);
**      extern void log(int, const char *, va_list, int);
**
**      message_fatal_cleanup = cleanup;
**      message_program_name = argv[0];
**
**      warn("Something horrible happened at %lu", time);
**      syswarn("Couldn't unlink temporary file %s", tmpfile);
**
**      die("Something fatal happened at %lu", time);
**      sysdie("open of %s failed", filename);
**
**      debug("Some debugging message about %s", string);
**      notice("Informational notices");
**
**      message_handlers_warn(1, log);
**      warn("This now goes through our log function");
**
**  These functions implement message reporting through user-configurable
**  handler functions.  debug() only does something if DEBUG is defined, and
**  notice() and warn() just output messages as configured.  die() similarly
**  outputs a message but then exits, normally with a status of 1.
**
**  The sys* versions do the same, but append a colon, a space, and the
**  results of strerror(errno) to the end of the message.  All functions
**  accept printf-style formatting strings and arguments.
**
**  If message_fatal_cleanup is non-NULL, it is called before exit by die and
**  sysdie and its return value is used as the argument to exit.  It is a
**  pointer to a function taking no arguments and returning an int, and can be
**  used to call cleanup functions or to exit in some alternate fashion (such
**  as by calling _exit).
**
**  If message_program_name is non-NULL, the string it points to, followed by
**  a colon and a space, is prepended to all error messages logged through the
**  message_log_stdout and message_log_stderr message handlers (the former is
**  the default for notice, and the latter is the default for warn and die).
**
**  Honoring error_program_name and printing to stderr is just the default
**  handler; with message_handlers_* the handlers for any message function can
**  be changed.  By default, notice prints to stdout, warn and die print to
**  stderr, and the others don't do anything at all.  These functions take a
**  count of handlers and then that many function pointers, each one to a
**  function that takes a message length (the number of characters snprintf
**  generates given the format and arguments), a format, an argument list as a
**  va_list, and the applicable errno value (if any).
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <syslog.h>

#include "inn/messages.h"
#include "inn/libinn.h"

/* The default handler lists. */
static message_handler_func stdout_handlers[2] = {
    message_log_stdout, NULL
};
static message_handler_func stderr_handlers[2] = {
    message_log_stderr, NULL
};

/* The list of logging functions currently in effect. */
static message_handler_func *debug_handlers  = NULL;
static message_handler_func *notice_handlers = stdout_handlers;
static message_handler_func *warn_handlers   = stderr_handlers;
static message_handler_func *die_handlers    = stderr_handlers;

/* If non-NULL, called before exit and its return value passed to exit. */
int (*message_fatal_cleanup)(void) = NULL;

/* If non-NULL, prepended (followed by ": ") to messages. */
const char *message_program_name = NULL;


/*
**  Set the handlers for a particular message function.  Takes a pointer to
**  the handler list, the count of handlers, and the argument list.
*/
static void
message_handlers(message_handler_func **list, size_t count, va_list args)
{
    size_t i;

    if (*list != stdout_handlers && *list != stderr_handlers)
        free(*list);
    *list = xmalloc(sizeof(message_handler_func) * (count + 1));
    for (i = 0; i < count; i++)
        (*list)[i] = (message_handler_func) va_arg(args, message_handler_func);
    (*list)[count] = NULL;
}


/*
**  There's no good way of writing these handlers without a bunch of code
**  duplication since we can't assume variadic macros, but I can at least make
**  it easier to write and keep them consistent.
*/
#define HANDLER_FUNCTION(type)                                  \
    void                                                        \
    message_handlers_ ## type(size_t count, ...)                \
    {                                                           \
        va_list args;                                           \
                                                                \
        va_start(args, count);                                  \
        message_handlers(& type ## _handlers, count, args);     \
        va_end(args);                                           \
    }
HANDLER_FUNCTION(debug)
HANDLER_FUNCTION(notice)
HANDLER_FUNCTION(warn)
HANDLER_FUNCTION(die)


/*
**  Print a message to stdout, supporting message_program_name.
*/
void
message_log_stdout(size_t len UNUSED, const char *fmt, va_list args, int err)
{
    if (message_program_name != NULL)
        fprintf(stdout, "%s: ", message_program_name);
    vfprintf(stdout, fmt, args);
    if (err)
        fprintf(stdout, ": %s", strerror(err));
    fprintf(stdout, "\n");
    fflush(stdout);
}


/*
**  Print a message to stderr, supporting message_program_name.  Also flush
**  stdout so that errors and regular output occur in the right order.
*/
void
message_log_stderr(size_t len UNUSED, const char *fmt, va_list args, int err)
{
    fflush(stdout);
    if (message_program_name != NULL)
        fprintf(stderr, "%s: ", message_program_name);
    vfprintf(stderr, fmt, args);
    if (err)
        fprintf(stderr, ": %s", strerror(err));
    fprintf(stderr, "\n");
}


/*
**  Log a message to syslog.  This is a helper function used to implement all
**  of the syslog message log handlers.  It takes the same arguments as a
**  regular message handler function but with an additional priority
**  argument.
*/
static void
message_log_syslog(int pri, size_t len, const char *fmt, va_list args, int err)
{
    char *buffer;

    buffer = malloc(len + 1);
    if (buffer == NULL) {
        fprintf(stderr, "failed to malloc %lu bytes at %s line %d: %s",
                (unsigned long) len + 1, __FILE__, __LINE__, strerror(errno));
        exit(message_fatal_cleanup ? (*message_fatal_cleanup)() : 1);
    }
    vsnprintf(buffer, len + 1, fmt, args);
    if (err == 0)
        syslog(pri, "%s", buffer);
    else
        syslog(pri, "%s: %s", buffer, strerror(err));
    free(buffer);
}


/*
**  Do the same sort of wrapper to generate all of the separate syslog logging
**  functions.
*/
#define SYSLOG_FUNCTION(name, type)                                        \
    void                                                                   \
    message_log_syslog_ ## name(size_t l, const char *f, va_list a, int e) \
    {                                                                      \
        message_log_syslog(LOG_ ## type, l, f, a, e);                      \
    }
SYSLOG_FUNCTION(debug,   DEBUG)
SYSLOG_FUNCTION(info,    INFO)
SYSLOG_FUNCTION(notice,  NOTICE)
SYSLOG_FUNCTION(warning, WARNING)
SYSLOG_FUNCTION(err,     ERR)
SYSLOG_FUNCTION(crit,    CRIT)


/*
**  All of the message handlers.  There's a lot of code duplication here too,
**  but each one is still *slightly* different and va_start has to be called
**  multiple times, so it's hard to get rid of the duplication.
*/

#ifdef DEBUG
void
debug(const char *format, ...)
{
    va_list args;
    message_handler_func *log;
    ssize_t length;

    if (debug_handlers == NULL)
        return;
    va_start(args, format);
    length = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (length < 0)
        return;
    for (log = debug_handlers; *log != NULL; log++) {
        va_start(args, format);
        (**log)((size_t) length, format, args, 0);
        va_end(args);
    }
}
#elif !INN_HAVE_C99_VAMACROS && !INN_HAVE_GNU_VAMACROS
void debug(const char *format UNUSED, ...) { }
#endif

void
notice(const char *format, ...)
{
    va_list args;
    message_handler_func *log;
    ssize_t length;

    va_start(args, format);
    length = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (length < 0)
        return;
    for (log = notice_handlers; *log != NULL; log++) {
        va_start(args, format);
        (**log)((size_t) length, format, args, 0);
        va_end(args);
    }
}

void
sysnotice(const char *format, ...)
{
    va_list args;
    message_handler_func *log;
    ssize_t length;
    int error = errno;

    va_start(args, format);
    length = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (length < 0)
        return;
    for (log = notice_handlers; *log != NULL; log++) {
        va_start(args, format);
        (**log)((size_t) length, format, args, error);
        va_end(args);
    }
}

void
warn(const char *format, ...)
{
    va_list args;
    message_handler_func *log;
    ssize_t length;

    va_start(args, format);
    length = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (length < 0)
        return;
    for (log = warn_handlers; *log != NULL; log++) {
        va_start(args, format);
        (**log)((size_t) length, format, args, 0);
        va_end(args);
    }
}

void
syswarn(const char *format, ...)
{
    va_list args;
    message_handler_func *log;
    ssize_t length;
    int error = errno;

    va_start(args, format);
    length = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (length < 0)
        return;
    for (log = warn_handlers; *log != NULL; log++) {
        va_start(args, format);
        (**log)((size_t) length, format, args, error);
        va_end(args);
    }
}

void
die(const char *format, ...)
{
    va_list args;
    message_handler_func *log;
    ssize_t length;

    va_start(args, format);
    length = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (length >= 0)
        for (log = die_handlers; *log != NULL; log++) {
            va_start(args, format);
            (**log)((size_t) length, format, args, 0);
            va_end(args);
        }
    exit(message_fatal_cleanup ? (*message_fatal_cleanup)() : 1);
}

void
sysdie(const char *format, ...)
{
    va_list args;
    message_handler_func *log;
    ssize_t length;
    int error = errno;

    va_start(args, format);
    length = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (length >= 0)
        for (log = die_handlers; *log != NULL; log++) {
            va_start(args, format);
            (**log)((size_t) length, format, args, error);
            va_end(args);
        }
    exit(message_fatal_cleanup ? (*message_fatal_cleanup)() : 1);
}
