/*  $Id$
**
**  MessageID to storage token cache
**
**  Written by Alex Kiernan (alex.kiernan@thus.net)
**
**  Implementation of a message ID to storage token cache which can be
**  built during XOVER/XHDR/NEWNEWS.  If we hit in the cache when
**  retrieving articles the (relatively) expensive cost of a trip
**  through the history database is saved.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "inn/tst.h"
#include "libinn.h"
#include "storage.h"

#include "cache.h"

/*
**  Pointer to the message ID to storage token ternary search tree
*/
static struct tst *msgidcache;

/*
**  Count of message IDs in the cache so that someone doing GROUP,
**  XOVER, GROUP, XOVER etc. for example doesn't blow up with out of
**  memory
*/
static int msgcachecount;


/*
**  Add a translation from HASH, h, to TOKEN, t, to the message ID
**  cache
*/
void
cache_add(const HASH h, const TOKEN t)
{
    if (msgcachecount < innconf->msgidcachesize) {
	if (!msgidcache)
	    msgidcache = tst_init(innconf->msgidcachesize / 10);
	if (msgidcache) {
	    TOKEN *token = malloc(sizeof *token);

	    if (token) {
		const unsigned char *p;

		p = (unsigned char *) HashToText(h);
		*token = t;
		
		if (tst_insert(msgidcache, p, token,
			       0, NULL) == TST_DUPLICATE_KEY) {
		    free(token);
		} else {
		    ++msgcachecount;
		}
	    }
	}
    }
}


/*
**  Lookup (and remove if found) a MessageID to TOKEN mapping. We
**  remove it if we find it since this matches the observed behaviour
**  of most clients, but cache it just in case we can reuse it if they
**  issue multiple commands against the same message ID (e.g. HEAD,
**  BODY).
*/
TOKEN
cache_get(const HASH h)
{
    static HASH last_hash;
    static TOKEN last_token;
    static const TOKEN empty_token = { TOKEN_EMPTY, 0, "" };

    if (HashCompare(&h, &last_hash) == 0 && !HashEmpty(last_hash))
	return last_token;

    if (msgidcache) {
	TOKEN *t;

	/* we can improve this... rather than delete the entry, make the pointer
	   in the TST a structure which includes an LRU list. Then in _add
	   expire entries from the front of the list as we need to steal them
        */
	t = tst_delete(msgidcache, (unsigned char *) HashToText(h));
	if (t != NULL) {
	    --msgcachecount;
	    last_token = *t;
	    last_hash = h;
	    free(t);
	    return last_token;
	}
    }
    return empty_token;
}
