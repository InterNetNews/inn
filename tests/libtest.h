/*  $Id$
**
**  Some utility routines for writing tests.
*/

#ifndef TESTLIB_H
#define TESTLIB_H 1

#include "config.h"
#include <inn/defines.h>

/* A global buffer into which errors_capture stores errors. */
extern char *errors;

BEGIN_DECLS

void ok(int n, int success);
void ok_int(int n, int wanted, int seen);
void ok_string(int n, const char *wanted, const char *seen);

/* Turn on capturing of errors with errors_capture.  Errors reported by warn
   will be stored in the global errors variable.  Turn this off again with
   errors_uncapture.  Caller is responsible for freeing errors when done. */
void errors_capture(void);
void errors_uncapture(void);

END_DECLS

#endif /* TESTLIB_H */
