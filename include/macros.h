/*  $Id$
**
**  Here be some useful macros.
*/

#ifndef MACROS_H
#define MACROS_H 1

#include "config.h"

/* <ctype.h> usually includes \n, which is not what we want. */
#define ISWHITE(c)              ((c) == ' ' || (c) == '\t')

#endif /* !MACROS_H */
