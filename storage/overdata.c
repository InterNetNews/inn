/*  $Id$
**
**  Overview data processing.
**
**  Here be routines for creating and checking the overview data, the
**  tab-separated list of overview fields.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>

#include "inn/vector.h"
#include "libinn.h"
#include "ovinterface.h"


/*
**  Check whether a given string is a valid number.
*/
static bool
valid_number(const char *string)
{
    const char *p;

    for (p = string; *p != '\0'; p++)
        if (!CTYPE(isdigit, *p))
            return false;
    return true;
}


/*
**  Check whether a given string is a valid overview string (doesn't contain
**  CR or LF, and if the second argument is true must be preceeded by a header
**  name, colon, and space).  Allow CRLF at the end of the data, but don't
**  require it.
*/
static bool
valid_overview_string(const char *string, bool full)
{
    const unsigned char *p;

    /* RFC 2822 says that header fields must consist of printable ASCII
       characters (characters between 33 and 126, inclusive) excluding colon.
       We also allow high-bit characters, just in case, but not DEL. */
    p = (const unsigned char *) string;
    if (full) {
        for (; *p != '\0' && *p != ':'; p++)
            if (*p < 33 || *p == 127)
                return false;
        if (*p != ':')
            return false;
        p++;
        if (*p != ' ')
            return false;
    }
    for (p++; *p != '\0'; p++) {
        if (*p == '\015' && p[1] == '\012' && p[2] == '\0')
            break;
        if (*p == '\015' || *p == '\012')
            return false;
    }
    return true;
}


/*
**  Check the given overview data and make sure it's well-formed.  Extension
**  headers are not checked against overview.fmt (having a different set of
**  extension headers doesn't make the data invalid), but the presence of the
**  standard fields is checked.  Also checked is whether the article number in
**  the data matches the passed article number.  Returns true if the data is
**  okay, false otherwise.
*/
bool
overview_check(const char *data, size_t length, ARTNUM article)
{
    char *copy;
    struct cvector *fields;
    ARTNUM overnum;
    size_t i;

    copy = xstrndup(data, length);
    fields = cvector_split(copy, '\t', NULL);

    /* The actual checks.  We don't verify all of the data, since that data
       may be malformed in the article, but we do check to be sure that the
       fields that should be numbers are numbers.  That should catch most
       positional errors.  We can't check Lines yet since right now INN is
       still accepting the value from the post verbatim. */
    if (fields->count < 8)
        goto fail;
    if (!valid_number(fields->strings[0]))
        goto fail;
    overnum = strtoul(fields->strings[0], NULL, 10);
    if (overnum != article)
        goto fail;
    if (!valid_number(fields->strings[6]))
        goto fail;
    for (i = 1; i < 6; i++)
        if (!valid_overview_string(fields->strings[i], false))
            goto fail;
    for (i = 8; i < fields->count; i++)
        if (!valid_overview_string(fields->strings[i], true))
            goto fail;
    cvector_free(fields);
    free(copy);
    return true;

 fail:
    cvector_free(fields);
    free(copy);
    return false;
}
