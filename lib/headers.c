/*
**  Routines for headers: manipulation and checks.
*/

#include "portable/system.h"

#include <ctype.h>

#include "inn/libinn.h"


/*
**  Check whether the argument is a valid header field name.
**
**  We currently assume the maximal line length has already been checked.
**  Only ensure the requirements for RFC 3977:
**
**    o  The name [of a header field] consists of one or more printable
**       US-ASCII characters other than colon.
*/
bool
IsValidHeaderName(const char *p)
{
    /* Not NULL and not empty. */
    if (p == NULL || *p == '\0')
        return false;

    for (; *p != '\0'; p++) {
        /* Contains only printable US-ASCII characters other
         * than colon. */
        if (!isgraph((unsigned char) *p) || *p == ':')
            return false;
    }

    return true;
}


/*
**  Check whether the argument is a valid header field body.  It starts
**  after the space following the header field name and its colon.
**  Internationalized header fields encoded in UTF-8 are allowed.
**
**  We currently assume the maximal line length has already been checked.
*/
bool
IsValidHeaderBody(const char *p)
{
    bool emptycontentline = true;

    /* Not NULL and not empty. */
    if (p == NULL || *p == '\0')
        return false;

    if (!is_valid_utf8(p))
        return false;

    for (; *p != '\0'; p++) {
        if (ISWHITE(*p)) {
            /* Skip SP and TAB. */
            continue;
        } else if (*p == '\n' || (*p == '\r' && *++p == '\n')) {
            /* Folding detected.  We expect CRLF or lone LF as some parts
             * of INN code internally remove CR.
             * Check that the line that has just been processed is not
             * "empty" and that the following character marks the beginning
             * of a continuation line. */
            if (emptycontentline || !ISWHITE(p[1])) {
                return false;
            }
            /* A continuation line begins.  This new line should also have
             * at least one printable octet other than SP or TAB, so we
             * re-initialize emptycontentline to true. */
            emptycontentline = true;
            continue;
        } else if (p[-1] == '\r') {
            /* Case of CR not followed by LF (handled at the previous
             * if statement). */
            return false;
        } else {
            /* Current header content line contains a (non-whitespace)
             * character. */
            emptycontentline = false;
            continue;
        }
    }

    return (!emptycontentline);
}


/*
**  Check whether the argument is a valid header field.
**
**  We currently assume the maximal line length has already been checked.
*/
bool
IsValidHeaderField(const char *p)
{
    /* Not NULL, not empty, and does not begin with a colon. */
    if (p == NULL || *p == '\0' || *p == ':')
        return false;

    for (; *p != '\0'; p++) {
        /* Header field names contain only printable US-ASCII characters
         * other than colon.  A colon terminates the header field name. */
        if (!isgraph((unsigned char) *p))
            return false;
        if (*p == ':') {
            p++;
            break;
        }
    }

    /* Empty body or no colon found in header field. */
    if (*p == '\0')
        return false;

    /* Missing space after colon. */
    if (*p != ' ')
        return false;

    p++;
    return IsValidHeaderBody(p);
}


/*
**  Skip any amount of CFWS (comments and folding whitespace), the RFC 5322
**  grammar term for whitespace, CRLF pairs, and possibly nested comments that
**  may contain escaped parens.  We also allow simple newlines since we don't
**  always deal with wire-format messages.  Note that we do not attempt to
**  ensure that CRLF or a newline is followed by whitespace.  Returns the new
**  position of the pointer.
*/
const char *
skip_cfws(const char *p)
{
    int nesting = 0;

    for (; *p != '\0'; p++) {
        switch (*p) {
        case ' ':
        case '\t':
        case '\n':
            break;
        case '\r':
            if (p[1] != '\n' && nesting == 0)
                return p;
            break;
        case '(':
            nesting++;
            break;
        case ')':
            if (nesting == 0)
                return p;
            nesting--;
            break;
        case '\\':
            if (nesting == 0 || p[1] == '\0')
                return p;
            p++;
            break;
        default:
            if (nesting == 0)
                return p;
            break;
        }
    }
    return p;
}


/*
**  Skip any amount of FWS (folding whitespace), the RFC 5322 grammar term
**  for whitespace and CRLF pairs.  We also allow simple newlines since we
**  don't always deal with wire-format messages.  Note that we do not attempt
**  to ensure that CRLF or a newline is followed by whitespace.  Returns the
**  new position of the pointer.
*/
const char *
skip_fws(const char *p)
{
    for (; *p != '\0'; p++) {
        switch (*p) {
        case ' ':
        case '\t':
        case '\n':
            break;
        case '\r':
            if (p[1] != '\n')
                return p;
            break;
        default:
            return p;
        }
    }
    return p;
}


/*
**  Return a newly allocated string with all CFWS removed from the
**  NULL-terminated argument, but preserving a unique space between words.
**  The caller is responsible for freeing it.
*/
char *
spaced_words_without_cfws(const char *p)
{
    char *buff;
    char *buffbegin;
    bool trailingspace = false;

    if (p == NULL)
        return NULL;

    buff = xmalloc(strlen(p) + 1);
    buffbegin = buff;

    while (*p != '\0') {
        p = skip_cfws(p);
        if (*p != '\0') {
            /* Not CFWS. */
            *buff++ = *p;
            trailingspace = false;
            p++;
            /* Another word may begin, keep one space.  The following ones will
             * be skipped by the next call to skip_cfws(). */
            if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'
                || *p == '(') {
                *buff++ = ' ';
                trailingspace = true;
            }
        }
    }

    /* Remove possible trailing space. */
    if (trailingspace)
        buff--;

    /* NULL-terminate the returned string. */
    *buff = '\0';

    return buffbegin;
}
