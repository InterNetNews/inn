/*  $Id$
**
**  Here be some useful macros.
*/

#ifndef MACROS_H
#define MACROS_H 1

#include "config.h"

/* <ctype.h> usually includes \n, which is not what we want. */
#define ISWHITE(c)              ((c) == ' ' || (c) == '\t')

/* Use a read or recv call to read a descriptor. */
#ifdef HAVE_UNIX_DOMAIN_SOCKETS
# define RECVorREAD(fd, p, s)   recv((fd), (p), (s), 0)
#else
# define RECVorREAD(fd, p, s)   read((fd), (p), (s))
#endif

#endif /* !MACROS_H */
