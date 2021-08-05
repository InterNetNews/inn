/*
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
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2000 Russ Allbery <eagle@eyrie.org>
 * Copyright 2008-2009, 2011
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#include "config.h"
#if HAVE_UNISTD_H
#    include <unistd.h>
#endif

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
