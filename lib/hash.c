/* This provides a generic hash function for use w/INN.  Currently
   is implemented using MD5, but should probably have a mechanism for
   choosing the hash algorithm and tagging the hash with the algorithm
   used */
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include "configdata.h"
#include "clibrary.h"
#include "libinn.h"
#include "macros.h"
#include "md5.h"

STATIC HASH empty= { { 0, 0, 0, 0, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0 }};

/* cipoint - where in this message-ID does it become case-insensitive?
 *
 * The RFC822 code is not quite complete.  Absolute, total, full RFC822
 * compliance requires a horrible parsing job, because of the arcane
 * quoting conventions -- abc"def"ghi is not equivalent to abc"DEF"ghi,
 * for example.  There are three or four things that might occur in the
 * domain part of a message-id that are case-sensitive.  They don't seem
 * to ever occur in real news, thank Cthulhu.  (What?  You were expecting
 * a merciful and forgiving deity to be invoked in connection with RFC822?
 * Forget it; none of them would come near it.)
 *
 * Returns: pointer into s, or NULL for "nowhere"
 */
STATIC char *cipoint(char *s, size_t size) {
    char *p;

    if ((p = memchr(s, '@', size))== NULL)			/* no local/domain split */
	return NULL;		/* assume all local */
    if (!strncasecmp("postmaster", s+1, 10)) {
	/* crazy -- "postmaster" is case-insensitive */
	return s;
    }
    return p;
}

HASH Hash(const void *value, const size_t len) {
    MD5_CTX context;
    HASH hash;

    MD5Init(&context);
    MD5Update(&context, value, len);
    MD5Final(&context);
    memcpy(&hash,
	   &context.digest,
	   (sizeof(hash) < sizeof(context.digest)) ? sizeof(hash) : sizeof(context.digest));
    return hash;
}

HASH HashMessageID(const char *MessageID) {
    char                *new;
    char                *cip;
    char                *p;
    int                 len;
    HASH                hash;

    new = COPY(MessageID);
    len = strlen(new);
    if ((cip = cipoint(new, len))) {
	for (p = new; p != cip; p++)
	    *p = tolower(*p);
    }
    hash = Hash(new, len);
    DISPOSE(new);
    return hash;
}

/*
**  Check if the hash is all zeros, and subseqently empty, see HashClear
**  for more info on this.
*/
BOOL HashEmpty(const HASH h) {
    return (memcmp(&empty, &h, sizeof(HASH)) == 0);
}

/*
**  Set the hash to all zeros.  Using all zeros as the value for empty 
**  introduces the possibility of colliding w/a value that actually hashes
**  to all zeros, but that's fairly unlikely.
*/
void HashClear(HASH *hash) {
    memset(hash, '\0', sizeof(HASH));
}

/*
**  Convert the binary form of the hash to a form that we can use in error
**  messages and logs.
*/
char *HashToText(const HASH hash) {
    STATIC char         hex[] = "0123456789ABCEDF";
    char                *p;
    STATIC char         hashstr[(sizeof(HASH) * 2) + 1];
    int                 i;

    for (p = (char *)&hash, i = 0; i < sizeof(HASH); i++, p++) {
	hashstr[i * 2] = hex[(*p & 0xF0) >> 4];
	hashstr[(i * 2) + 1] = hex[*p & 0x0F];
    }
    hashstr[(sizeof(HASH) * 2)] = '\0';
    return hashstr;
}

/*
** Converts a hex digit and converts it to a int
*/
STATIC int hextodec(const char c) {
    return isdigit(c) ? (c - '0') : ((c - 'A') + 10);
}

/*
**  Convert the ASCII representation of the hash back to the canonical form
*/
HASH TextToHash(const char *text) {
    char                *q;
    int                 i;
    HASH                hash;

    for (q = (char *)&hash, i = 0; i != sizeof(HASH); i++) {
	q[i] = (hextodec(text[i * 2]) << 4) + hextodec(text[(i * 2) + 1]);
    }
    return hash;
}
