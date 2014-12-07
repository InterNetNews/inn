/*  $Id$
**
**  Replacement for a missing seteuid.
**
**  Written by Russ Allbery <eagle@eyrie.org>
**  This work is hereby placed in the public domain by its author.
**
**  Some systems don't have seteuid but do have setreuid.  setreuid with
**  -1 given for the real UID is equivalent to seteuid on systems with
**  POSIX saved UIDs.  On systems without POSIX saved UIDs, we'd lose our
**  ability to regain privileges if we just set the effective UID, so
**  instead fake a saved UID by setting the real UID to the current
**  effective UID, using the real UID as the saved UID.
**
**  Note that swapping UIDs doesn't work on AIX, but AIX has saved UIDs.
**  Note also that systems without setreuid lose, and that we assume that
**  any system with seteuid has saved UIDs.
*/

#include "config.h"
#include "clibrary.h"

int
seteuid(uid_t euid)
{
    int ruid;

#ifdef _POSIX_SAVED_IDS
    ruid = -1;
#else
    ruid = geteuid();
#endif
    return setreuid(ruid, euid);
}
