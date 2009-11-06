/* $Id$
**
** Routines for keeping track of incoming articles, articles that haven't
** acked from a duplex channel feed, and history caching.
**
** WIP stands for work-in-progress
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "innd.h"

#define WIPTABLESIZE        1024
#define WIP_ARTMAX          300		/* innfeed default max send time */

static WIP     *WIPtable[WIPTABLESIZE];      /* Top level of the WIP hash table */

void
WIPsetup(void)
{
    memset(WIPtable, '\0', sizeof(WIPtable));
}

/* Add a new entry into the table.  It is the appilications responsiblity to
   to call WIPinprogress or WIPbyid first. */
WIP *
WIPnew(const char *messageid, CHANNEL *cp)
{
    HASH hash;
    unsigned long bucket;
    WIP *new;

    hash = Hash(messageid, strlen(messageid));
    memcpy(&bucket, &hash,
	   sizeof(bucket) < sizeof(hash) ? sizeof(bucket) : sizeof(hash));
    bucket = bucket % WIPTABLESIZE;
    
    new = xmalloc(sizeof(WIP));
    new->MessageID = hash;
    new->Timestamp = Now.tv_sec;
    new->Chan = cp;
    /* Link the new entry into the list */
    new->Next = WIPtable[bucket];
    WIPtable[bucket] = new;
    return new; 
}

void
WIPprecomfree(CHANNEL *cp)
{
    WIP *cur;
    int i;
    if (cp == NULL)
	return;

    for (i = 0 ; i < PRECOMMITCACHESIZE ; i++) {
	cur = cp->PrecommitWIP[i];
	if (cur != (WIP *)NULL) {
	    WIPfree(cur);
	}
    }
}

void
WIPfree(WIP *wp)
{
    unsigned long bucket;
    WIP *cur;
    WIP *prev = NULL;
    int i;
    /* This is good error checking, but also allows us to
          WIPfree(WIPbymessageid(id))
       without having to check if id exists first */
    if (wp == NULL)
	return;

    memcpy(&bucket, &wp->MessageID,
	   sizeof(bucket) < sizeof(HASH) ? sizeof(bucket) : sizeof(HASH));
    bucket = bucket % WIPTABLESIZE;

    for (i = 0 ; i < PRECOMMITCACHESIZE ; i++) {
	if (wp->Chan->PrecommitWIP[i] == wp) {
	    wp->Chan->PrecommitWIP[i] = (WIP *)NULL;
	    break;
	}
    }
    for (cur = WIPtable[bucket]; (cur != NULL) && (cur != wp); prev = cur, cur = cur->Next);

    if (cur == NULL)
	return;

    if (prev == NULL)
	WIPtable[bucket] = cur->Next;
    else
	prev->Next = cur->Next;

    /* unlink the entry and free the memory */
    free(wp);
}

/* Check if the given messageid is being transfered on another channel.  If
   Add is true then add the given message-id to the current channel */
bool
WIPinprogress(const char *msgid, CHANNEL *cp, bool Precommit)
{
    WIP *wp;
    unsigned long i;
    
    if ((wp = WIPbyid(msgid)) != NULL) {
	if(wp->Chan->ArtBeg == 0)
	    i = 0;
	else {
	    i = wp->Chan->ArtMax;
	    if(i > WIP_ARTMAX)
		i = WIP_ARTMAX;
	}
 
	if ((Now.tv_sec - wp->Timestamp) < (time_t) (i + innconf->wipcheck))
	    return true;
	if ((Now.tv_sec - wp->Timestamp) > (time_t) (i + innconf->wipexpire)) {
	    for (i = 0 ; i < PRECOMMITCACHESIZE ; i++) {
		if (wp->Chan->PrecommitWIP[i] == wp) {
		    wp->Chan->PrecommitWIP[i] = (WIP *)NULL;
		    break;
		}
	    }
	    WIPfree(wp);
	    WIPinprogress(msgid, cp, Precommit);
	    return false;
	}
	if (wp->Chan == cp)
	    return true;
	return false;
    }
    wp = WIPnew(msgid, cp);
    if (Precommit) {
	if (cp->PrecommitiCachenext == PRECOMMITCACHESIZE)
	    cp->PrecommitiCachenext = 0;
	if (cp->PrecommitWIP[cp->PrecommitiCachenext])
	    WIPfree(cp->PrecommitWIP[cp->PrecommitiCachenext]);
	cp->PrecommitWIP[cp->PrecommitiCachenext++] = wp;
    } else {
	WIPfree(WIPbyhash(cp->CurrentMessageIDHash));
	cp->CurrentMessageIDHash = wp->MessageID;
    }
    return false;
}

WIP *
WIPbyid(const char *messageid)
{
    HASH hash;
    unsigned long bucket;
    WIP *wp;

    hash = Hash(messageid, strlen(messageid));
    memcpy(&bucket, &hash,
	   sizeof(bucket) < sizeof(hash) ? sizeof(bucket) : sizeof(hash));
    bucket = bucket % WIPTABLESIZE;
    
    /* Traverse the list until we find a match or find the head again */
    for (wp = WIPtable[bucket]; wp != NULL; wp = wp->Next) 
	if (!memcmp(&hash, &wp->MessageID, sizeof(HASH)))
	    return wp;

    return NULL;
}

WIP *
WIPbyhash(const HASH hash)
{
    unsigned long bucket;
    WIP *wp;

    memcpy(&bucket, &hash,
	   sizeof(bucket) < sizeof(hash) ? sizeof(bucket) : sizeof(hash));
    bucket = bucket % WIPTABLESIZE;
    
    /* Traverse the list until we find a match or find the head again */
    for (wp = WIPtable[bucket]; wp != NULL; wp = wp->Next) 
	if (!memcmp(&hash, &wp->MessageID, sizeof(HASH)))
	    return wp;

    return NULL;
}
