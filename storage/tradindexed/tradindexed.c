/*  $Id$
**
**  Interface implementation for the tradindexed overview method.
**
**  This code converts between the internal interface used by the tradindexed
**  implementation and the interface expected by the INN overview API.  The
**  internal interface is in some cases better suited to the data structures
**  that the tradindexed overview method uses, and this way the internal
**  interface can be kept isolated from the external interface.  (There are
**  also some operations that can be performed entirely in the interface
**  layer.)
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "inn/messages.h"
#include "libinn.h"
#include "ov.h"
#include "storage.h"
#include "tdx-private.h"
#include "tdx-structure.h"
#include "tradindexed.h"

/* This structure holds all of the data about the open overview files.  We can
   eventually pass one of these structures back to the caller of open when the
   overview API is more object-oriented. */
struct tradindexed {
    struct group_index *index;
    struct cache *cache;
    bool cutoff;
};

/* Global data about the open tradindexed method. */
static struct tradindexed *tradindexed;


/*
**  Helper function to open a group_data structure via the cache, inserting it
**  into the cache if it wasn't found in the cache.
*/
static struct group_data *
data_cache_open(struct tradindexed *global, const char *group,
                struct group_entry *entry)
{
    struct group_data *data;

    data = tdx_cache_lookup(global->cache, entry->hash);
    if (data == NULL) {
        data = tdx_data_open(global->index, group, entry);
        if (data == NULL)
            return NULL;
        tdx_cache_insert(global->cache, entry->hash, data);
    }
    return data;
}


/*
**  Helper function to reopen the data files and remove the old entry from the
**  cache if we think that might help better fulfill a search.
*/
static struct group_data *
data_cache_reopen(struct tradindexed *global, const char *group,
                  struct group_entry *entry)
{
    struct group_data *data;

    tdx_cache_delete(global->cache, entry->hash);
    data = tdx_data_open(global->index, group, entry);
    if (data == NULL)
        return NULL;
    tdx_cache_insert(global->cache, entry->hash, data);
    return data;
}


/*
**  Open the overview method.
*/
bool
tradindexed_open(int mode)
{
    unsigned int cache_size, fdlimit;

    if (tradindexed != NULL) {
        warn("tradindexed: overview method already open");
        return false;
    }
    tradindexed = xmalloc(sizeof(struct tradindexed));
    tradindexed->index = tdx_index_open((mode & OV_WRITE) ? true : false);
    tradindexed->cutoff = false;

    /* Use a cache size of two for read-only connections.  We may want to
       rethink the limitation of the cache for reading later based on
       real-world experience. */
    cache_size = (mode & OV_WRITE) ? innconf->overcachesize : 1;
    fdlimit = getfdlimit();
    if (fdlimit > 0 && fdlimit < cache_size * 2) {
        warn("tradindexed: not enough file descriptors for an overview cache"
             " size of %u; increase rlimitnofile or decrease overcachesize"
             " to at most %u", cache_size, fdlimit / 2);
        cache_size = (fdlimit > 2) ? fdlimit / 2 : 1;
    }
    tradindexed->cache = tdx_cache_create(cache_size);

    return (tradindexed->index == NULL) ? false : true;
}


/*
**  Get statistics about a group.  Convert between the multiple pointer API
**  and the structure API used internally.
*/
bool
tradindexed_groupstats(char *group, int *low, int *high, int *count,
                       int *flag)
{
    const struct group_entry *entry;

    if (tradindexed == NULL || tradindexed->index == NULL) {
        warn("tradindexed: overview method not initialized");
        return false;
    }
    entry = tdx_index_entry(tradindexed->index, group);
    if (entry == NULL)
        return false;
    if (low != NULL)
        *low = entry->low;
    if (high != NULL)
        *high = entry->high;
    if (count != NULL)
        *count = entry->count;
    if (flag != NULL)
        *flag = entry->flag;
    return true;
}


/*
**  Add a new newsgroup to the index.
*/
bool
tradindexed_groupadd(char *group, ARTNUM low, ARTNUM high, char *flag)
{
    if (tradindexed == NULL || tradindexed->index == NULL) {
        warn("tradindexed: overview method not initialized");
        return false;
    }
    return tdx_index_add(tradindexed->index, group, low, high, flag);
}


/*
**  Delete a newsgroup from the index.
*/
bool
tradindexed_groupdel(char *group)
{
    if (tradindexed == NULL || tradindexed->index == NULL) {
        warn("tradindexed: overview method not initialized");
        return false;
    }
    return tdx_index_delete(tradindexed->index, group);
}


/*
**  Add data about a single article.  Convert between the multiple argument
**  API and the structure API used internally, and also implement low article
**  cutoff if that was requested.
*/
bool
tradindexed_add(char *group, ARTNUM artnum, TOKEN token, char *data,
                int length, time_t arrived, time_t expires)
{
    struct article article;
    struct group_data *group_data;
    struct group_entry *entry;

    if (tradindexed == NULL || tradindexed->index == NULL) {
        warn("tradindexed: overview method not initialized");
        return false;
    }

    /* Get the group index entry and don't do any work if cutoff is set and
       the article number is lower than the low water mark for the group. */
    entry = tdx_index_entry(tradindexed->index, group);
    if (entry == NULL)
        return false;
    if (tradindexed->cutoff && entry->low > artnum)
        return true;

    /* Fill out the article data structure. */
    article.number = artnum;
    article.overview = data;
    article.overlen = length;
    article.token = token;
    article.arrived = arrived;
    article.expires = expires;

    /* Open the appropriate data structures, using the cache. */
    group_data = data_cache_open(tradindexed, group, entry);
    if (group_data == NULL)
        return false;
    return tdx_data_add(tradindexed->index, entry, group_data, &article);
}


