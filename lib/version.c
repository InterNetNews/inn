/*  $Id$
**
**  INN compile-time version information.
*/

#include "config.h"
#include "inn/version.h"

const int inn_version[3] = {
    INN_VERSION_MAJOR, INN_VERSION_MINOR, INN_VERSION_PATCH
};
const char inn_version_extra[]  = INN_VERSION_EXTRA;
const char inn_version_string[] = INN_VERSION_STRING;
