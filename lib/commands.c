/*  $Id$
**
**  Routines for NNTP commands:  manipulation and checks.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>

#include "inn/libinn.h"


/*
**  Return true if the given string is a keyword according to RFC 3977:
**
**      A "keyword" MUST consist only of US-ASCII letters, digits, and the
**      characters dot (".") and dash ("-") and MUST begin with a letter.
**      Keywords MUST be at least three characters in length.
*/
bool
IsValidKeyword(const char *string)
{
    int len = 0;

    /* Not NULL. */
    if (string == NULL)
        return false;

    /* Begins with a letter. */
    if (!isalpha((unsigned char) string[0]))
        return false;

    for (; *string != '\0'; string++) {
        if (isalnum((unsigned char) *string) || *string == '.' || *string == '-')
            len++;
        else
            return false;
    }

    /* At least 3 octets in length. */
    if (len > 2)
        return true;
    else
        return false;
}
