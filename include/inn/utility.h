/*  $Id$
**
**  INN utility functions.
**
**  This is a collection of miscellaneous utility functions that aren't
**  logically part of a larger package of routines.  All functions defined in
**  this file must be prefixed with inn_ (and constants and macros, if any,
**  must be prefixed with INN_).  The only exception are the macros handled by
**  <inn/defines.h>.
*/

#ifndef INN_UTILITY_H
#define INN_UTILITY_H 1

#include <inn/defines.h>
#include <sys/types.h>          /* size_t */

BEGIN_DECLS

/* Convert data to an ASCII hex representation (using capital hex digits).
   The length of the output buffer must be at least 2 * input + 1 to hold the
   full representation.  The result is truncated if there isn't enough room
   and will always be nul-terminated. */
void inn_encode_hex(const unsigned char *, size_t, char *, size_t);

/* Convert data from an ASCII hex representation.  No adjustment is made for
   byte order.  The conversion stops at the first character that's not a hex
   digit; upper- and lower-case digits are allowed.  If there are an uneven
   number of input characters, the input is zero-padded at the end (so the
   input string "F" is equivalent to the string "F0").  The output buffer must
   be at least (input - 1) / 2 to hold the full representation.  The result is
   truncated if there isn't enough room. */
void inn_decode_hex(const char *, unsigned char *, size_t);

END_DECLS

#endif /* !INN_UTILITY_H */
