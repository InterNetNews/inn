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
    bool cutoff;
};

/* Global data about the open tradindexed method. */
static struct tradindexed *tradindexed;


/*
**  Open the overview method.
*/
bool
tradindexed_open(int mode)
{
    if (tradindexed != NULL) {
        warn("tradindexed: overview method already open");
        return false;
    }
    tradindexed = xmalloc(sizeof(struct tradindexed));
    tradindexed->index = tdx_index_open(mode);
    tradindexed->cutoff = false;
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
**  API and the structure API used internally.
*/
bool
tradindexed_add(char *group, ARTNUM artnum, TOKEN token, char *data,
                int length, time_t arrived, time_t expires)
{
    struct article article;

    if (tradindexed == NULL || tradindexed->index == NULL) {
        warn("tradindexed: overview method not initialized");
        return false;
    }
    article.number = artnum;
    article.overview = data;
    article.overlen = length;
    article.token = token;
    article.arrived = arrived;
    article.expires = expires;
    return tdx_data_add(tradindexed->index, group, &article);
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
    struct group_data *data;

    if (tradindexed == NULL || tradindexed->index == NULL) {
        warn("tradindexed: overview method not initialized");
        return NULL;
    }
    data = tdx_data_open(tradindexed->index, group, NULL);
    if (data == NULL)
        return NULL;
    return tdx_search_open(data, low, high);
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
    struct group_data *data;
    const struct index_entry *entry;

    if (tradindexed == NULL || tradindexed->index == NULL) {
        warn("tradindexed: overview method not initialized");
        return false;
    }
    data = tdx_data_open(tradindexed->index, group, NULL);
    if (data == NULL)
        return false;
    entry = tdx_article_entry(data, artnum);
    if (entry == NULL) {
        tdx_data_close(data);
        return false;
    }
    if (token != NULL)
        *token = entry->token;
    tdx_data_close(data);
    return true;
}


/*
**  Expire a single newsgroup.  Not yet implemented.
*/
bool
tradindexed_expiregroup(char *group UNUSED, int *low UNUSED,
                        struct history *history UNUSED)
{
    return false;
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
        return TRUE;
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
        free(tradindexed);
        tradindexed = NULL;
    }
}
