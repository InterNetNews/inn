/*  $Id$
**
**  Replacement symlink that always fails for systems without it.
**
**  This is basically a hack to make code in INN simpler and more
**  straightforward.  On systems where symlink isn't available, we link in
**  this replacement that just always fails after setting errno to ENOSYS.
*/

#include "config.h"
#include <errno.h>

int
symlink(const char *oldpath UNUSED, const char *newpath UNUSED)
{
    errno = ENOSYS;
    return -1;
}
