/*
**  Overview data processing.
**
**  Here be routines for creating and checking the overview data, the
**  tab-separated list of overview fields.
*/

#include "portable/system.h"

#include <ctype.h>

#include "inn/buffer.h"
#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/overview.h"
#include "inn/vector.h"
#include "inn/wire.h"
#include "ovinterface.h"


/* The standard overview fields.  The order of these fields is important. */
static const char *const fields[] = {
    "Subject", "From", "Date", "Message-ID", "References", "Bytes", "Lines"};


/*
**  Return a vector of the standard overview fields.  Note there is no
**  way to free up the resulting data structure.
*/
const struct cvector *
overview_fields(void)
{
    static struct cvector *list = NULL;

    if (list == NULL) {
        unsigned int field;

        list = cvector_new();
        cvector_resize(list, ARRAY_SIZE(fields));

        for (field = 0; field < ARRAY_SIZE(fields); ++field) {
            cvector_add(list, fields[field]);
        }
    }
    return list;
}

/*
**  Return a vector of the additional fields over the standard ones.
**  The order of these fields is important.
**
**  The Xref header field is mandatory for INN and we make it the first
**  extra field after the seven overview fields defined in RFC 3977.
**
**  Caller is responsible for freeing the vector.
*/
struct vector *
overview_extra_fields(bool hidden)
{
    struct vector *list = NULL;
    unsigned int i;

    list = vector_new();

    if (hidden) {
        vector_resize(list, innconf->extraoverviewadvertised->count
                                + innconf->extraoverviewhidden->count + 1);
    } else {
        vector_resize(list, innconf->extraoverviewadvertised->count + 1);
    }

    vector_add(list, "Xref");

    if (innconf->extraoverviewadvertised->strings != NULL) {
        for (i = 0; i < innconf->extraoverviewadvertised->count; i++) {
            if (innconf->extraoverviewadvertised->strings[i] != NULL) {
                vector_add(list, innconf->extraoverviewadvertised->strings[i]);
            }
        }
    }

    if (hidden) {
        if (innconf->extraoverviewhidden->strings != NULL) {
            for (i = 0; i < innconf->extraoverviewhidden->count; i++) {
                if (innconf->extraoverviewhidden->strings[i] != NULL) {
                    vector_add(list, innconf->extraoverviewhidden->strings[i]);
                }
            }
        }
    }

    return list;
}


/*
**  Given an article, its length, the name of a header field, and a buffer to
**  append the data to, append header data for that header field to the
**  overview data that's being constructed.  Doesn't append any data if the
**  header field isn't found.
*/
static void
build_header(const char *article, size_t length, const char *header,
             struct buffer *overview)
{
    ptrdiff_t size;
    size_t offset;
    const char *data, *end, *p;

    data = wire_findheader(article, length, header, false);
    if (data == NULL)
        return;
    end = wire_endheader(data, article + length - 1);
    if (end == NULL)
        return;

    /* Someone managed to break their server so that they were appending
       multiple Xref header fields, and INN had a bug where it wouldn't notice
       this and reject the article.  Just in case, see if there are multiple
       Xref header fields and use the last one. */
    if (strcasecmp(header, "xref") == 0) {
        const char *next = end + 1;

        while (next != NULL) {
            next = wire_findheader(next, length - (next - article), header,
                                   false);
            if (next != NULL) {
                data = next;
                end = wire_endheader(data, article + length - 1);
                if (end == NULL)
                    return;
            }
        }
    }

    size = end - data + 1;
    offset = overview->used + overview->left;
    buffer_resize(overview, offset + size);

    for (p = data; p <= end; p++) {
        if (*p == '\r' && p[1] == '\n') {
            p++;
            continue;
        }
        if (*p == '\0' || *p == '\t' || *p == '\n' || *p == '\r')
            overview->data[offset++] = ' ';
        else
            overview->data[offset++] = *p;
        overview->left++;
    }
}


