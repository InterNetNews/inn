/*  $Id$
**
**  Overview API for storing and retrieving overview information.
**
**  This header defines the overview API, which consists of a set of functions
**  that dispatch to the appropriate overview backend based on the inn.conf
**  configuration, along with some additional utility functions for
**  manipulating overview information.
*/

#ifndef INN_OVERVIEW_H
#define INN_OVERVIEW_H 1

#include <stdio.h>              /* FILE */

#include "storage.h"

/* Forward declarations to avoid unnecessary includes. */
struct history;
struct vector;

/* Offsets into vectors for standard overview headers. */
enum {
    OVERVIEW_SUBJECT = 0,
    OVERVIEW_FROM,
    OVERVIEW_DATE,
    OVERVIEW_MESSAGE_ID,
    OVERVIEW_REFERENCES,
    OVERVIEW_BYTES,
    OVERVIEW_LINES,
    OVERVIEW_MAX
};

/* Opaque struct used to hold internal overview API status. */
struct overview;

/* The statistics information for one newsgroup. */
struct overview_group {
    ARTNUM high;
    ARTNUM low;
    ARTNUM count;
    char flag;
};

/* The overview data for one article. */
struct overview_data {
    ARTNUM number;
    const char *overview;
    size_t overlen;
    TOKEN token;
    time_t arrived;
    time_t expires;
};

/* General configuration for and information about the overview method. */
struct overview_config {
    int mode;                   /* The mode passed to overview_open. */
    bool sorted;                /* Set if the overview method will be faster
                                   when given data sorted by newsgroup, used
                                   by overview rebuilds. */
    bool persistant;            /* Set if overview search results are usable
                                   beyond the next call to overview_search, as
                                   long as overview_search_close hasn't been
                                   called. */
    bool cutoff;                /* Ignore overview information for articles
                                   below the current low water mark. */
};

/* Configuration for overview-driven expiration.  The caller fleshes out this
   struct with appropriate options and then passes it into the expire
   calls, which fill in the statistics portion. */
struct overview_expire {
    FILE *rmfile;               /* If not NULL, append files or tokens to
                                   remove to this file rather than deleting
                                   them immediately. */
    time_t now;                 /* Used as the current time for expiration. */
    bool usepost;               /* Use posting date to determine expire. */
    bool quiet;                 /* Suppress statistics output. */
    bool keepcross;             /* Keep article so long as it hasn't expired
                                   from any newsgroup to which it's
                                   crossposted. */
    bool purgecross;            /* Purge article as soon as it expires from
                                   any crossposted newsgroup. */
    bool ignoreselfexpire;      /* Purge article even if the storage method
                                   is self-expiring. */
    bool statall;               /* Stat all articles when expiring. */
    struct history *history;    /* History interface, used to check to see if
                                   the article is still present in the spoool
                                   in the non-group-based-expiry case. */

    /* Statistics.  The following should be initialized by the caller to zero
       and will be incremented when calling overview_expire. */
    long processed;             /* Articles processed. */
    long dropped;               /* Articles deleted. */
    long indexdropped;          /* Overview entries deleted. */
};

/* Flags passed to overview_open. */
#define OV_READ         1
#define OV_WRITE        2

BEGIN_DECLS

/* Open or close the overview API.  overview_open takes some combination of
   OV_READ and OV_WRITE flags.  The resulting opaque struct overview * pointer
   should be passed into all other overview API calls. */
struct overview *overview_open(int mode);
void overview_close(struct overview *);

/* Retrieve high and low water marks, article count, and flag for a group. */
bool overview_group(struct overview *, const char *, struct overview_group *);

/* Add a new newsgroup to the overview database.  This must be called before
   storing any articles in that newsgroup.  The count in the provided struct
   is ignored. */
bool overview_group_add(struct overview *, const char *,
                        struct overview_group *);

/* Delete a newsgroup from overview.  This generally will purge all of the
   overview data for that group immediately. */
bool overview_group_delete(struct overview *, const char *);

/* Add data for an article.  This must be called for every group and article
   number combination at which the article is stored. */
bool overview_add(struct overview *, const char *, struct overview_data *);

/* Add data for an article, using the provided Xref information (without the
   leading hostname) to determine which groups and article numbers.  The data
   will be added to each group and article number combination listed.  Returns
   true only if the overview was successfully stored in every group.  There is
   no way to tell which group failed. */
bool overview_add_xref(struct overview *, const char *xref,
                       struct overview_data *);

/* Cancel the overview data for an article (make it inaccessible to searches).
   Unfortunately, most callers will have to use the _xref interface. */
bool overview_cancel(struct overview *, const char *group, ARTNUM);

/* Cancel the overview data for an article from all groups, based on Xref
   information.  This retrieves the article to find the groups and article
   numbers (ew!), so call this before deleting the article out of the storage
   API. */
bool overview_cancel_xref(struct overview *, TOKEN token);

/* Used to retrieve overview data.  Even when just retrieving a single record,
   a caller must call overview_search_open and then overview_search.  The data
   returned by overview_search may be invalidated by the next call to that
   function unless overview_config returns persistant as true.  When done with
   the search, overview_search_close will discard the search handle. */
void *overview_search_open(struct overview *, const char *group, ARTNUM low,
                           ARTNUM high);
bool overview_search(struct overview *, void *handle, struct overview_data *);
void overview_search_close(struct overview *, void *handle);

/* Given the group and article number, retrieve the storage token for that
   article from the overview data. */
bool overview_token(struct overview *, const char *group, ARTNUM, TOKEN *);

/* Expire overview for a particular group.  Returns in low the new low water
   mark for the group.  overview_expire holds the configuration information
   for the expiration.  Takes an open history struct, which is used to see if
   the article is still present in the spool unless groupbasedexpiry is set to
   true. */
bool overview_expire(struct overview *, const char *group, ARTNUM *low,
                     struct overview_expire *);

/* Get or set the configuration for the overview method.  The only thing that
   can be changed at present by overview_config_set is cutoff. */
void overview_config_get(struct overview *, struct overview_config *);
bool overview_config_set(struct overview *, struct overview_config *);

/* Returns the free space of the overview method as a percentage, or -1 if
   that concept isn't meaningful for the overview method.  Currently, this is
   only useful for buffindexed. */
float overview_free_space(struct overview *);

/* Overview data manipulation functions. */
const struct cvector *overview_fields(void);
struct vector *overview_extra_fields(void);
struct buffer *overview_build(ARTNUM number, const char *article,
                              size_t length, const struct vector *extra,
                              struct buffer *);
bool overview_check(const char *data, size_t length, ARTNUM article);
int overview_index(const char *field, const struct vector *extra);
struct cvector *overview_split(const char *line, size_t length,
			       ARTNUM *number, struct cvector *vector);
char *overview_getheader(const struct cvector *vector, unsigned int element,
			 const struct vector *extra);

END_DECLS

#endif /* !INN_OVERVIEW_H */
