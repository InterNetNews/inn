/*  $Id$
**
**  Data structures for the tradindexed overview method.
**
**  This header defines the data structures used by the tradindexed overview
**  method.  Currently, these data structures are read and written directly to
**  disk (and the disk files are therefore endian-dependent and possibly
**  architecture-dependent due to structure padding).  This will eventually be
**  fixed.
**
**  The structure of a tradindexed overview spool is as follows:  At the root
**  of the spool is a group.index file composed of a struct group_header
**  followed by some number of struct group_entry's, one for each group plus
**  possibly some number of free entries linked to a free list that's headed
**  in the struct index_header.  Each entry corresponds to a particular
**  newsgroup carried by the server and stores the high and low article
**  numbers for that group, its status flag, and the base of the index file
**  for each group.
**
**  The storage of the group.index file implements a hash table with chaining;
**  in other words, there is a table indexed by hash value stored in the
**  header that points to the starts of the chains, new entries are appended
**  to the end of the file and added to the hash table, and if they collide
**  with an existing entry are instead linked to the appropriate hash chain.
**
**  The overview information for each group is stored in a pair of files named
**  <group>.IDX and <group>.DAT.  These files are found in a subdirectory
**  formed by taking the first letter of component of the newsgroup name as
**  a directory name; in other words, news.announce.newgroups overview data is
**  stored in <pathoverview>/n/a/n/news.announce.newgroups.{IDX,DAT}.  The
**  .DAT file contains the individual overview entries, one per line, stored
**  in wire format (in other words, suitable for dumping directly across the
**  network to a client in response to an XOVER command).  The overview data
**  stored in that file may be out of order.
**
**  The .IDX file consists of a series of struct index_entry's, one for each
**  overview entry stored in the .DAT file.  Each index entry stores the
**  offset of the data for one article in the .DAT file and its length, along
**  with some additional metainformation about the article used to drive
**  article expiration.  The .IDX file is addressed like an array; the first
**  entry corresponds to the article with the number stored in the base field
**  of the group_entry for that newsgroup in the group.index file and each
**  entry stores the data for the next consecutive article.  Index entries may
**  be tagged as deleted if that article has been deleted or expired.
*/

#ifndef INN_TDX_STRUCTURE_H
#define INN_TDX_STRUCTURE_H 1

#include "config.h"
#include <sys/types.h>

#include "libinn.h"
#include "storage.h"

/* A location in group.index (this many records past the end of the header of
   the file).  There's no reason for this to be a struct, but that can't be
   changed until the format of the group.index file is changed to be
   architecture-independent since putting it into a struct may have changed
   the alignment or padding on some architectures. */
typedef struct {
    int                 recno;             /* Record number in group index */
} GROUPLOC;

/* The hard-coded constant size of the hash table for group.index.  This need
   not be a power of two and has no special constraints.  Changing this at
   present will break backward compatibility with group.index files written by
   previous versions of the code. */
#define GROUPHEADERHASHSIZE (16 * 1024)

/* A magic number for the group.index file so that we can later change the
   format in a backward-compatible fashion. */
#define GROUPHEADERMAGIC    (~(0xf1f0f33d))

/* The header at the top of group.index.  magic contains GROUPHEADERMAGIC
   always; hash contains pointers to the heads of the entry chains, and
   freelist points to a linked list of free entries (entries that were used
   for groups that have since been deleted). */
struct group_header {
    int         magic;
    GROUPLOC    hash[GROUPHEADERHASHSIZE];
    GROUPLOC    freelist;
};

/* An entry for a particular group.  Note that a good bit of active file
   information is duplicated here, and depending on the portion of INN asking
   questions, sometimes the main active file is canonical and sometimes the
   overview data is canonical.  This needs to be rethought at some point.

   Groups are matched based on the MD5 hash of their name.  This may prove
   inadequate in the future.  Ideally, INN really needs to assign unique
   numbers to each group, which could then be used here as well as in
   tradspool rather than having to do hacks like using a hash of the group
   name or constructing one's own number to name mapping like tradspool does.
   Unfortunately, this ideally requires a non-backward-compatible change to
   the active file format.

   Several of these elements aren't used.  This structure, like the others,
   cannot be changed until the whole format of the group.index file is changed
   since it's currently read as binary structs directly from disk. */
struct group_entry {
    HASH        hash;           /* MD5 hash of the group name. */
    HASH        alias;          /* Intended to point to the group this group
                                   is an alias for.  Not currently used. */
    ARTNUM      high;           /* High article number in the group. */
    ARTNUM      low;            /* Low article number in the group. */
    ARTNUM      base;           /* Article number of the first entry in the
                                   .IDX index file for the group. */
    int         count;          /* Number of articles in group. */
    int         flag;           /* Posting/moderation status. */
    time_t      deleted;        /* When this group was deleted, or 0 if the
                                   group is still valid. */    
    ino_t       indexinode;     /* The inode of the index file for the group,
                                   used to detect when the file has been
                                   recreated and swapped out. */
    GROUPLOC    next;           /* Next block in this chain. */
};

/* An entry in the per-group .IDX index file. */
struct index_entry {
    off_t       offset;
    int         length;
    time_t      arrived;
    time_t      expires;        /* Expiration time from Expires: header. */
    TOKEN       token;
};

#endif /* INN_TDX_STRUCTURE_H */
