/*  $Id$
**
**  Group index handling for the tradindexed overview method.
**
**  Implements the handling of the group.index file for the tradindexed
**  overview method.  This file contains an entry for every group and stores
**  the high and low article marks and the base article numbers for each
**  individual group index file.
**
**  Externally visible functions have a tdx_ prefix; internal functions do
**  not.  (Externally visible unfortunately means everything that needs to be
**  visible outside of this object file, not just interfaces exported to
**  consumers of the overview API.)
**
**  This code has to support readers and writers sharing the same files, and
**  we want to avoid locking where possible since locking may be very slow
**  (such as over NFS).  Each group has two data files (and one has to get the
**  right index file for a given data file or get mangled results) and one
**  piece of data in the main index file required to interpret the individual
**  index file, namely the article base of that index.
**
**  We can make the following assumptions:
**
**   - The high water mark for a group is monotonically increasing; in other
**     words, the highest numbered article in a group won't ever decrease.
**
**   - While the article base may either increase or decrease, it will never
**     change unless the inode of the index file on disk also changes, since
**     changing the base requires rewriting the index file.
**
**   - No two files will have the same inode (this requirement should be safe
**     even in strange Unix file formats, since the files are all in the same
**     directory).
**
**  We therefore use the following procedure to update the data:  The high
**  water mark may be changed at any time.  The base may only be changed as
**  part of an index rebuild.  To do an index rebuild, we follow the following
**  procedure:
**
**   1) Obtain a write lock on the group entry in the main index.
**   2) Write out new index and data files to new temporary file names.
**   3) Store the new index inode into the main index.
**   4) Update the high, low, and base article numbers in the main index.
**   5) Rename the data file to its correct name.
**   6) Rename the index file to its correct name.
**   7) Release the write lock.
**
**  We use the following procedure to read the data:
**
**   1) Open the group data files (both index and data).
**   2) Store copies of the current high water mark and base in variables.
**   3) Check to be sure the index inode matches the master index file.
**
**  If it does match, then we have a consistent set of data, since the high
**  water mark and base values have to match the index we have (the inode
**  value is updated first).  It may not be the most current set of data, but
**  since we have those index and data files open, even if they're later
**  rebuilt we'll continue looking at the same files.  They may have further
**  data appended to them, but that's safe.
**
**  If the index inode doesn't match, someone's rebuilt the file while we were
**  trying to open it.  Continue with the following procedure:
**
**   4) Close the data files that we opened.
**   5) Obtain a read lock on the group entry in the main index.
**   6) Reopen the data files.
**   7) Grab the current high water mark and base.
**   8) Release the read lock.
**
**  In other words, if there appears to be contention, we fall back to using
**  locking so that we don't try to loop (which also avoids an infinite loop
**  in the event of corruption of the main index).
**
**  Note that once we have a consistent set of data files open, we don't need
**  to aggressively check for new data files until someone asks for an article
**  outside the range of articles that we know about.  We may be working from
**  outdated data files, but the most we'll miss is a cancel or an expiration
**  run.  Overview data doesn't change; new data is appended and old data is
**  expired.  We can afford to check only every once in a while, just to be
**  sure that we're not going to hand out overview data for a bunch of expired
**  articles.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/mmap.h"
#include <fcntl.h>
#include <sys/stat.h>

#include "inn/hashtab.h"
#include "inn/qio.h"
#include "libinn.h"
#include "ov.h"
#include "paths.h"
#include "tdx-private.h"
#include "tdx-structure.h"

#ifndef NFSREADER
# define NFSREADER 0
#endif

/* Returned to callers as an opaque data type, this stashes all of the
   information about an open group.index file. */
struct group_index {
    char *path;
    int fd;
    int mode;
    struct group_header *header;
    struct group_entry *entries;
    int count;
};

/* The value used to mark an invalid or empty location in the group index. */
static const GROUPLOC empty_loc = { -1 };

/* Internal prototypes. */
static int index_entry_count(size_t size);
static size_t index_file_size(int count);
static bool index_lock(int fd, enum inn_locktype type);
static bool index_lock_group(int fd, GROUPLOC loc, enum inn_locktype type);
static bool index_map(struct group_index *);
static bool index_maybe_remap(struct group_index *, GROUPLOC loc);
static bool index_expand(struct group_index *);
static GROUPLOC index_find(struct group_index *, const char *group);


