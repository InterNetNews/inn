/*  $Id$
**
**  Private APIs for the tradindexed overview method.
*/

#ifndef INN_TDX_PRIVATE_H
#define INN_TDX_PRIVATE_H 1

/* Opaque data structure returned by group index functions. */
struct group_index;

/* Open the group index and return an opaque data structure to use for further
   queries.  The mode should be a combination of OV_READ and OV_WRITE. */ 
struct group_index *group_index_open(int mode);

/* Return the stored information about a single newsgroup. */
bool group_index_info(struct group_index *, const char *group,
                      unsigned long *high, unsigned long *low,
                      unsigned long *count, char *flag);

/* Dump the contents of the index file to stdout in human-readable form. */
void group_index_dump(struct group_index *);

/* Close the open index file and dispose of the opaque data structure. */
void group_index_close(struct group_index *);

#endif /* INN_TDX_PRIVATE_H */
