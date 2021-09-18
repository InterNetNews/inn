/*
**  Routines for numbers:  manipulation and checks.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>

#include "inn/libinn.h"


/*
**  Check if the argument is a valid article number according to RFC 3977,
**  that is to say it contains from 1 to 16 digits and must lie in the range
**  0-2,147,483,647.
*/
bool
IsValidArticleNumber(const char *string)
{
    int len = 0, digit;
    const unsigned char *p;
    unsigned long v = 0;

    /* Not NULL. */
    if (string == NULL)
        return false;

    p = (const unsigned char *) string;
   
    for (; *p != '\0'; p++) {
        len++;
        if (!isdigit((unsigned char) *p))
            return false;
        digit = *p - '0';
        if(v > (0x7FFFFFFFul - digit) / 10)
            return false;
        v = 10 * v + digit;
    }

    if (len > 0 && len < 17)
        return true;
    else
        return false;
}


/*
**  Return true if the provided string is a valid range, that is to say:
** 
**    - An article number.
**    - An article number followed by a dash to indicate all following.
**    - An article number followed by a dash followed by another article
**      number.
**
**  In addition to RFC 3977, we also accept:
**    - A dash followed by an article number to indicate all previous.
**    - A dash for everything.
*/
bool
IsValidRange(char *string)
{
    char *p;
    bool valid;

    /* Not NULL. */
    if (string == NULL)
        return false;

    /* Just a dash. */
    if (strcmp(string, "-") == 0)
        return true;

    p = string;

    /* Begins with a dash.  There must be a number after. */
    if (*string == '-') {
        p++;
        return IsValidArticleNumber(p);
    }

    /* Got just a single number? */
    if ((p = strchr(string, '-')) == NULL)
        return IsValidArticleNumber(string);

    /* "-" becomes "\0" and we parse the low water mark. */
    *p++ = '\0';
    if (*p == '\0') {
        /* Ends with a dash. */
        valid = IsValidArticleNumber(string);
    } else {
        valid = (IsValidArticleNumber(string) && IsValidArticleNumber(p));
    }

    p--;
    *p = '-';
    return valid;
}
