/*  $Id$
**
**  Wire format article utilities.
**
**  Originally written by Alex Kiernan (alex.kiernan@thus.net)
**
**  These routines manipulate wire format articles; in particular, they should
**  be safe in the presence of embedded NULs.  They assume wire format
**  conventions (\r\n as a line ending, in particular) and will not work with
**  articles in native format (with the exception of wire_from_native, of
**  course).
**
**  The functions in this file take const char * pointers and return char *
**  pointers so that they can work on both const char * and char * article
**  bodies without changing the const sense.  This unfortunately means that
**  the routines in this file will produce warnings about const being cast
**  away.  To avoid those, one would need to duplicate all the code in this
**  file or use C++.
*/

#include "config.h"
#include "clibrary.h"
#include <assert.h>

#include "inn/wire.h"
#include "inn/libinn.h"

/*
**  Given a pointer to the start of an article, locate the first octet of the
**  body (which may be the octet beyond the end of the buffer if your article
**  is bodiless).
*/
char *
wire_findbody(const char *article, size_t length)
{
    char *p;
    const char *end;

    /* Handle the degenerate case of an article with no headers. */
    if (length > 5 && article[0] == '\r' && article[1] == '\n')
        return (char *) article + 2;

    /* Jump from \r to \r and give up if we're too close to the end. */
    end = article + length;
    for (p = (char *) article; (p + 4) <= end; ++p) {
        p = memchr(p, '\r', end - p - 3);
        if (p == NULL)
            break;
        if (memcmp(p, "\r\n\r\n", 4) == 0) {
            p += 4;
            return p;
        }
    }
    return NULL;
}


/*
**  Given a pointer into an article and a pointer to the last octet of the
**  article, find the next line ending and return a pointer to the first
**  character after that line ending.  If no line ending is found in the
**  article or if it is at the end of the article, return NULL.
*/
char *
wire_nextline(const char *article, const char *end)
{
    char *p;

    for (p = (char *) article; (p + 2) <= end; ++p) {
        p = memchr(p, '\r', end - p - 2);
        if (p == NULL)
            break;
        if (p[1] == '\n') {
            p += 2;
            return p;
        }
    }
    return NULL;
}


/*
**  Returns true if line is the beginning of a valid header for header, also
**  taking the length of the header name as a third argument.  Assumes that
**  there is at least length + 2 bytes of data at line, and that the header
**  name doesn't contain nul.
*/
static bool
isheader(const char *line, const char *header, size_t length)
{
    if (line[length] != ':' || !ISWHITE(line[length + 1]))
        return false;
    return strncasecmp(line, header, length) == 0;
}


/*
**  Skip over folding whitespace, as defined by RFC 2822.  Takes a pointer to
**  where to start skipping and a pointer to the end of the data, and will not
**  return a pointer past the end pointer.  If skipping folding whitespace
**  takes us past the end of data, return NULL.
*/
static char *
skip_fws(char *text, const char *end)
{
    char *p;

    for (p = text; p <= end; p++) {
        if (p < end + 1 && p[0] == '\r' && p[1] == '\n' && ISWHITE(p[2]))
            p += 2;
        if (!ISWHITE(*p))
            return p;
    }
    return NULL;
}


/*
**  Given a pointer to the start of the article, the article length, and the
**  header to look for, find the first occurance of that header in the
**  article.  Skip over headers with no content, but allow for headers that
**  are folded before the first text in the header.  If no matching headers
**  with content other than spaces and tabs are found, return NULL.
*/
char *
wire_findheader(const char *article, size_t length, const char *header)
{
    char *p;
    const char *end;
    ptrdiff_t headerlen;

    headerlen = strlen(header);
    end = article + length - 1;

    /* There has to be enough space left in the article for at least the
       header, the colon, whitespace, and one non-whitespace character, hence
       3, minus 1 since the character pointed to by end is part of the
       article. */
    p = (char *) article;
    while (p != NULL && end - p > headerlen + 2) {
        if (p[0] == '\r' && p[1] == '\n')
            return NULL;
        else if (isheader(p, header, headerlen)) {
            p = skip_fws(p + headerlen + 2, end);
            if (p == NULL)
                return NULL;
            if (p >= end || p[0] != '\r' || p[1] != '\n')
                return p;
        }
        p = wire_nextline(p, end);
    }
    return NULL;
}


