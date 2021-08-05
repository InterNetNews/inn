/*
**  Replacement for a missing fseeko.
**
**  fseeko is a version of fseek that takes an off_t instead of a long.  For
**  large file support (and because it's a more logical interface), INN uses
**  fseeko unconditionally; if fseeko isn't provided by a platform but
**  fpos_t is compatible with off_t (as in BSDI), define it in terms of
**  fsetpos.  Otherwise, just call fseek (which won't work for files over
**  2GB).
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>

#if HAVE_LARGE_FPOS_T

int
fseeko(FILE *stream, off_t pos, int whence)
{
    fpos_t fpos;

    switch (whence) {
    case SEEK_SET:
        fpos = pos;
        return fsetpos(stream, &fpos);

    case SEEK_END:
        if (fseek(stream, 0, SEEK_END) < 0)
            return -1;
        if (pos == 0)
            return 0;
        /* Fall through. */

    case SEEK_CUR:
        if (fgetpos(stream, &fpos) < 0)
            return -1;
        fpos += pos;
        return fsetpos(stream, &fpos);

    default:
        errno = EINVAL;
        return -1;
    }
}

#else /* !HAVE_LARGE_FPOS_T */

int
fseeko(FILE *stream, off_t pos, int whence)
{
    return fseek(stream, (long) pos, whence);
}

#endif /* !HAVE_LARGE_FPOS_T */
