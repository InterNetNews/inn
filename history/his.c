/*  $Id$
**
**  API to history routines 
**
**  Copyright (c) 2001, Thus plc 
**  
**  Redistribution and use of the source code in source and binary 
**  forms, with or without modification, are permitted provided that
**  the following 3 conditions are met:
**  
**  1. Redistributions of the source code must retain the above 
**  copyright notice, this list of conditions and the disclaimer 
**  set out below. 
**  
**  2. Redistributions of the source code in binary form must 
**  reproduce the above copyright notice, this list of conditions 
**  and the disclaimer set out below in the documentation and/or 
**  other materials provided with the distribution. 
**  
**  3. Neither the name of the Thus plc nor the names of its 
**  contributors may be used to endorse or promote products 
**  derived from this software without specific prior written 
**  permission from Thus plc. 
**  
**  Disclaimer:
**  
**  "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
**  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
**  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE DIRECTORS
**  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
**  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
**  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
**  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
**  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
**  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
**  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
*/

#include "config.h"
#include "clibrary.h"
#include "errno.h"
#include "portable/time.h"
#include <syslog.h>
#include "libinn.h"
#include "macros.h"
#include "inn/history.h"
#include "inn/messages.h"
#include "inn/timer.h"
#include "hisinterface.h"
#include "hismethods.h"
#include "storage.h"

struct hiscache {
    HASH Hash;	/* Hash value of the message-id using Hash() */
    bool Found;	/* Whether this entry is in the dbz file yet */
};

struct history {
    struct hismethod *methods;
    void *sub;
    struct hiscache *cache;
    size_t cachesize;
    const char *error;
    struct histstats stats;
};

enum HISRESULT {HIScachehit, HIScachemiss, HIScachedne};

static const struct histstats nullhist = {0};

/*
** Put an entry into the history cache 
*/
static void
his_cacheadd(struct history *h, HASH MessageID, bool Found)
{
    unsigned int  i, loc;

    his_logger("HIScacheadd begin", S_HIScacheadd);
    if (h->cache != NULL) {
	memcpy(&loc, ((char *)&MessageID) + (sizeof(HASH) - sizeof(loc)),
	       sizeof(loc));
	i = loc % h->cachesize;
	memcpy((char *)&h->cache[i].Hash, (char *)&MessageID, sizeof(HASH));
	h->cache[i].Found = Found;
    }
    his_logger("HIScacheadd end", S_HIScacheadd);
}

/*
** Lookup an entry in the history cache
*/
static enum HISRESULT
his_cachelookup(struct history *h, HASH MessageID)
{
    unsigned int i, loc;

    if (h->cache == NULL)
	return HIScachedne;
    his_logger("HIScachelookup begin", S_HIScachelookup);
    memcpy(&loc, ((char *)&MessageID) + (sizeof(HASH) - sizeof(loc)), sizeof(loc));
    i = loc % h->cachesize;
    if (memcmp((char *)&h->cache[i].Hash, (char *)&MessageID, sizeof(HASH)) == 0) {
	his_logger("HIScachelookup end", S_HIScachelookup);
        if (h->cache[i].Found) {
            return HIScachehit;
        } else {
            return HIScachemiss;
        }
    } else {
	his_logger("HIScachelookup end", S_HIScachelookup);
        return HIScachedne;
    }
}

/*
**  set error status to that indicated by s; doesn't copy the string,
**  assumes the caller did that for us
*/
void
his_seterror(struct history *h, const char *s)
{
    if (h != NULL) {
	if (h->error)
	    free((void *)h->error);
	h->error = s;
    }
    if (s != NULL)
	warn("%s", s);
}

struct history *
HISopen(const char *path, const char *method, int flags)
{
    struct history *h;
    int i;

    for (i = 0; i < NUM_HIS_METHODS; ++i) {
	if (strcmp(method, his_methods[i].name) == 0)
	    break;
    }
    if (i == NUM_HIS_METHODS) {
	warn("`%s' isn't a valid history method", method);
	return NULL;
    }

    /* allocate up our structure and start subordinate history
     * manager */
    h = xmalloc(sizeof *h);
    h->methods = &his_methods[i];
    h->cache = NULL;
    h->error = NULL;
    h->cachesize = 0;
    h->stats = nullhist;
    h->sub = (*h->methods->open)(path, flags, h);
    if (h->sub == NULL) {
	free(h);
	h = NULL;
    }
    return h;
}

