/*  $Id$
**
**  Here be some useful macros.
*/

#ifndef MACROS_H
#define MACROS_H 1

#include "config.h"

/* Wrappers around str[n]cmp.  Don't add the ((a) == (b)) test here; it's
   already been done in places where it's time-critical. */
#define EQ(a, b)		(strcmp((a), (b)) == 0)
#define EQn(a, b, n)		(strncmp((a), (b), (n)) == 0)
#define caseEQ(a, b)		(strcasecmp((a), (b)) == 0)
#define caseEQn(a, b, n)	(strncasecmp((a), (b), (n)) == 0)

/* <ctype.h> usually includes \n, which is not what we want. */
#define ISWHITE(c)              ((c) == ' ' || (c) == '\t')

/* Get the number of elements in a fixed-size array, or a pointer just past
   the end of it. */
#define SIZEOF(array)           (sizeof array / sizeof array[0])
#define ENDOF(array)            (&array[SIZEOF(array)])

/* Get the length of a string constant. */
#define STRLEN(string)          (sizeof string - 1)

/* Turn a TIMEINFO into a floating point number. */
#define TIMEINFOasDOUBLE(t)	\
    ((double)(t).time + ((double)(t).usec) / 1000000.0)

/* Use a read or recv call to read a descriptor. */
#ifdef HAVE_UNIX_DOMAIN_SOCKETS
# define RECVorREAD(fd, p, s)   recv((fd), (p), (s), 0)
#else
# define RECVorREAD(fd, p, s)   read((fd), (p), (s))
#endif

#endif /* !MACROS_H */
