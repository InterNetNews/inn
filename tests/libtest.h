/*  $Id$
**
**  Some utility routines for writing tests.
*/

#ifndef LIBTEST_H
#define LIBTEST_H 1

#include "config.h"
#include <inn/defines.h>

/* A global buffer into which errors_capture stores errors. */
extern char *errors;

BEGIN_DECLS

void ok(int n, int success);
void ok_int(int n, int wanted, int seen);
void ok_double(int n, double wanted, double seen);
void ok_string(int n, const char *wanted, const char *seen);
void skip(int n, const char *reason);

/* Report the same status on, or skip, the next count tests. */
void ok_block(int n, int count, int success);
void skip_block(int n, int count, const char *reason);

/* Print out the number of tests and set standard output to line buffered. */
void test_init(int count);

/* Turn on capturing of errors with errors_capture.  Errors reported by warn
   will be stored in the global errors variable.  Turn this off again with
   errors_uncapture.  Caller is responsible for freeing errors when done. */
void errors_capture(void);
void errors_uncapture(void);

END_DECLS

#endif /* LIBTEST_H */
