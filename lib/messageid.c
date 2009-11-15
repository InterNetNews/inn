/*  $Id$
**
**  Routines for message-IDs:  generation and checks.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <time.h>

#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/nntp.h"

/*  Scale time back a bit, for shorter message-IDs. */
#define OFFSET	673416000L

/*
**  Flag array, indexed by character.  Character classes for message-IDs.
*/
static char             midcclass[256];
#define CC_MSGID_ATOM   01
#define CC_MSGID_NORM   02
#define midnormchar(c)  ((midcclass[(unsigned char)(c)] & CC_MSGID_NORM) != 0)
#define midatomchar(c)  ((midcclass[(unsigned char)(c)] & CC_MSGID_ATOM) != 0)

char *
GenerateMessageID(char *domain)
{
    static char		buff[SMBUF];
    static int		count;
    char		*p;
    char		sec32[10];
    char		pid32[10];
    time_t		now;

    now = time(NULL);
    Radix32(now - OFFSET, sec32);
    Radix32(getpid(), pid32);
    if ((domain != NULL && innconf->domain == NULL) ||
	(domain != NULL && innconf->domain != NULL
         && strcmp(domain, innconf->domain) != 0)) {
	p = domain;
    } else {
	if ((p = GetFQDN(domain)) == NULL)
	    return NULL;
    }
    snprintf(buff, sizeof(buff), "<%s$%s$%d@%s>", sec32, pid32, ++count, p);
    return buff;
}


/*
**  Initialize the character class tables.
*/
void
InitializeMessageIDcclass(void)
{
    const unsigned char *p;
    unsigned int i;

    /* Set up the character class tables.  These are written a
     * little strangely to work around a GCC2.0 bug. */
    memset(midcclass, 0, sizeof(midcclass));
        
    p = (const unsigned char*) "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    while ((i = *p++) != 0) {
        midcclass[i] = CC_MSGID_ATOM | CC_MSGID_NORM;
    }
      
    p = (const unsigned char*) "!#$%&'*+-/=?^_`{|}~";
    while ((i = *p++) != 0) {
        midcclass[i] = CC_MSGID_ATOM | CC_MSGID_NORM;
    }

    p = (const unsigned char*) "\"(),.:;<@";
    while ((i = *p++) != 0) {
        midcclass[i] = CC_MSGID_NORM;
    }
}


/*
**  According to RFC 3977:
**
**    o  A message-ID MUST begin with "<", end with ">", and MUST NOT
**       contain the latter except at the end.
**
**    o  A message-ID MUST be between 3 and 250 octets in length.
**
**    o  A message-ID MUST NOT contain octets other than printable US-ASCII
**       characters.
**
**  Besides, we check message-ID format based on RFC 5322 grammar, except that
**  (as per USEFOR, RFC 5536) whitespace, non-printing, and '>' characters
**  are excluded.
**  Based on code by Paul Eggert posted to news.software.b on 22-Nov-90
**  in <#*tyo2'~n@twinsun.com>, with additional e-mail discussion.
**  Thanks, Paul, for the original implementation based upon RFC 1036.
**  Updated to RFC 5536 by Julien Elie.
*/
bool
IsValidMessageID(const char *MessageID)
{
    int c;
    const unsigned char *p;

    /* Check the length of the message-ID. */
    if (MessageID == NULL || strlen(MessageID) > NNTP_MAXLEN_MSGID)
        return false;

    p = (const unsigned char *) MessageID;

    /* Scan local-part:  "< dot-atom-text". */
    if (*p++ != '<')
        return false;
    for (; ; p++) {
        if (midatomchar(*p)) {
            while (midatomchar(*++p))
                continue;
        } else {
            return false;
        }
        if (*p != '.')
            break;
    }
    
    /* Scan domain part:  "@ dot-atom-text|no-fold-literal > \0" */
    if (*p++ != '@')
        return false;
    for ( ; ; p++) {
        if (midatomchar(*p)) {
            while (midatomchar(*++p))
                continue;
        } else {
            /* no-fold-literal only */
            if (p[-1] != '@' || *p++ != '[')
                return false;
            for ( ; ; ) {
                switch (c = *p++) {
                    default:
                        if (midnormchar(c)) {
                            continue;
                        } else {
                            return false;
                        }
                    case ']':
                        break;
                }
                break;
            }
            break;
        }
        if (*p != '.')
            break;
    }
    
    return *p == '>' && *++p == '\0';
}
