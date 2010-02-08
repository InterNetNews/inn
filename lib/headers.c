/*  $Id$
**
**  Routines for headers:  manipulation and checks.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>

#include "inn/libinn.h"


/*
**  We currently only check the requirements for RFC 3977:
**
**    o  The name [of a header] consists of one or more printable
**       US-ASCII characters other than colon.
*/
bool
IsValidHeaderName(const char *string)
{
    const unsigned char *p;

    /* Not NULL. */
    if (string == NULL)
        return false;

    p = (const unsigned char *) string;
   
    /* Not empty. */
    if (*p == '\0')
        return false;

    for (; *p != '\0'; p++) {
        /* Contains only printable US-ASCII characters other
         * than colon. */
        if (!CTYPE(isgraph, *p) || *p == ':')
            return false;
    }

    return true;
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
**  for whitespace and CRLF pairs.  We also allow simple newlines since we don't
**  always deal with wire-format messages.  Note that we do not attempt to
**  ensure that CRLF or a newline is followed by whitespace.  Returns the new
**  position of the pointer.
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