static bool
his_checknull(struct history *h)
{
    if (h != NULL)
	return false;
    errno = EBADF;
    return true;

}

bool 
HISclose(struct history *h)
{
    bool r;

    if (his_checknull(h))
	return false;
    r = (*h->methods->close)(h->sub);
    if (h->cache) {
	free(h->cache);
	h->cache = NULL;
    }
    if (h->error) {
	free((void *)h->error);
	h->error = NULL;
    }
    free(h);
    return r;
}

bool
HISsync(struct history *h)
{
    bool r = false;

    if (his_checknull(h))
	return false;
    TMRstart(TMR_HISSYNC);
    r = (*h->methods->sync)(h->sub);
    TMRstop(TMR_HISSYNC);
    return r;
}

bool
HISlookup(struct history *h, const char *key, time_t *arrived,
	  time_t *posted, time_t *expires, TOKEN *token)
{
    bool r;

    if (his_checknull(h))
	return false;
    TMRstart(TMR_HISGREP);
    r = (*h->methods->lookup)(h->sub, key, arrived, posted, expires, token);
    TMRstop(TMR_HISGREP);
    return r;
}

bool
HIScheck(struct history *h, const char *key)
{
    bool r;
    HASH hash;

    if (his_checknull(h))
	return false;
    TMRstart(TMR_HISHAVE);
    hash = HashMessageID(key);
    switch (his_cachelookup(h, hash)) {
    case HIScachehit:
	h->stats.hitpos++;
	r = true;
	break;

    case HIScachemiss:
	h->stats.hitneg++;
	r = false;
	break;

    case HIScachedne:
	r = (*h->methods->check)(h->sub, key);
	his_cacheadd(h, hash, r);
	if (r)
	    h->stats.misses++;
	else
	    h->stats.dne++;
	break;
    }
    TMRstop(TMR_HISHAVE);
    return r;
}

bool
HISwrite(struct history *h, const char *key, time_t arrived,
	 time_t posted, time_t expires, const TOKEN *token)
{
    bool r;

    if (his_checknull(h))
	return false;
    TMRstart(TMR_HISWRITE);
    r = (*h->methods->write)(h->sub, key, arrived, posted, expires, token);
    if (r == true) {
	HASH hash;

	/* if we successfully wrote it, add it to the cache */
	hash = HashMessageID(key);
	his_cacheadd(h, hash, true);
    }
    TMRstop(TMR_HISWRITE);

    return r;
}

bool
HISremember(struct history *h, const char *key, time_t arrived)
{
    bool r;

    if (his_checknull(h))
	return false;
    TMRstart(TMR_HISWRITE);
    r = (*h->methods->remember)(h->sub, key, arrived);
    if (r == true) {
	HASH hash;

	/* if we successfully wrote it, add it to the cache */
	hash = HashMessageID(key);
	his_cacheadd(h, hash, true);
    }
    TMRstop(TMR_HISWRITE);

    return r;
}

bool
HISreplace(struct history *h, const char *key, time_t arrived,
	   time_t posted, time_t expires, const TOKEN *token)
{
    bool r;

    if (his_checknull(h))
	return false;
    r = (*h->methods->replace)(h->sub, key, arrived, posted, expires, token);
    if (r == true) {
	HASH hash;

	/* if we successfully wrote it, add it to the cache */
	hash = HashMessageID(key);
	his_cacheadd(h, hash, true);
    }
    return r;
}

bool
HISwalk(struct history *h, const char *reason, void *cookie,
	bool (*callback)(void *, time_t, time_t, time_t, const TOKEN *))
{
    bool r;

    if (his_checknull(h))
	return false;
    r = (*h->methods->walk)(h->sub, reason, cookie, callback);
    return r;
}