/*
**  Given a file size, return the number of group entries that it contains.
*/
static int
index_entry_count(size_t size)
{
    return (size - sizeof(struct group_header)) / sizeof(struct group_entry);
}


/*
**  Given a number of group entries, return the required file size.
*/
static size_t
index_file_size(int count)
{
    return sizeof(struct group_header) + count * sizeof(struct group_entry);
}


/*
**  Lock the hash table for the group index, used to acquire global locks on
**  the group index when updating it.
*/
static bool
index_lock(int fd, enum inn_locktype type)
{
    return inn_lock_range(fd, type, true, 0, sizeof(struct group_header));
}


/*
**  Lock the group entry for a particular group.  Used when updating and
**  coordinating opening a group when it was just updated.
*/
static bool
index_lock_group(int fd, GROUPLOC group, enum inn_locktype type)
{
    off_t offset = sizeof(struct group_header);

    offset += group.recno * sizeof(struct group_entry);
    return inn_lock_range(fd, type, true, offset, sizeof(struct group_entry));
}


/*
**  Memory map (or read into memory) the key portions of the group.index
**  file.  Takes a struct group_index to fill in and returns true on success
**  and false on failure.
*/
static bool
index_map(struct group_index *index)
{
    if (NFSREADER && (index->mode & OV_WRITE)) {
        warn("tradindexed: cannot open for writing without mmap");
        return false;
    }

    if (NFSREADER) {
        ssize_t header_size;
        ssize_t entry_size;

        header_size = sizeof(struct group_header);
        entry_size = index->count * sizeof(struct group_entry);
        index->header = xmalloc(header_size);
        index->entries = xmalloc(entry_size);
        if (read(index->fd, index->header, header_size) != header_size) {
            syswarn("tradindexed: cannot read header from %s", index->path);
            goto fail;
        }
        if (read(index->fd, index->entries, entry_size) != entry_size) {
            syswarn("tradindexed: cannot read entries from %s", index->path);
            goto fail;
        }
        return true;

    fail:
        free(index->header);
        free(index->entries);
        return false;

    } else {
        char *data;
        size_t size;
        int flag = PROT_READ;

        if (index->mode & OV_WRITE)
            flag = PROT_READ | PROT_WRITE;
        size = index_file_size(index->count);
        data = mmap(NULL, size, flag, MAP_SHARED, index->fd, 0);
        if (data == MAP_FAILED) {
            syswarn("tradindexed: cannot mmap %s", index->path);
            return false;
        }
        index->header = (struct group_header *)(void *) data;
        index->entries = (struct group_entry *)
            (void *)(data + sizeof(struct group_header));
        return true;
    }
}


/*
**  Given a group location, remap the index file if our existing mapping isn't
**  large enough to include that group.  (This can be the case when another
**  writer is appending entries to the group index.)  Not yet implemented.
*/
static bool
index_maybe_remap(struct group_index *index UNUSED, GROUPLOC loc UNUSED)
{
    return true;
}


/*
**  Expand the group.index file to hold more entries; also used to build the
**  initial file.  Not yet implemented.
*/
static bool
index_expand(struct group_index *index UNUSED)
{
    return false;
}


/*
**  Open the group.index file and allocate a new struct for it, returning a
**  pointer to that struct.  Takes the overview open mode, which is some
**  combination of OV_READ and OV_WRITE.
*/
struct group_index *
tdx_index_open(int mode)
{
    struct group_index *index;
    int open_mode;
    struct stat st;

    index = xmalloc(sizeof(struct group_index));
    index->path = concatpath(innconf->pathoverview, "group.index");
    index->mode = mode;
    index->header = NULL;
    open_mode = (mode & OV_WRITE) ? O_RDWR | O_CREAT : O_RDONLY;
    index->fd = open(index->path, open_mode, ARTFILE_MODE);
    if (index->fd < 0) {
        syswarn("tradindexed: cannot open %s", index->path);
        goto fail;
    }

    if (fstat(index->fd, &st) < 0) {
        syswarn("tradindexed: cannot fstat %s", index->path);
        goto fail;
    }
    if ((size_t) st.st_size > sizeof(struct group_header)) {
        index->count = index_entry_count(st.st_size);
        if (!index_map(index))
            goto fail;
    } else {
        index->count = 0;
        if (mode & OV_WRITE) {
            if (!index_expand(index))
                goto fail;
        } else {
            index->count = 0;
            index->header = NULL;
            index->entries = NULL;
        }
    }
    close_on_exec(index->fd, true);
    return index;

 fail:
    tdx_index_close(index);
    return NULL;
}


