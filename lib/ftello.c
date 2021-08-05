/*
**  Replacement for a missing ftello.
**
**  ftello is a version of ftell that returns an off_t instead of a long.
**  For large file support (and because it's a more logical interface), INN
**  uses ftello unconditionally; if ftello isn't provided by a platform but
**  fpos_t is compatible with off_t (as in BSDI), define it in terms of
**  fgetpos.  Otherwise, just call ftell (which won't work for files over
**  2GB).
*/

#include "config.h"
#include "clibrary.h"

#if HAVE_LARGE_FPOS_T

off_t
ftello(FILE *stream)
{
    fpos_t fpos;

    if (fgetpos(stream, &fpos) < 0) {
        return -1;
    } else {
        return (off_t) fpos;
    }
}

#else /* !HAVE_LARGE_FPOS_T */

off_t
ftello(FILE *stream)
{
    return (off_t) ftell(stream);
}

#endif /* !HAVE_LARGE_FPOS_T */
