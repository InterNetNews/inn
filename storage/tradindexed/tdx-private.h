/*  $Id$
**
**  Private APIs for the tradindexed overview method.
*/

#ifndef INN_TDX_PRIVATE_H
#define INN_TDX_PRIVATE_H 1

#include "config.h"
#include <stdio.h>
#include <sys/types.h>

#include "libinn.h"
#include "storage.h"

/* Forward declarations to avoid unnecessary includes. */
struct history;

/* Opaque data structure used by the cache. */
struct cache;

/* Opaque data structure returned by group index functions. */
struct group_index;

/* Opaque data structure returned by search functions. */
struct search;

/* All of the information about an open set of group data files. */
struct group_data {
    char *path;
    bool writable;
    ARTNUM high;
    ARTNUM base;
    int indexfd;
    int datafd;
    struct index_entry *index;
    char *data;
    off_t indexlen;
    off_t datalen;
    ino_t indexinode;
    int refcount;
};

/* All of the data about an article, used as the return of the search
   functions.  This is just cleaner than passing back all of the information
   that's used by the regular interface. */
struct article {
    ARTNUM number;
    const char *overview;
    size_t overlen;
    TOKEN token;
    time_t arrived;
    time_t expires;
};

BEGIN_DECLS

/* tdx-cache.c */

/* Create a new cache with the given number of entries. */
struct cache *tdx_cache_create(unsigned int size);

/* Look up a given newsgroup hash in the cache, returning the group_data
   struct for its open data files if present. */
struct group_data *tdx_cache_lookup(struct cache *, HASH);

/* Insert a new group_data struct into the cache. */
void tdx_cache_insert(struct cache *, HASH, struct group_data *);

/* Delete a group entry from the cache. */
void tdx_cache_delete(struct cache *, HASH);

/* Free the cache and its resources. */
void tdx_cache_free(struct cache *);


/* tdx-group.c */

/* Open the group index and return an opaque data structure to use for further
   queries. */
struct group_index *tdx_index_open(bool writable);

/* Return the stored information about a single newsgroup. */
struct group_entry *tdx_index_entry(struct group_index *, const char *group);

/* Print the contents of a single group entry in human-readable form. */
void tdx_index_print(const char *name, const struct group_entry *, FILE *);

/* Add a new newsgroup to the index file. */
bool tdx_index_add(struct group_index *, const char *group, ARTNUM low,
                   ARTNUM high, const char *flag);

/* Delete a newsgroup from the index file. */
bool tdx_index_delete(struct group_index *, const char *group);

/* Dump the contents of the index file to stdout in human-readable form. */
void tdx_index_dump(struct group_index *, FILE *);

/* Close the open index file and dispose of the opaque data structure. */
void tdx_index_close(struct group_index *);

/* Open the overview information for a particular group. */
struct group_data *tdx_data_open(struct group_index *, const char *group,
                                 struct group_entry *);

/* Add a new overview entry. */
bool tdx_data_add(struct group_index *, struct group_entry *,
                  struct group_data *, const struct article *);

/* Expire a single group. */
bool tdx_expire(const char *group, ARTNUM *low, struct history *);


/* tdx-data.c */

/* Create a new group data structure. */
struct group_data *tdx_data_new(const char *group, bool writable);

/* Open the data files for a group. */
bool tdx_data_open_files(struct group_data *);

/* Return the metadata about a particular article in a group. */
const struct index_entry *tdx_article_entry(struct group_data *,
                                            ARTNUM article, ARTNUM high);

/* Create, perform, and close a search. */
struct search *tdx_search_open(struct group_data *, ARTNUM start, ARTNUM end,
                               ARTNUM high);
bool tdx_search(struct search *, struct article *);
void tdx_search_close(struct search *);

/* Store article data. */
bool tdx_data_store(struct group_data *, const struct article *);

/* Start a repack of the files for a newsgroup. */
bool tdx_data_pack_start(struct group_data *, ARTNUM);

/* Complete a repack of the files for a newsgroup. */
bool tdx_data_pack_finish(struct group_data *);

/* Start the expiration of a newsgroup and do most of the work, filling out
   the provided group_entry struct. */
bool tdx_data_expire_start(const char *group, struct group_data *,
                           struct group_entry *, struct history *);

/* Complete the expiration of a newsgroup. */
bool tdx_data_expire_finish(const char *group);

/* Dump the contents of the index file for a group. */
void tdx_data_index_dump(struct group_data *, FILE *);

/* Close the open data files for a group and free the structure. */
void tdx_data_close(struct group_data *);

/* Delete the data files for a group. */
void tdx_data_delete(const char *group, const char *suffix);

END_DECLS

#endif /* INN_TDX_PRIVATE_H */
