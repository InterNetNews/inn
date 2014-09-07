/* $Id$
 *
 * Utility functions to test message handling.
 *
 * These functions set up a message handler to trap warn and notice output
 * into a buffer that can be inspected later, allowing testing of error
 * handling.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Copyright 2002, 2004, 2005 Russ Allbery <eagle@eyrie.org>
 * Copyright 2006, 2007, 2009, 2012, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "config.h"
#include "clibrary.h"

#include "tap/macros.h"
#include "tap/messages.h"
#include "tap/string.h"
#include "inn/messages.h"

/* A global buffer into which message_log_buffer stores error messages. */
char *errors = NULL;


/*
 * An error handler that appends all errors to the errors global.  Used by
 * error_capture.
 */
static void
message_log_buffer(int len UNUSED, const char *fmt, va_list args,
                   int error UNUSED)
{
    char *message;

    bvasprintf(&message, fmt, args);
    if (errors == NULL)
        basprintf(&errors, "%s\n", message);
    else {
        char *new_errors;

        basprintf(&new_errors, "%s%s\n", errors, message);
        free(errors);
        errors = new_errors;
    }
    free(message);
}


/*
 * Turn on the capturing of errors.  Errors will be stored in the global
 * errors variable where they can be checked by the test suite.  Capturing is
 * turned off with errors_uncapture.
 */
void
errors_capture(void)
{
    free(errors);
    errors = NULL;
    message_handlers_warn(1, message_log_buffer);
    message_handlers_notice(1, message_log_buffer);
}


/*
 * Turn off the capturing of errors again.
 */
void
errors_uncapture(void)
{
    message_handlers_warn(1, message_log_stderr);
    message_handlers_notice(1, message_log_stdout);
}
