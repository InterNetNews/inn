/*
 * $Id$
 */
#include "inn/version.h"

const int inn_version[3] = { INN_VERSION_MAJOR,
                             INN_VERSION_MINOR,
                             INN_VERSION_PATCH  };
const char inn_version_extra[]  = INN_VERSION_EXTRA;
const char inn_version_string[] = INN_VERSION_STRING;

/* This function is deprecated, and is provided solely for backwards
   compatibility for non-INN users of libinn.  Callers should reference
   inn_version_string directly instead. */
const char *
INNVersion(void)
{
    return inn_version_string;
}