/*
**  Find a group in the index file, returning the GROUPLOC for that group.
*/
static GROUPLOC
index_find(struct group_index *index, const char *group)
{
    HASH grouphash;
    unsigned int bucket;
    GROUPLOC loc;

    grouphash = Hash(group, strlen(group));
    memcpy(&bucket, &grouphash, sizeof(index));
    loc = index->header->hash[bucket % GROUPHEADERHASHSIZE];
    if (!index_maybe_remap(index, loc))
        return empty_loc;

    while (loc.recno >= 0) {
        struct group_entry *entry;

        entry = index->entries + loc.recno;
        if (entry->deleted == 0)
            if (memcmp(&grouphash, &entry->hash, sizeof(grouphash)) == 0)
                return loc;
        loc = entry->next;
    }
    return empty_loc;
}


/*
**  Return the information stored about a given group in the group index.
*/
const struct group_entry *
tdx_index_entry(struct group_index *index, const char *group)
{
    GROUPLOC loc;

    loc = index_find(index, group);
    if (loc.recno == empty_loc.recno)
        return false;
    return index->entries + loc.recno;
}


/*
**  Close an open handle to the group index file, freeing the group_index
**  structure at the same time.  The argument to this function becomes invalid
**  after this call.
*/
void
tdx_index_close(struct group_index *index)
{
    if (index->header != NULL) {
        if (NFSREADER) {
            free(index->header);
            free(index->entries);
        } else {
            size_t count;

            count = index_file_size(index->count);
            if (munmap((void *) index->header, count) < 0)
                syswarn("tradindexed: cannot munmap %s", index->path);
        }
    }
    if (index->fd >= 0)
        close(index->fd);
    free(index->path);
    free(index);
}


/*
**  Open the data files for a particular group.  The interface to this has to
**  be in this file because we have to lock the group and retry if the inode
**  of the opened index file doesn't match the one recorded in the group index
**  file.
*/
struct group_data *
tdx_data_open(struct group_index *index, const char *group)
{
    GROUPLOC loc;
    struct group_entry *entry;
    struct group_data *data;
    ARTNUM high, base;

    loc = index_find(index, group);
    if (loc.recno == empty_loc.recno)
        return NULL;
    entry = &index->entries[loc.recno];

    data = tdx_data_new(group, index->mode & OV_WRITE);

    /* Check to see if the inode of the index file matches.  If it doesn't,
       this probably means that as we were opening the index file, someone
       else rewrote it (either expire or repack).  Obtain a lock and try
       again.  If there's still a mismatch, go with what we get; there's some
       sort of corruption.

       We have to be sure we get a group index entry that corresponds to the
       index file, since otherwise base may be wrong and we'll find the wrong
       overview entries.

       We also need to get a high water mark and base that match the index and
       data files that we opened, but the group index entry can change at any
       time.  So grab local copies and then set the data variables after we've
       successfully opened the data files. */
    if (!tdx_data_open_files(data))
        goto fail;
    high = entry->high;
    base = entry->base;
    if (entry->indexinode != data->indexinode) {
        if (!index_lock_group(index->fd, loc, INN_LOCK_READ))
            syswarn("tradindexed: cannot lock group entry for %s", group);
        if (!tdx_data_open_files(data))
            goto fail;
        if (entry->indexinode != data->indexinode)
            warn("tradindexed: index inode mismatch for %s", group);
        high = entry->high;
        base = entry->base;
        if (!index_lock_group(index->fd, loc, INN_LOCK_UNLOCK))
            syswarn("tradindexed: cannot unlock group entry for %s", group);
    }
    data->high = high;
    data->base = base;
    return data;

 fail:
    tdx_data_close(data);
    return NULL;
}