bool
HISexpire(struct history *h, const char *path, const char *reason,
	  bool writing, void *cookie, time_t threshold,
	  bool (*exists)(void *, time_t, time_t, time_t, TOKEN *))
{
    bool r;

    if (his_checknull(h))
	return false;
    r = (*h->methods->expire)(h->sub, path, reason, writing,
			      cookie, threshold, exists);
    return r;
}

void
HISsetcache(struct history *h, size_t size)
{
    if (h == NULL)
	return;
    if (h->cache) {
	free(h->cache);
	h->cache = NULL;
    }
    h->cachesize = size / sizeof(struct hiscache);
    if (h->cachesize != 0) {
	h->cache = malloc(h->cachesize * sizeof(struct hiscache));
	memset(h->cache, '\0', h->cachesize * sizeof h->cache);
    }
    h->stats = nullhist;
}


/*
**  return current history cache stats and zero the counters
*/
struct histstats
HISstats(struct history *h)
{
    struct histstats r;

    if (h == NULL)
	return nullhist;
    r = h->stats;
    h->stats = nullhist;
    return r;
}


/*
**  return current error status to caller
*/
const char *
HISerror(struct history *h)
{
    if (h == NULL)
	return NULL;
    return h->error;
}


/*
**  control interface to underlying method
*/
bool
HISctl(struct history *h, int selector, void *val)
{
    bool r;

    if (his_checknull(h))
	return false;
    r = (*h->methods->ctl)(h->sub, selector, val);
    return r;
}


/*
**  This code doesn't fit well with the generic history API, it really
**  needs migrating to use the new nested timers
*/

FILE             *HISfdlog = NULL; /* filehandle for history logging purpose */

static struct timeval HISstat_start[S_HIS_MAX];
static struct timeval HISstat_total[S_HIS_MAX];
static unsigned long  HISstat_count[S_HIS_MAX];

void HISlogclose(void) {
   if (HISfdlog != NULL)
       Fclose(HISfdlog);
   HISfdlog = NULL;
}

void HISlogto(const char *s) {
   int i;

   HISlogclose();
   if ((HISfdlog = Fopen(s, "a", INND_HISLOG)) == NULL)
       syslog(L_FATAL, "cant open %s %m", s);
   /* initialize our counters */
   for (i = 0; i < S_HIS_MAX; i++) {
       HISstat_start[i].tv_sec = 0;
       HISstat_start[i].tv_usec = 0;
       HISstat_total[i].tv_sec = 0;
       HISstat_total[i].tv_usec = 0;
       HISstat_count[i] = 0;
   }
}

void
his_logger(char *s, int code)
{
    struct timeval tv;
    struct tm *tm;

    if (HISfdlog == NULL) /* do nothing unless HISlogto() has been called */
	return;

    gettimeofday(&tv, NULL);
    tm = localtime((const time_t *)&(tv.tv_sec));
    if (HISstat_start[code].tv_sec != 0) {
	fprintf(HISfdlog, "%d/%d/%d %02d:%02d:%02d.%06d: [%d] %s (%.6f)\n",
		tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
		tm->tm_min, tm->tm_sec, (int)tv.tv_usec, code, s, (float) tv.tv_sec +
		(float) tv.tv_usec / 1000000 - (float) HISstat_start[code].tv_sec -
		(float) HISstat_start[code].tv_usec / 1000000);
	if (tv.tv_usec < HISstat_start[code].tv_usec) {
	    HISstat_total[code].tv_sec++;
	    HISstat_total[code].tv_usec +=
		tv.tv_usec - HISstat_start[code].tv_usec + 1000000;
      }
      else
          HISstat_total[code].tv_usec +=
              tv.tv_usec - HISstat_start[code].tv_usec;
	HISstat_total[code].tv_sec += tv.tv_sec - HISstat_start[code].tv_sec;
	HISstat_count[code]++;
	HISstat_start[code].tv_sec = 0;
	HISstat_start[code].tv_usec = 0;
    }
    else {
	fprintf(HISfdlog, "%d/%d/%d %02d:%02d:%02d.%06d: [%d] %s\n",
		tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
		tm->tm_min, tm->tm_sec, (int)tv.tv_usec, code, s);
	HISstat_start[code].tv_sec = tv.tv_sec;
	HISstat_start[code].tv_usec = tv.tv_usec;
    }
}
