/*  $Id$
**
**  Some utility routines for writing tests.
**
**  Herein are a variety of utility routines for writing tests.  All
**  routines of the form ok*() take a test number and some number of
**  appropriate arguments, check to be sure the results match the expected
**  output using the arguments, and print out something appropriate for that
**  test number.  Other utility routines help in constructing more complex
**  tests.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/messages.h"
#include "libinn.h"
#include "libtest.h"

/* A global buffer into which message_log_buffer stores error messages. */
char *errors = NULL;


/*
**  Initialize things.  Turns on line buffering on stdout and then prints out
**  the number of tests in the test suite.
*/
void
test_init(int count)
{
    if (setvbuf(stdout, NULL, _IOLBF, BUFSIZ) != 0)
        syswarn("cannot set stdout to line buffered");
    printf("%d\n", count);
}


/*
**  Takes a boolean success value and assumes the test passes if that value
**  is true and fails if that value is false.
*/
void
ok(int n, int success)
{
    printf("%sok %d\n", success ? "" : "not ", n);
}


/*
**  Takes an expected integer and a seen integer and assumes the test passes
**  if those two numbers match.
*/
void
ok_int(int n, int wanted, int seen)
{
    if (wanted == seen)
        printf("ok %d\n", n);
    else
        printf("not ok %d\n  wanted: %d\n    seen: %d\n", n, wanted, seen);
}


/*
**  Takes a string and what the string should be, and assumes the test
**  passes if those strings match (using strcmp).
*/
void
ok_string(int n, const char *wanted, const char *seen)
{
    if (wanted == NULL)
        wanted = "(null)";
    if (seen == NULL)
        seen = "(null)";
    if (strcmp(wanted, seen) != 0)
        printf("not ok %d\n  wanted: %s\n    seen: %s\n", n, wanted, seen);
    else
        printf("ok %d\n", n);
}


/*
**  Takes an expected integer and a seen integer and assumes the test passes
**  if those two numbers match.
*/
void
ok_double(int n, double wanted, double seen)
{
    if (wanted == seen)
        printf("ok %d\n", n);
    else
        printf("not ok %d\n  wanted: %g\n    seen: %g\n", n, wanted, seen);
}


/*
**  An error handler that appends all errors to the errors global.  Used by
**  error_capture.
*/
static void
message_log_buffer(int len, const char *fmt, va_list args, int error UNUSED)
{
    char *message;

    message = xmalloc(len + 1);
    vsnprintf(message, len + 1, fmt, args);
    if (errors == NULL) {
        errors = concat(message, "\n", (char *) 0);
    } else {
        char *new_errors;

        new_errors = concat(errors, message, "\n", (char *) 0);
        free(errors);
        errors = new_errors;
    }
    free(message);
}


/*
**  Turn on the capturing of errors.  Errors will be stored in the global
**  errors variable where they can be checked by the test suite.  Capturing is
**  turned off with errors_uncapture.
*/
void
errors_capture(void)
{
    if (errors != NULL) {
        free(errors);
        errors = NULL;
    }
    message_handlers_warn(1, message_log_buffer);
}


/*
**  Turn off the capturing of errors again.
*/
void
errors_uncapture(void)
{
    message_handlers_warn(1, message_log_stderr);
}
