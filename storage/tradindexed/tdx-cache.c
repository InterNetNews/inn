/*  $Id$
**
**  Cache functions for open overview data files.
**
**  This code maintains a cache of open overview data files to avoid some of
**  the overhead involved in closing and reopening files.  All opens and
**  closes should go through this code, and the hit ratio is tracked to check
**  cache effectiveness.
*/

#include "config.h"
#include "clibrary.h"
#include <time.h>

#include "inn/hashtab.h"
#include "inn/messages.h"
#include "libinn.h"
#include "storage.h"
#include "tdx-private.h"

/* Returned to callers as an opaque data type, this struct holds all of the
   information about the cache. */
struct cache {
    struct hash *hashtable;
    unsigned int count;
    unsigned int max;
    unsigned long queries;
    unsigned long hits;
};

/* A cache entry, holding a group_data struct and some additional information
   used to do cache lookups and to choose what to drop from the cache. */
struct cache_entry {
    struct group_data *data;
    HASH hash;
    time_t lastused;
};


/*
**  The hash function for a cache_entry.  Just return as much of the group
**  hash as can fit in an unsigned long.
*/
static unsigned long
entry_hash(const void *key)
{
    const HASH *hash = key;
    unsigned long bucket;

    memcpy(&bucket, hash, sizeof(bucket));
    return bucket;
}


/*
**  Given a cache_entry, return its key.
*/
static const void *
entry_key(const void *data)
{
    const struct cache_entry *entry = (const struct cache_entry *) data;

    return &entry->hash;
}


/*
**  Check to see if two entries are equal.
*/
static bool
entry_equal(const void *key, const void *data)
{
    const HASH *hash = (const HASH *) key;
    const struct cache_entry *entry = (const struct cache_entry *) data;

    return (memcmp(hash, &entry->hash, sizeof(HASH)) == 0);
}


/*
**  Free a cache entry.
*/
static void
entry_delete(void *data)
{
    struct cache_entry *entry = (struct cache_entry *) data;

    entry->data->refcount--;
    if (entry->data->refcount == 0)
        tdx_data_close(entry->data);
    free(entry);
}


/*
**  Called by hash_traverse, this function finds the oldest entry with the
**  smallest refcount and stores it in the provided pointer so that it can be
**  freed.  This is used when the cache is full to drop the least useful
**  entry.
*/
static void
entry_find_oldest(void *data, void *cookie)
{
    struct cache_entry *entry = (struct cache_entry *) data;
    struct cache_entry **oldest = (struct cache_entry **) cookie;

    if (*oldest == NULL) {
        *oldest = entry;
        return;
    }
    if (entry->data->refcount > (*oldest)->data->refcount)
        return;
    if (entry->lastused > (*oldest)->lastused)
        return;
    *oldest = data;
}


/*
**  Create a new cache with the given size.
*/
struct cache *
tdx_cache_create(unsigned int size)
{
    struct cache *cache;

    cache = xmalloc(sizeof(struct cache));
    cache->count = 0;
    cache->max = size;
    cache->queries = 0;
    cache->hits = 0;
    cache->hashtable = hash_create(size * 4 / 3, entry_hash, entry_key,
                                   entry_equal, entry_delete);
    return cache;
}


/*
**  Look up a particular entry and return it.
*/
struct group_data *
tdx_cache_lookup(struct cache *cache, HASH hash)
{
    struct cache_entry *entry;

    cache->queries++;
    entry = hash_lookup(cache->hashtable, &hash);
    if (entry != NULL) {
        cache->hits++;
        entry->lastused = time(NULL);
    }
    return (entry == NULL) ? NULL : entry->data;
}


/*
**  Insert a new entry, clearing out the oldest entry if the cache is
**  currently full.
*/
void
tdx_cache_insert(struct cache *cache, HASH hash, struct group_data *data)
{
    struct cache_entry *entry;

    if (cache->count == cache->max) {
        struct cache_entry *oldest = NULL;

        hash_traverse(cache->hashtable, entry_find_oldest, &oldest);
        if (oldest == NULL) {
            warn("tradindexed: unable to find oldest cache entry");
            return;
        } else {
            if (!hash_delete(cache->hashtable, &oldest->hash)) {
                warn("tradindexed: cannot delete oldest cache entry");
                return;
            }
        }
        cache->count--;
    }
    entry = xmalloc(sizeof(struct cache_entry));
    entry->data = data;
    entry->hash = hash;
    entry->lastused = time(NULL);
    if (!hash_insert(cache->hashtable, &entry->hash, entry)) {
        warn("tradindexed: duplicate cache entry for %s", HashToText(hash));
        free(entry);
    } else {
        entry->data->refcount++;
        cache->count++;
    }
}


/*
**  Delete an entry from the cache.
*/
void
tdx_cache_delete(struct cache *cache, HASH hash)
{
    if (!hash_delete(cache->hashtable, &hash))
        warn("tradindexed: unable to remove cache entry for %s",
             HashToText(hash));
}


/*
**  Delete the cache and all of the resources that it's holding open.
*/
void
tdx_cache_free(struct cache *cache)
{
    hash_free(cache->hashtable);
    free(cache);
}