/*
**  Given an article number, an article, and a vector of additional header
**  fields, generate overview data into the provided buffer.  If the buffer
**  parameter is NULL, a new buffer is allocated.  The article should be in
**  wire format.
**  Returns the buffer containing the overview data.
*/
struct buffer *
overview_build(ARTNUM number, const char *article, size_t length,
               const struct vector *extra, struct buffer *overview)
{
    unsigned int field;
    char buffer[32];

    snprintf(buffer, sizeof(buffer), "%lu", number);
    if (overview == NULL)
        overview = buffer_new();
    buffer_set(overview, buffer, strlen(buffer));
    for (field = 0; field < ARRAY_SIZE(fields); field++) {
        buffer_append(overview, "\t", 1);
        if (field == 5) {
            snprintf(buffer, sizeof(buffer), "%lu", (unsigned long) length);
            buffer_append(overview, buffer, strlen(buffer));
        } else
            build_header(article, length, fields[field], overview);
    }
    if (extra != NULL) {
        for (field = 0; field < extra->count; field++) {
            buffer_append(overview, "\t", 1);
            buffer_append(overview, extra->strings[field],
                          strlen(extra->strings[field]));
            buffer_append(overview, ": ", 2);
            build_header(article, length, extra->strings[field], overview);
        }
    }
    buffer_append(overview, "\r\n", 2);
    return overview;
}


/*
**  Check whether a given string is a valid number.
*/
static bool
valid_number(const char *string)
{
    const char *p;

    for (p = string; *p != '\0'; p++)
        if (!isdigit((unsigned char) *p))
            return false;
    return true;
}


/*
**  Check whether a given string is a valid overview string (doesn't contain
**  CR or LF, and if the second argument is true must be preceded by a header
**  field name, colon, and space).  Allow CRLF at the end of the data, but
**  don't require it.
*/
static bool
valid_overview_string(const char *string, bool full)
{
    const unsigned char *p;

    /* RFC 5322 says that header fields must consist of printable ASCII
       characters (characters between 33 and 126, inclusive) excluding colon.
       We also allow high-bit characters, just in case, but not DEL. */
    p = (const unsigned char *) string;
    if (full) {
        if (*p == '\0' || (*p == '\r' && p[1] == '\n' && p[2] == '\0'))
            return true;
        for (; *p != '\0' && *p != ':'; p++)
            if (!isgraph((unsigned char) *p))
                return false;
        if (*p != ':')
            return false;
        p++;
        if (*p != ' ')
            return false;
    }
    for (p++; *p != '\0'; p++) {
        if (*p == '\r' && p[1] == '\n' && p[2] == '\0')
            break;
        if (*p == '\r' || *p == '\n')
            return false;
    }
    return true;
}


/*
**  Check the given overview data and make sure it's well-formed.  Extension
**  header fields are not checked against LIST OVERVIEW.FMT (having a different
**  set of extension header fields doesn't make the data invalid), but the
**  presence of the standard header fields is checked.  Also checked is whether
**  the article number in the data matches the passed article number.
**  Returns true if the data is okay, false otherwise.
*/
bool
overview_check(const char *data, size_t length, ARTNUM article)
{
    char *copy;
    struct cvector *overview;
    ARTNUM overnum;
    size_t i;

    copy = xstrndup(data, length);
    overview = cvector_split(copy, '\t', NULL);

    /* The actual checks.  We don't verify all of the data, since that data
       may be malformed in the article, but we do check to be sure that the
       fields that should be numbers are numbers.  That should catch most
       positional errors.  We can't check Lines yet since right now INN is
       still accepting the value from the post verbatim. */
    if (overview->count < 8)
        goto fail;
    if (!valid_number(overview->strings[0]))
        goto fail;
    overnum = strtoul(overview->strings[0], NULL, 10);
    if (overnum != article)
        goto fail;
    if (!valid_number(overview->strings[6]))
        goto fail;
    for (i = 1; i < 6; i++)
        if (!valid_overview_string(overview->strings[i], false))
            goto fail;
    for (i = 8; i < overview->count; i++)
        if (!valid_overview_string(overview->strings[i], true))
            goto fail;
    cvector_free(overview);
    free(copy);
    return true;

fail:
    cvector_free(overview);
    free(copy);
    return false;
}


