/*  $Id$
**
**  Portable defines used by other INN header files.
**
**  In order to make the libraries built by INN usable by other software,
**  INN needs to install several header files.  Installing autoconf-
**  generated header files, however, is a bad idea, since the defines will
**  conflict with other software that uses autoconf.
**
**  This header contains common definitions, such as internal typedefs and
**  macros, common to INN's header files but not based on autoconf probes.
**  As such, it's limited in what it can do; if compiling software against
**  INN's header files on a system not supporting basic ANSI C features
**  (such as const) or standard types (like size_t), the software may need
**  to duplicate the tests that INN itself performs, generate a config.h,
**  and make sure that config.h is included before any INN header files.
*/

#ifndef INN_DEFINES_H
#define INN_DEFINES_H 1

#include <inn/system.h>
#include "inn/macros.h"
#include "portable/stdbool.h"

// TODO:  Remove this file (defines.h), now that it has been split
//        into other headers.

#endif /* !INN_DEFINES_H */