/*
**  RECOVERY AND AUDITING
**
**  All code below this point is not used in the normal operations of the
**  overview method.  Instead, it's code to dump various data structures or
**  audit them for consistency, used by recovery tools and inspection tools.
*/

/* Holds a newsgroup name and its hash, used to form a hash table mapping
   newsgroup hash values to the actual names. */
struct hashmap {
    HASH hash;
    char *name;
};


/*
**  Hash table functions for the mapping from group hashes to names.
*/
static unsigned long
hashmap_hash(const void *entry)
{
    unsigned long hash;
    const struct hashmap *group = entry;

    memcpy(&hash, &group->hash, sizeof(hash));
    return hash;
}


static const void *
hashmap_key(const void *entry)
{
    return &((const struct hashmap *) entry)->hash;
}


static bool
hashmap_equal(const void *key, const void *entry)
{
    const HASH *first = key;
    const HASH *second;

    second = &((const struct hashmap *) entry)->hash;
    return memcmp(first, second, sizeof(HASH)) == 0;
}


static void
hashmap_delete(void *entry)
{
    struct hashmap *group = entry;

    free(group->name);
    free(group);
}


/*
**  Construct a hash table of group hashes to group names by scanning the
**  active file.  Returns the constructed hash table.
*/
static struct hash *
hashmap_load(void)
{
    struct hash *hash;
    QIOSTATE *active;
    char *activepath, *line, *p;
    struct stat st;
    size_t hash_size;
    struct hashmap *group;
    HASH grouphash;

    activepath = concatpath(innconf->pathdb, _PATH_ACTIVE);
    active = QIOopen(activepath);
    free(activepath);
    if (active == NULL)
        return NULL;
    if (fstat(QIOfileno(active), &st) < 0)
        hash_size = 32 * 1024;
    else
        hash_size = st.st_size / 30;
    hash = hash_create(hash_size, hashmap_hash, hashmap_key, hashmap_equal,
                       hashmap_delete);

    line = QIOread(active);
    while (line != NULL) {
        p = strchr(line, ' ');
        if (p != NULL)
            *p = '\0';
        group = xmalloc(sizeof(struct hashmap));
        group->name = xstrdup(line);
        grouphash = Hash(group->name, strlen(group->name));
        memcpy(&group->hash, &grouphash, sizeof(HASH));
        hash_insert(hash, &group->hash, group);
        line = QIOread(active);
    }
    QIOclose(active);
    return hash;
}


/*
**  Print the stored information about a single group in human-readable form
**  to stdout.  The format is:
**
**    name high low base count flag deleted inode
**
**  all on one line.  Name is passed into this function.
*/
void
tdx_index_print(const char *name, const struct group_entry *entry)
{
    printf("%s %lu %lu %lu %lu %c %lu %lu\n", name, entry->high,
           entry->low, entry->base, (unsigned long) entry->count,
           entry->flag, entry->deleted, (unsigned long) entry->indexinode);
}


/*
**  Dump the complete contents of the group.index file in human-readable form
**  to stdout, one line per group.
*/
void
tdx_index_dump(struct group_index *index)
{
    int bucket;
    GROUPLOC current;
    struct group_entry *entry;
    struct hash *hashmap;
    struct hashmap *group;
    char *name;

    hashmap = hashmap_load();
    for (bucket = 0; bucket < GROUPHEADERHASHSIZE; bucket++) {
        current = index->header->hash[bucket];
        while (current.recno != empty_loc.recno) {
            if (!index_maybe_remap(index, current)) {
                warn("tradindexed: cannot remap %s", index->path);
                return;
            }
            entry = index->entries + current.recno;
            name = NULL;
            if (hashmap != NULL) {
                group = hash_lookup(hashmap, &entry->hash);
                if (group != NULL)
                    name = group->name;
            }
            if (name == NULL)
                name = HashToText(entry->hash);
            tdx_index_print(name, entry);
            current = entry->next;
        }
    }
    if (hashmap != NULL)
        hash_free(hashmap);
}