/*
**  Given a pointer to a header and a pointer to the last octet of the
**  article, find the end of the header (a pointer to the final \n of the
**  header value).  If the header contents don't end in \r\n, return NULL.
*/
char *
wire_endheader(const char *header, const char *end)
{
    char *p;

    p = wire_nextline(header, end);
    while (p != NULL) {
        if (!ISWHITE(*p))
            return p - 1;
        p = wire_nextline(p, end);
    }
    if (end - header >= 1 && *end == '\n' && *(end - 1) == '\r')
        return (char *) end;
    return NULL;
}


/*
**  Given an article and length in non-wire format, return a malloced region
**  containing the article in wire format.  Set *newlen to the length of the
**  new article.  The caller is responsible for freeing the allocated memory.
*/ 
char *
wire_from_native(const char *article, size_t len, size_t *newlen)
{
    size_t bytes;
    char *newart;
    const char *p;
    char *dest;
    bool at_start = true;

    /* First go thru article and count number of bytes we need.  Add a CR for
       every LF and an extra character for any period at the beginning of a
       line for dot-stuffing.  Add 3 characters at the end for .\r\n. */
    for (bytes = 0, p = article; p < article + len; p++) {
        if (at_start && *p == '.')
            bytes++;
        bytes++;
        at_start = (*p == '\n');
        if (at_start)
            bytes++;
    }
    bytes += 3;

    /* Now copy the article, making the required changes. */
    newart = xmalloc(bytes + 1);
    *newlen = bytes;
    at_start = true;
    for (p = article, dest = newart; p < article + len; p++) {
        if (*p == '\n') {
            *dest++ = '\r';
            *dest++ = '\n';
            at_start = true;
        } else {
            if (at_start && *p == '.')
                *dest++ = '.';
            *dest++ = *p;
            at_start = false;
        }
    }
    *dest++ = '.';
    *dest++ = '\r';
    *dest++ = '\n';
    *dest = '\0';
    return newart;
}


/*
**  Given an article and length in wire format, return a malloced region
**  containing the article in native format.  Set *newlen to the length of the
**  new article.  The caller is responsible for freeing the allocated memory.
*/
char *
wire_to_native(const char *article, size_t len, size_t *newlen)
{
    size_t bytes;
    char *newart;
    const char *p, *end;
    char *dest;
    bool at_start = true;

    /* If the article is shorter than three bytes, it's definitely not in wire
       format.  Just return a copy of it. */
    if (len < 3) {
        *newlen = len;
        return xstrndup(article, len);
    }
    end = article + len - 3;

    /* First go thru article and count number of bytes we need.  Once we reach
       .\r\n, we're done.  We'll remove one . from .. at the start of a line
       and change CRLF to just LF. */
    for (bytes = 0, p = article; p < article + len; ) {
        if (p == end && p[0] == '.' && p[1] == '\r' && p[2] == '\n')
            break;
        if (at_start && p < article + len - 1 && p[0] == '.' && p[1] == '.') {
            bytes++;
            p += 2;
            at_start = false;
        } else if (p < article + len - 1 && p[0] == '\r' && p[1] == '\n') {
            bytes++;
            p += 2;
            at_start = true;
        } else {
            bytes++;
            p++;
            at_start = false;
        }
    }

    /* Now, create the new space and copy the article over. */
    newart = xmalloc(bytes + 1);
    *newlen = bytes;
    at_start = true;
    for (p = article, dest = newart; p < article + len; ) {
        if (p == end && p[0] == '.' && p[1] == '\r' && p[2] == '\n')
            break;
        if (at_start && p < article + len - 1 && p[0] == '.' && p[1] == '.') {
            *dest++ = '.';
            p += 2;
            at_start = false;
        } else if (p < article + len - 1 && p[0] == '\r' && p[1] == '\n') {
            *dest++ = '\n';
            p += 2;
            at_start = true;
        } else {
            *dest++ = *p++;
            at_start = false;
        }
    }
    *dest = '\0';
    return newart;
}
