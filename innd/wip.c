/* $Id$
**
** Routines for keeping track of incoming articles, articles that haven't
** acked from a duplex channel feed, and history caching.
**
** WIP stands for work-in-progress
*/

#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include "innd.h"
#include "dbz.h"

#define WIPTABLESIZE        1024
#define WIP_CHECK             5
#define WIP_EXPIRE            10

STATIC WIP     *WIPtable[WIPTABLESIZE];      /* Top level of the WIP hash table */
int            WIPcheck = WIP_CHECK;         /* Amount of time to lock a message id */
int            WIPexpire = WIP_EXPIRE;       /* Amount of time to expire a message id */

void WIPsetup(void) {
    memset(WIPtable, '\0', sizeof(WIPtable));
}

/* Add a new entry into the table.  It is the appilications responsiblity to
   to call WIPinprogress or WIPbyid first. */
WIP *WIPnew(const char *messageid, CHANNEL *cp) {
    HASH hash;
    unsigned long bucket;
    WIP *new;

    hash = Hash(messageid, strlen(messageid));
    memcpy(&bucket, &hash,
	   sizeof(bucket) < sizeof(hash) ? sizeof(bucket) : sizeof(hash));
    bucket = bucket % WIPTABLESIZE;
    
    new = NEW(WIP, sizeof(WIP));
    new->MessageID = hash;
    new->Timestamp = Now.time;
    new->Chan = cp;
    /* Link the new entry into the list */
    new->Next = WIPtable[bucket];
    WIPtable[bucket] = new;
    return new; 
}

void WIPprecomfree(CHANNEL *cp) {
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

void WIPfree(WIP *wp) {
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
    DISPOSE(wp);
}

/* Check if the given messageid is being transfered on another channel.  If
   Add is true then add the given message-id to the current channel */
BOOL WIPinprogress(const char *msgid, CHANNEL *cp, const BOOL Precommit) {
    WIP *wp;
    int i;
    
    if ((wp = WIPbyid(msgid)) != NULL) {
	if ((Now.time - wp->Timestamp) < WIPcheck)
	    return TRUE;
	if ((Now.time - wp->Timestamp) > WIPexpire) {
	    for (i = 0 ; i < PRECOMMITCACHESIZE ; i++) {
		if (wp->Chan->PrecommitWIP[i] == wp) {
		    wp->Chan->PrecommitWIP[i] = (WIP *)NULL;
		    break;
		}
	    }
	    WIPfree(wp);
	    (void)WIPinprogress(msgid, cp, Precommit);
	    return FALSE;
	}
	if (wp->Chan == cp)
	    return TRUE;
	return FALSE;
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
    return FALSE;
}

WIP *WIPbyid(const char *messageid) {
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

WIP *WIPbyhash(const HASH hash) {
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

