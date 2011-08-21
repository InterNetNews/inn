/* This provides a generic hash function for use w/INN.  Currently
   is implemented using MD5, but should probably have a mechanism for
   choosing the hash algorithm and tagging the hash with the algorithm
   used */
#include "config.h"
#include "clibrary.h"
#include <ctype.h>

#include "inn/md5.h"
#include "inn/utility.h"
#include "inn/libinn.h"

static HASH empty= { { 0, 0, 0, 0, 0, 0, 0, 0,
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
static const char *
cipoint(const char *s, size_t size)
{
    char *p;
    static const char post[] = "postmaster";
    static int plen = sizeof(post) - 1;

    if ((p = memchr(s, '@', size))== NULL)	/* no local/domain split */
	return NULL;				/* assume all local */
    if ((p - (s + 1) == plen) && !strncasecmp(post, s+1, plen)) {
	/* crazy -- "postmaster" is case-insensitive */
	return s;
    }
    return p;
}

HASH
Hash(const void *value, const size_t len)
{
    struct md5_context context;
    HASH hash;

    md5_init(&context);
    md5_update(&context, value, len);
    md5_final(&context);
    memcpy(&hash,
	   &context.digest,
	   (sizeof(hash) < sizeof(context.digest)) ? sizeof(hash) : sizeof(context.digest));
    return hash;
}

HASH
HashMessageID(const char *MessageID)
{
    char                *new = NULL;
    const char          *cip, *p = NULL;
    char                *q;
    int                 len;
    HASH                hash;

    len = strlen(MessageID);
    cip = cipoint(MessageID, len);
    if (cip != NULL) {
        for (p = cip + 1; *p != '\0'; p++) {
            if (!islower((unsigned char) *p)) {
                new = xstrdup(MessageID);
                break;
            }
        }
    }
    if (new != NULL)
        for (q = new + (p - MessageID); *q != '\0'; q++)
            *q = tolower((unsigned char) *q);
    hash = Hash(new ? new : MessageID, len);
    if (new != NULL)
	free(new);
    return hash;
}

/*
**  Check if the hash is all zeros, and subseqently empty, see HashClear
**  for more info on this.
*/
bool
HashEmpty(const HASH h)
{
    return (memcmp(&empty, &h, sizeof(HASH)) == 0);
}

/*
**  Set the hash to all zeros.  Using all zeros as the value for empty 
**  introduces the possibility of colliding w/a value that actually hashes
**  to all zeros, but that's fairly unlikely.
*/
void
HashClear(HASH *hash)
{
    memset(hash, '\0', sizeof(HASH));
}

/*
**  Convert the binary form of the hash to a form that we can use in error
**  messages and logs.  Just a wrapper around inn_encode_hex.
*/
char *
HashToText(const HASH hash)
{
    static char	hashstr[(sizeof(HASH) * 2) + 1];

    inn_encode_hex((unsigned char *) hash.hash, sizeof(HASH), hashstr,
                   sizeof(hashstr));
    return hashstr;
}

/*
**  Convert the ASCII representation of the hash back to the canonical form.
**  Just a wrapper around inn_decode_hex.
*/
HASH
TextToHash(const char *text)
{
    HASH hash;

    inn_decode_hex(text, (unsigned char *) hash.hash, sizeof(HASH));
    return hash;
}

/* This is rather gross, we compare like the last 4 bytes of the
   hash are at the beginning because dbz considers them to be the
   most significant bytes */
int HashCompare(const HASH *h1, const HASH *h2) {
    int i;
    int tocomp = sizeof(HASH) - sizeof(unsigned long);
    
    if ((i = memcmp(&h1->hash[tocomp], &h1->hash[tocomp], sizeof(unsigned long))))
	return i;
    return memcmp(h1, h2, sizeof(HASH));
}
