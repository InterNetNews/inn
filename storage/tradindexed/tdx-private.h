/*  $Id$
**
**  Private APIs for the tradindexed overview method.
*/

#ifndef INN_TDX_PRIVATE_H
#define INN_TDX_PRIVATE_H 1

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


/* tdx-group.c */

/* Open the group index and return an opaque data structure to use for further
   queries.  The mode should be a combination of OV_READ and OV_WRITE. */ 
struct group_index *tdx_index_open(int mode);

/* Return the stored information about a single newsgroup. */
const struct group_entry *tdx_index_entry(struct group_index *,
                                          const char *group);

/* Print the contents of a single group entry to stdout in human-readable
   form. */
void tdx_index_print(const char *name, const struct group_entry *);

/* Dump the contents of the index file to stdout in human-readable form. */
void tdx_index_dump(struct group_index *);

/* Close the open index file and dispose of the opaque data structure. */
void tdx_index_close(struct group_index *);

/* Open the overview information for a particular group. */
struct group_data *tdx_data_open(struct group_index *, const char *group);


/* tdx-data.c */

/* Create a new group data structure. */
struct group_data *tdx_data_new(const char *group, bool writable);

/* Open the data files for a group. */
bool tdx_data_open_files(struct group_data *);

/* Return the metadata about a particular article in a group. */
const struct index_entry *tdx_article_entry(struct group_data *,
                                            ARTNUM article);

/* Create, perform, and close a search. */
struct search *tdx_search_open(struct group_data *, ARTNUM low, ARTNUM high);
bool tdx_search(struct search *, struct article *);
void tdx_search_close(struct search *);

/* Dump the contents of the index file for a group. */
void tdx_data_index_dump(struct group_data *data);

/* Close the open data files for a group and free the structure. */
void tdx_data_close(struct group_data *);

#endif /* INN_TDX_PRIVATE_H */