/*
**  Cancel an article.  At present, tradindexed can't do anything with this
**  information because we lack a mapping from the token to newsgroup names
**  and article numbers, so we just silently return true and let expiration
**  take care of this.
*/
bool
tradindexed_cancel(TOKEN token UNUSED)
{
    return true;
}


/*
**  Open an overview search.  Open the appropriate group and then start a
**  search in it.
*/
void *
tradindexed_opensearch(char *group, int low, int high)
{
    struct group_entry *entry;
    struct group_data *data;

    if (tradindexed == NULL || tradindexed->index == NULL) {
        warn("tradindexed: overview method not initialized");
        return NULL;
    }
    entry = tdx_index_entry(tradindexed->index, group);
    if (entry == NULL)
        return NULL;
    data = data_cache_open(tradindexed, group, entry);
    if (data == NULL)
        return NULL;
    if (entry->base != data->base)
        if (data->base > (ARTNUM) low && entry->base < data->base) {
            data = data_cache_reopen(tradindexed, group, entry);
            if (data == NULL)
                return NULL;
        }
    return tdx_search_open(data, low, high, entry->high);
}


/*
**  Get the next article returned by a search.  Convert between the multiple
**  pointer API and the structure API we use internally.
*/
bool
tradindexed_search(void *handle, ARTNUM *artnum, char **data, int *length,
                   TOKEN *token, time_t *arrived)
{
    struct article article;

    if (tradindexed == NULL || tradindexed->index == NULL) {
        warn("tradindexed: overview method not initialized");
        return false;
    }
    if (!tdx_search(handle, &article))
        return false;
    if (artnum != NULL)
        *artnum = article.number;
    if (data != NULL)
        *data = (char *) article.overview;
    if (length != NULL)
        *length = article.overlen;
    if (token != NULL)
        *token = article.token;
    if (arrived != NULL)
        *arrived = article.arrived;
    return true;
}


/*
**  Close an overview search.
*/
void
tradindexed_closesearch(void *handle)
{
    tdx_search_close(handle);
}


/*
**  Get information for a single article.  Open the appropriate group and then
**  convert from the pointer API to the struct API used internally.
*/
bool
tradindexed_getartinfo(char *group, ARTNUM artnum, TOKEN *token)
{
    struct group_entry *entry;
    struct group_data *data;
    const struct index_entry *index_entry;

    if (tradindexed == NULL || tradindexed->index == NULL) {
        warn("tradindexed: overview method not initialized");
        return false;
    }
    entry = tdx_index_entry(tradindexed->index, group);
    if (entry == NULL)
        return false;
    data = data_cache_open(tradindexed, group, entry);
    if (data == NULL)
        return false;
    if (entry->base != data->base)
        if (data->base > artnum && entry->base <= artnum) {
            data = data_cache_reopen(tradindexed, group, entry);
            if (data == NULL)
                return false;
        }
    index_entry = tdx_article_entry(data, artnum, entry->high);
    if (index_entry == NULL)
        return false;
    if (token != NULL)
        *token = index_entry->token;
    return true;
}


/*
**  Expire a single newsgroup.
*/
bool
tradindexed_expiregroup(char *group, int *low, struct history *history)
{
    ARTNUM new_low;
    bool status;

    /* tradindexed doesn't have any periodic cleanup. */
    if (group == NULL)
        return true;

    status = tdx_expire(group, &new_low, history);
    if (status && low != NULL)
        *low = (int) new_low;
    return status;
}


/*
**  Set various options or query various paramaters for the overview method.
**  The interface is, at present, not particularly sane.
*/
bool
tradindexed_ctl(OVCTLTYPE type, void *val)
{
    int *i;
    bool *b;
    OVSORTTYPE *sort;

    if (tradindexed == NULL) {
        warn("tradindexed: overview method not initialized");
        return false;
    }

    switch (type) {
    case OVSPACE:
        i = (int *) val;
        *i = -1;
        return true;
    case OVSORT:
        sort = (OVSORTTYPE *) val;
        *sort = OVNEWSGROUP;
        return true;
    case OVCUTOFFLOW:
        b = (bool *) val;
        tradindexed->cutoff = *b;
        return true;
    case OVSTATICSEARCH:
        i = (int *) val;
        *i = false;
        return true;
    case OVCACHEKEEP:
    case OVCACHEFREE:
        b = (bool *) val;
        *b = false;
        return true;
    default:
        return false;
    }
}


/*
**  Close the overview method.
*/
void
tradindexed_close(void)
{
    if (tradindexed != NULL) {
        if (tradindexed->index != NULL)
            tdx_index_close(tradindexed->index);
        if (tradindexed->cache != NULL)
            tdx_cache_free(tradindexed->cache);
        free(tradindexed);
        tradindexed = NULL;
    }
}
