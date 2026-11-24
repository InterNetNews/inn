/*
**  File names for copies of posts saved by reader tracking.
*/

#include "portable/system.h"

#include "nnrpd.h"

#define TRACKPOST_PREFIX        "track."
#define TRACKPOST_PREFIX_LENGTH (sizeof(TRACKPOST_PREFIX) - 1)


/*
**  Return an allocated file name for a tracked post.  Keep the historical,
**  readable name when it is safe.  Message-IDs containing a path separator
**  or too long for a file-name component use INN's canonical Message-ID hash.
**  The history database uses the same hash to identify articles.
*/
char *
TrackPostFilename(const char *message_id)
{
    const char *name = message_id;

    if (strchr(message_id, '/') != NULL
        || strlen(message_id) > (size_t) NAME_MAX - TRACKPOST_PREFIX_LENGTH)
        name = HashToText(HashMessageID(message_id));
    return concat(TRACKPOST_PREFIX, name, (char *) 0);
}
