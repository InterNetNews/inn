/* $Revision$
**
** Routines for keeping track of incoming articles, articles that haven't
** acked from a duplex channel feed, and history caching.
**
** WIP stands for work-in-progress
*/

#include <stdio.h>
#include "configdata.h"
#include "clibrary.h"
#include "innd.h"
#include "dbz.h"

#define WIPTABLESIZE        1024
#define WIP_CHECK             5

STATIC WIP     *WIPtable[WIPTABLESIZE];      /* Top level of the WIP hash table */
int            WIPcheck = WIP_CHECK;         /* Amount of time to lock a message id */

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

void WIPfree(WIP *wp) {
    unsigned long bucket;
    WIP *cur;
    WIP *prev = NULL;
    /* This is good error checking, but also allows us to
          WIPfree(WIPbymessageid(id))
       without having to check if id exists first */
    if (wp == NULL)
	return;

    memcpy(&bucket, &wp->MessageID,
	   sizeof(bucket) < sizeof(HASH) ? sizeof(bucket) : sizeof(HASH));
    bucket = bucket % WIPTABLESIZE;

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
BOOL WIPinprogress(const char *msgid, CHANNEL *cp, const BOOL Add) {
    WIP *wp;
    
    if ((wp = WIPbyid(msgid)) != NULL) {
        if ((Now.time - wp->Timestamp) < WIPcheck)
	    return TRUE;
	if (wp->Chan == cp)
	    return TRUE;
    }
    if (Add) {
	wp = WIPnew(msgid, cp);
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

