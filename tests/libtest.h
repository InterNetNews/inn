/*  $Id$
**
**  Some utility routines for writing tests.
*/

#ifndef TESTLIB_H
#define TESTLIB_H 1

#include "config.h"
#include <inn/defines.h>

BEGIN_DECLS

void ok(int n, int success);
void ok_int(int n, int wanted, int seen);
void ok_string(int n, const char *wanted, const char *seen);

END_DECLS

#endif /* TESTLIB_H */
