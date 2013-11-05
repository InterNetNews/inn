/* $Id$
 *
 * Replacement for a missing seteuid.
 *
 * Some systems don't have seteuid but do have setreuid.  setreuid with -1
 * given for the real UID is equivalent to seteuid on systems with POSIX saved
 * UIDs.  On systems without POSIX saved UIDs, we'd lose our ability to regain
 * privileges if we just set the effective UID, so instead fake a saved UID by
 * setting the real UID to the current effective UID, using the real UID as
 * the saved UID.
 *
 * Note that swapping UIDs doesn't work on AIX, but AIX has saved UIDs.  Note
 * also that systems without setreuid lose, and that we assume that any system
 * with seteuid has saved UIDs.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 *
 * The authors hereby relinquish any claim to any copyright that they may have
 * in this work, whether granted under contract or by operation of law or
 * international treaty, and hereby commit to the public, at large, that they
 * shall not, at any time in the future, seek to enforce any copyright in this
 * work against any person or entity, or prevent any person or entity from
 * copying, publishing, distributing or creating derivative works of this
 * work.
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
