/*
**  Routines for message identifiers: generation and checks.
*/

#include "portable/system.h"

#include <ctype.h>
#include <time.h>

#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/nntp.h"

/*  Scale time back a bit, for shorter message-IDs. */
#define OFFSET 673416000L

/*
**  Flag array, indexed by character.  Character classes for message-IDs.
*/
static bool initialized = false;
static char midcclass[256];
#define CC_MSGID_ATOM  01
#define CC_MSGID_NORM  02
#define midnormchar(c) ((midcclass[(unsigned char) (c)] & CC_MSGID_NORM) != 0)
#define midatomchar(c) ((midcclass[(unsigned char) (c)] & CC_MSGID_ATOM) != 0)

static bool IsValidRightPartMessageID(const char *domain, bool stripspaces,
                                      bool bracket);

/*
**  Generate a Message-ID.
**  The left-hand side is currently based on the current time, nnrpd's PID and
**  a global static counter incrementing at each post in the same NNTP session.
**  These data are encoded with a 32-character alphabet.
**  Chances of collision should be rare (unless multiple hosts are using the
**  same right-hand side).
**
**  The right-hand side is the given argument (if not NULL) or otherwise the
**  FQDN of the server.  The caller is responsible to ensure the given
**  argument, if any, is a valid domain name (it will otherwise be use as-is).
**
**  This function returns the generated Message-ID or NULL if no valid domain
**  could be picked for the right-hand side.
*/
char *
GenerateMessageID(char *domain)
{
    static char buff[SMBUF];
    static int count;
    char *p;
    char *fqdn = NULL;
    char sec32[10];
    char pid32[10];
    time_t now;

    now = time(NULL);
    Radix32(now - OFFSET, sec32);
    Radix32(getpid(), pid32);

    if (domain != NULL)
        p = domain;
    else {
        fqdn = inn_getfqdn(innconf->domain);
        if (!IsValidDomain(fqdn))
            return NULL;
        p = fqdn;
    }

    snprintf(buff, sizeof(buff), "<%s$%s$%d@%s>", sec32, pid32, ++count, p);
    free(fqdn);
    return buff;
}


/*
**  Initialize the character class tables.
**  See Section 3.2.3 of RFC 5322 (atext) and Section 3.1.3 of RFC 5536
**  (mdtext).
*/
static void
InitializeMessageIDcclass(void)
{
    const unsigned char *p;
    unsigned int i;

    /* Set up the character class tables.  These are written a
     * little strangely to work around a GCC2.0 bug. */
    memset(midcclass, 0, sizeof(midcclass));

    p = (const unsigned char *) "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRS"
                                "TUVWXYZ0123456789";
    while ((i = *p++) != 0) {
        midcclass[i] = CC_MSGID_ATOM | CC_MSGID_NORM;
    }

    p = (const unsigned char *) "!#$%&'*+-/=?^_`{|}~";
    while ((i = *p++) != 0) {
        midcclass[i] = CC_MSGID_ATOM | CC_MSGID_NORM;
    }

    p = (const unsigned char *) "\"(),.:;<@";
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
**
**  When stripspaces is true, whitespace at the beginning and at the end
**  of MessageID are discarded.
**
**  When laxsyntax is true, '@' can occur twice in MessageID, and '..' is
**  also accepted in the left part of MessageID.
*/
bool
IsValidMessageID(const char *MessageID, bool stripspaces, bool laxsyntax)
{
    bool atfound = false;
    const unsigned char *p;

    if (!initialized) {
        InitializeMessageIDcclass();
        initialized = true;
    }

    /* Check the length of the message identifier. */
    if (MessageID == NULL || strlen(MessageID) > NNTP_MAXLEN_MSGID)
        return false;

    p = (const unsigned char *) MessageID;

    if (stripspaces) {
        for (; ISWHITE(*p); p++)
            ;
    }

    /* Scan local-part: "< dot-atom-text". */
    if (*p++ != '<')
        return false;
    for (;; p++) {
        if (midatomchar(*p)) {
            while (midatomchar(*++p))
                continue;
        } else {
            /* Invalid character.
             * Also ensure we have at least one character. */
            return false;
        }
        if (*p != '.') {
            if (laxsyntax && *p == '@') {
                /* The domain part begins at the second '@', if it exists. */
                if (atfound || (p[1] == '[')
                    || (strchr((const char *) p + 1, '@') == NULL)) {
                    break;
                }
                atfound = true;
                continue;
            } else {
                break;
            }
        }
        /* Dot found. */
        if (laxsyntax) {
            if (*p != '\0' && p[1] == '.') {
                p++;
            }
        }
    }

    /* Scan domain: "@ dot-atom-text|no-fold-literal > \0" */
    if (*p++ != '@')
        return false;

    return IsValidRightPartMessageID((const char *) p, stripspaces, true);
}


/*
**  Check the syntax of the right-hand side of a message identifier.
**
**  If stripspaces is true, whitespace at the end of domain are discarded.
**  If bracket is true, '>' is expected at the end of domain.
*/
static bool
IsValidRightPartMessageID(const char *domain, bool stripspaces, bool bracket)
{
    int c;
    bool firstchar = true;
    const unsigned char *p;

    if (!initialized) {
        InitializeMessageIDcclass();
        initialized = true;
    }

    if (domain == NULL)
        return false;

    p = (const unsigned char *) domain;

    for (;; p++) {
        if (midatomchar(*p)) {
            firstchar = false;
            while (midatomchar(*++p))
                continue;
        } else {
            /* no-fold-literal only */
            if (!firstchar || *p++ != '[')
                return false;
            for (;;) {
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
        /* Beginning of another component? */
        if (*p != '.')
            break;
    }

    /* Final bracket in a Message-ID? */
    if (bracket && *p++ != '>')
        return false;

    if (stripspaces) {
        for (; ISWHITE(*p); p++)
            ;
    }

    return (*p == '\0');
}


/*
**  Wrapper around the function which actually does the check.
*/
bool
IsValidDomain(const char *domain)
{
    return IsValidRightPartMessageID(domain, false, false);
}