/*
**  Given an overview header field, return the offset of the field within the
**  overview data, or -1 if the field is not present in the overview schema
**  for this installation.
*/
int
overview_index(const char *field, const struct vector *extra)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(fields); i++)
        if (strcasecmp(field, fields[i]) == 0)
            return i;
    for (i = 0; i < extra->count; i++)
        if (strcasecmp(field, extra->strings[i]) == 0)
            return i + ARRAY_SIZE(fields);
    return -1;
}


/*
**  Given an overview header line, split out a vector pointing at each
**  of the components (within line), returning a pointer to the
**  vector.  If the vector initially passed in is NULL, a new vector is
**  created, otherwise the existing one is filled in.
**
**  A member `n' of the vector is of length (vector->strings[n+1] -
**  vector->strings[n] - 1).  Note that the last member of the vector
**  will always point beyond (line + length).
*/
struct cvector *
overview_split(const char *line, size_t length, ARTNUM *number,
               struct cvector *vector)
{
    const char *p = NULL;

    if (vector == NULL) {
        vector = cvector_new();
    } else {
        cvector_clear(vector);
    }
    while (line != NULL) {
        /* The first field is the article number. */
        if (p == NULL) {
            if (number != NULL) {
                *number = atoi(line);
            }
        } else {
            cvector_add(vector, line);
        }
        p = memchr(line, '\t', length);
        if (p != NULL) {
            /* Skip over the tab. */
            ++p;
            /* And calculate the remaining length. */
            length -= (p - line);
        } else {
            /* Add in a pointer to beyond the end of the final
               component, so you can always calculate the length;
               overview lines are always terminated with \r\n, so the
               -1 ends up chopping those off. */
            cvector_add(vector, line + length - 1);
        }
        line = p;
    }
    return vector;
}

/*
**  Given an overview vector (from overview_split), return a copy of
**  the member which the caller is interested in (and must free).
**  The name and order of standard overview fields are fixed so we
**  can request them by their index.
*/
char *
overview_get_standard_header(const struct cvector *vector,
                             unsigned int element)
{
    char *field = NULL;
    size_t len;
    const char *p;
    size_t max = ARRAY_SIZE(fields) - 1;

    /* vector ends with an additional pointer beyond the end of the final
       component.  It therefore has a count of elements increased by 1. */
    if ((element + 1) >= vector->count || element > max) {
        warn("request for invalid standard overview field %u", element);
        return NULL;
    }

    p = vector->strings[element];
    len = vector->strings[element + 1] - vector->strings[element] - 1;

    field = xstrndup(p, len);
    return field;
}

/*
**  Given an overview vector (from overview_split), return a copy of
**  the member which the caller is interested in (and must free).
**  The order of extra overview fields may vary so we walk all the
**  extra header fields to find the requested field.
*/
char *
overview_get_extra_header(const struct cvector *vector, const char *header)
{
    char *field = NULL;
    size_t i, len;
    const char *p;
    size_t headerlen = strlen(header);

    /* vector ends with an additional pointer beyond the end of the final
       component.  It therefore has a count of elements increased by 1. */
    for (i = ARRAY_SIZE(fields); i < vector->count - 1; i++) {
        if (strncasecmp(header, vector->strings[i], headerlen) == 0) {
            p = vector->strings[i] + headerlen;
            /* Check for ": " after the header field name. */
            if ((*p++ == ':') && (*p++ == ' ')) {
                len = vector->strings[i + 1] - p - 1;

                field = xstrndup(p, len);
                return field;
            }
        }
    }

    /* The required header field was not found in the extra overview fields. */
    return NULL;
}
