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
**  water mark may be changed at any time but surrounded in a write lock.  The
**  base may only be changed as part of an index rebuild.  To do an index
**  rebuild, we follow the following procedure:
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
#include <time.h>

#include "inn/hashtab.h"
#include "inn/messages.h"
#include "inn/qio.h"
#include "libinn.h"
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
    bool writable;
    struct group_header *header;
    struct group_entry *entries;
    int count;
};

/* Internal prototypes. */
static int index_entry_count(size_t size);
static size_t index_file_size(int count);
static bool index_lock(int fd, enum inn_locktype type);
static bool index_lock_group(int fd, ptrdiff_t offset, enum inn_locktype);
static bool index_map(struct group_index *);
static bool index_maybe_remap(struct group_index *, long loc);
static void index_unmap(struct group_index *);
static bool index_expand(struct group_index *);
static unsigned int index_bucket(HASH hash);
static long index_find(struct group_index *, const char *group);
static long index_splice(struct group_index *, const char *group);


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
    bool status;

    status = inn_lock_range(fd, type, true, 0, sizeof(struct group_header));
    if (!status)
        syswarn("tradindexed: cannot %s index hash table",
                (type == INN_LOCK_UNLOCK) ? "unlock" : "lock");
    return status;
}


/*
**  Lock the group entry for a particular group.  Takes the offset of that
**  group entry from the start of the group entries (not the start of the
**  file; we have to add the size of the group header).  Used for coordinating
**  updates of the data for a group.
*/
static bool
index_lock_group(int fd, ptrdiff_t offset, enum inn_locktype type)
{
    bool status;
    size_t size;

    size = sizeof(struct group_entry);
    offset += sizeof(struct group_header);
    status = inn_lock_range(fd, type, true, offset, size);
    if (!status)
        syswarn("tradindexed: cannot %s group entry at %lu",
                (type == INN_LOCK_UNLOCK) ? "unlock" : "lock",
                (unsigned long) offset);
    return status;
}


/*
**  Memory map (or read into memory) the key portions of the group.index
**  file.  Takes a struct group_index to fill in and returns true on success
**  and false on failure.
*/
static bool
index_map(struct group_index *index)
{
    if (NFSREADER && index->writable) {
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
        index->header = NULL;
        index->entries = NULL;
        return false;

    } else {
        char *data;
        size_t size;
        int flag = PROT_READ;

        if (index->writable)
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
**  writer is appending entries to the group index.)
*/
static bool
index_maybe_remap(struct group_index *index, long loc)
{
    struct stat st;
    int count;

    if (loc < index->count)
        return true;

    /* Don't remap if remapping wouldn't actually help. */
    if (fstat(index->fd, &st) < 0) {
        syswarn("tradindexed: cannot stat %s", index->path);
        return false;
    }
    count = index_entry_count(st.st_size);
    if (count < loc)
        return true;

    /* Okay, remapping will actually help. */
    index_unmap(index);
    index->count = count;
    return index_map(index);
}


/*
**  Unmap the index file, either in preparation for closing the overview
**  method or to get ready to remap it.  We warn about failures to munmap but
**  don't do anything about them; there isn't much that we can do.
*/
static void
index_unmap(struct group_index *index)
{
    if (index->header == NULL)
        return;
    if (NFSREADER) {
        free(index->header);
        free(index->entries);
    } else {
        if (munmap(index->header, index_file_size(index->count)) < 0)
            syswarn("tradindexed: cannot munmap %s", index->path);
    }
    index->header = NULL;
    index->entries = NULL;
}


/*
**  Expand the group.index file to hold more entries; also used to build the
**  initial file.  The caller is expected to lock the group index.
*/
static bool
index_expand(struct group_index *index)
{
    int i;

    index_unmap(index);
    index->count += 1024;
    if (ftruncate(index->fd, index_file_size(index->count)) < 0) {
        syswarn("tradindexed: cannot expand %s", index->path);
        return false;
    }
    if (!index_map(index))
        return false;

    /* If the magic isn't right, assume this is a new index file. */
    if (index->header->magic != TDX_MAGIC) {
        index->header->magic = TDX_MAGIC;
        index->header->freelist.recno = -1;
        for (i = 0; i < TDX_HASH_SIZE; i++)
            index->header->hash[i].recno = -1;
    }

    /* Walk the new entries back to front, adding them to the free list. */
    for (i = index->count - 1; i >= index->count - 1024; i--) {
        index->entries[i].next = index->header->freelist;
        index->header->freelist.recno = i;
    }

    msync(index->header, index_file_size(index->count), MS_ASYNC);
    return true;
}


/*
**  Open the group.index file and allocate a new struct for it, returning a
**  pointer to that struct.  Takes a bool saying whether or not the overview
**  should be opened for write.
*/
struct group_index *
tdx_index_open(bool writable)
{
    struct group_index *index;
    int open_mode;
    struct stat st;

    index = xmalloc(sizeof(struct group_index));
    index->path = concatpath(innconf->pathoverview, "group.index");
    index->writable = writable;
    index->header = NULL;
    open_mode = index->writable ? O_RDWR | O_CREAT : O_RDONLY;
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
        if (index->writable) {
            if (!index_expand(index))
                goto fail;
        } else {
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
**  Given a group name hash, return an index into the hash table in the
**  group.index header.
*/
static unsigned int
index_bucket(HASH hash)
{
    unsigned int bucket;

    memcpy(&bucket, &hash, sizeof(bucket));
    return bucket % TDX_HASH_SIZE;
}


/*
**  Find a group in the index file, returning the group number for that group
**  or -1 if the group can't be found.
*/
static long
index_find(struct group_index *index, const char *group)
{
    HASH hash;
    long loc;

    if (index->header == NULL || index->entries == NULL)
        return -1;
    hash = Hash(group, strlen(group));
    loc = index->header->hash[index_bucket(hash)].recno;

    while (loc >= 0) {
        struct group_entry *entry;

        if (loc > index->count && !index_maybe_remap(index, loc))
            return -1;
        entry = index->entries + loc;
        if (entry->deleted == 0)
            if (memcmp(&hash, &entry->hash, sizeof(hash)) == 0)
                return loc;
        loc = entry->next.recno;
    }
    return -1;
}


/*
**  Like index_find, but also splices that entry out of whatever chain it
**  might belong to.  This function is called by tdx_index_delete.  No locking
**  is at present done.
*/
static long
index_splice(struct group_index *index, const char *group)
{
    HASH hash;
    struct loc *parent;
    long loc;

    if (!index->writable)
        return -1;
    if (index->header == NULL || index->entries == NULL)
        return -1;
    hash = Hash(group, strlen(group));
    parent = &index->header->hash[index_bucket(hash)];
    loc = parent->recno;

    while (loc >= 0) {
        struct group_entry *entry;

        if (loc > index->count && !index_maybe_remap(index, loc))
            return -1;
        entry = index->entries + loc;
        if (entry->deleted == 0)
            if (memcmp(&hash, &entry->hash, sizeof(hash)) == 0) {
                *parent = entry->next;
                entry->next.recno = -1;
                msync(parent, sizeof(*parent), MS_ASYNC);
                return loc;
            }
        parent = &entry->next;
        loc = entry->next.recno;
    }
    return -1;
}


/*
**  Return the information stored about a given group in the group index.
*/
struct group_entry *
tdx_index_entry(struct group_index *index, const char *group)
{
    long loc;

    loc = index_find(index, group);
    if (loc == -1)
        return NULL;
    return index->entries + loc;
}


/*
**  Add a new newsgroup to the group.index file.  Takes the newsgroup name,
**  its high and low water marks, and the newsgroup flag.  Note that aliased
**  newsgroups are not currently handled.  If the group already exists, just
**  update the flag (not the high and low water marks).
*/
bool
tdx_index_add(struct group_index *index, const char *group, ARTNUM low,
              ARTNUM high, const char *flag)
{
    HASH hash;
    unsigned int bucket;
    long loc;
    struct group_entry *entry;
    struct group_data *data;

    if (!index->writable)
        return false;

    /* If the group already exists, update the flag as necessary and then
       we're all done. */
    loc = index_find(index, group);
    if (loc != -1) {
        entry = &index->entries[loc];
        if (entry->flag != *flag) {
            entry->flag = *flag;
            msync(entry, sizeof(*entry), MS_ASYNC);
        }
        return true;
    }

    index_lock(index->fd, INN_LOCK_WRITE);

    /* Find a free entry.  If we don't have any free space, make some. */
    if (index->header->freelist.recno == -1)
        if (!index_expand(index)) {
            index_lock(index->fd, INN_LOCK_UNLOCK);
            return false;
        }
    loc = index->header->freelist.recno;
    index->header->freelist = index->entries[loc].next;

    /* Initialize the entry. */
    entry = &index->entries[loc];
    hash = Hash(group, strlen(group));
    entry->hash = hash;
    entry->low = low;
    entry->high = high;
    entry->deleted = 0;
    entry->base = 0;
    entry->count = 0;
    entry->flag = *flag;
    bucket = index_bucket(hash);
    entry->next = index->header->hash[bucket];
    index->header->hash[bucket].recno = loc;

    /* Create the data files and initialize the index inode. */
    data = tdx_data_new(group, index->writable);
    if (!tdx_data_open_files(data))
        warn("tradindexed: unable to create data files for %s", group);
    entry->indexinode = data->indexinode;
    tdx_data_close(data);

    index_lock(index->fd, INN_LOCK_UNLOCK);
    msync(index->header, sizeof(struct group_header), MS_ASYNC);
    msync(entry, sizeof(*entry), MS_ASYNC);

    return true;
}


/*
**  Delete a group index entry.
*/
bool
tdx_index_delete(struct group_index *index, const char *group)
{
    long loc;
    struct group_entry *entry;

    if (!index->writable)
        return false;

    loc = index_splice(index, group);
    if (loc == -1)
        return false;

    entry = &index->entries[loc];
    entry->deleted = time(NULL);
    HashClear(&entry->hash);

    /* The only thing we have to lock is the modification of the free list. */
    index_lock(index->fd, INN_LOCK_WRITE);
    entry->next = index->header->freelist;
    index->header->freelist.recno = loc;
    index_lock(index->fd, INN_LOCK_UNLOCK);
    msync(index->header, sizeof(struct group_header), MS_ASYNC);
    msync(entry, sizeof(*entry), MS_ASYNC);

    /* Delete the group data files for this group. */
    tdx_data_delete(group, NULL);

    return true;
}


/*
**  Close an open handle to the group index file, freeing the group_index
**  structure at the same time.  The argument to this function becomes invalid
**  after this call.
*/
void
tdx_index_close(struct group_index *index)
{
    index_unmap(index);
    if (index->fd >= 0)
        close(index->fd);
    free(index->path);
    free(index);
}


/*
**  Open the data files for a particular group.  The interface to this has to
**  be in this file because we have to lock the group and retry if the inode
**  of the opened index file doesn't match the one recorded in the group index
**  file.  Optionally take a pointer to the group index entry if the caller
**  has already gone to the work of finding it.
*/
struct group_data *
tdx_data_open(struct group_index *index, const char *group,
              struct group_entry *entry)
{
    struct group_data *data;
    ARTNUM high, base;
    ptrdiff_t offset;

    if (entry == NULL) {
        entry = tdx_index_entry(index, group);
        if (entry == NULL)
            return NULL;
    }
    offset = entry - index->entries;
    data = tdx_data_new(group, index->writable);

    /* Check to see if the inode of the index file matches.  If it doesn't,
       this probably means that as we were opening the index file, someone
       else rewrote it (either expire or repack).  Obtain a lock and try
       again.  If there's still a mismatch, go with what we get; there's some
       sort of corruption.

       This code is very sensitive to order and parallelism.  See the comment
       at the beginning of this file for methodology. */
    if (!tdx_data_open_files(data))
        goto fail;
    high = entry->high;
    base = entry->base;
    if (entry->indexinode != data->indexinode) {
        index_lock_group(index->fd, offset, INN_LOCK_READ);
        if (!tdx_data_open_files(data)) {
            index_lock_group(index->fd, offset, INN_LOCK_UNLOCK);
            goto fail;
        }
        if (entry->indexinode != data->indexinode)
            warn("tradindexed: index inode mismatch for %s", group);
        high = entry->high;
        base = entry->base;
        index_lock_group(index->fd, offset, INN_LOCK_UNLOCK);
    }
    data->high = high;
    data->base = base;
    return data;

 fail:
    tdx_data_close(data);
    return NULL;
}


/*
**  Add an overview record for a particular article.  Takes the group entry,
**  the open overview data structure, and the information about the article
**  and returns true on success, false on failure.  This function calls
**  tdx_data_store to do most of the real work and then updates the index
**  information.
*/
bool
tdx_data_add(struct group_index *index, struct group_entry *entry,
             struct group_data *data, const struct article *article)
{
    ARTNUM old_base;
    ino_t old_inode;
    ptrdiff_t offset = entry - index->entries;

    if (!index->writable)
        return false;
    index_lock_group(index->fd, offset, INN_LOCK_WRITE);

    /* Make sure we have the most current data files. */
    if (entry->indexinode != data->indexinode) {
        if (!tdx_data_open_files(data))
            goto fail;
        if (entry->indexinode != data->indexinode)
            warn("tradindexed: index inode mismatch for %s",
                 HashToText(entry->hash));
    }

    /* If the article number is too low to store in the group index, repack
       the group with a lower base index. */
    if (entry->base > article->number) {
        if (!tdx_data_pack_start(data, article->number))
            goto fail;
        old_inode = entry->indexinode;
        old_base = entry->base;
        entry->indexinode = data->indexinode;
        entry->base = data->base;
        msync(entry, sizeof(*entry), MS_ASYNC);
        if (!tdx_data_pack_finish(data)) {
            entry->base = old_base;
            entry->indexinode = old_inode;
            msync(entry, sizeof(*entry), MS_ASYNC);
            goto fail;
        }
    }

    /* Store the data. */
    if (!tdx_data_store(data, article))
        goto fail;
    if (entry->base == 0)
        entry->base = data->base;
    if (entry->low == 0 || entry->low > article->number)
        entry->low = article->number;
    if (entry->high < article->number)
        entry->high = article->number;
    entry->count++;
    msync(entry, sizeof(*entry), MS_ASYNC);
    index_lock_group(index->fd, offset, INN_LOCK_UNLOCK);
    return true;

 fail:
    index_lock_group(index->fd, offset, INN_LOCK_UNLOCK);
    return false;
}


/*
**  Expire a single newsgroup.  Most of the work is done by tdx_data_expire*,
**  but this routine has the responsibility to do locking (the same as would
**  be done for repacking, since the group base may change) and updating the
**  group entry.
*/
bool
tdx_expire(const char *group, ARTNUM *low, struct history *history)
{
    struct group_index *index;
    struct group_entry *entry;
    struct group_entry new_entry;
    struct group_data *data = NULL;
    ptrdiff_t offset;
    ARTNUM old_base;
    ino_t old_inode;

    index = tdx_index_open(true);
    if (index == NULL)
        return false;
    entry = tdx_index_entry(index, group);
    if (entry == NULL) {
        tdx_index_close(index);
        return false;
    }
    offset = entry - index->entries;
    index_lock_group(index->fd, offset, INN_LOCK_WRITE);

    /* tdx_data_expire_start builds the new IDX and DAT files and fills in the
       struct group_entry that was passed to it.  tdx_data_expire_finish does
       the renaming of the new files to the final file names. */
    new_entry = *entry;
    new_entry.low = 0;
    new_entry.count = 0;
    new_entry.base = 0;
    data = tdx_data_open(index, group, entry);
    if (data == NULL)
        goto fail;
    if (!tdx_data_expire_start(group, data, &new_entry, history))
        goto fail;
    old_inode = entry->indexinode;
    old_base = entry->base;
    entry->indexinode = new_entry.indexinode;
    entry->base = new_entry.base;
    msync(entry, sizeof(*entry), MS_ASYNC);
    if (!tdx_data_expire_finish(group)) {
        entry->base = old_base;
        entry->indexinode = old_inode;
        msync(entry, sizeof(*entry), MS_ASYNC);
        goto fail;
    }

    /* Almost done.  Update the group index. */
    if (new_entry.low == 0)
        new_entry.low = new_entry.high;
    *entry = new_entry;
    msync(entry, sizeof(*entry), MS_ASYNC);
    index_lock_group(index->fd, offset, INN_LOCK_UNLOCK);

    /* Return the lowmark to our caller.  If there are no articles in the
       group, this should be one more than the high water mark. */
    if (low != NULL) {
        if (entry->count == 0)
            *low = entry->high + 1;
        else
            *low = entry->low;
    }
    tdx_index_close(index);
    return true;

 fail:
    index_lock_group(index->fd, offset, INN_LOCK_UNLOCK);
    if (data != NULL)
        tdx_data_close(data);
    tdx_index_close(index);
    return false;
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
tdx_index_print(const char *name, const struct group_entry *entry,
                FILE *output)
{
    fprintf(output, "%s %lu %lu %lu %lu %c %lu %lu\n", name, entry->high,
            entry->low, entry->base, (unsigned long) entry->count,
            entry->flag, entry->deleted, (unsigned long) entry->indexinode);
}


/*
**  Dump the complete contents of the group.index file in human-readable form
**  to the specified file, one line per group.
*/
void
tdx_index_dump(struct group_index *index, FILE *output)
{
    int bucket;
    long current;
    struct group_entry *entry;
    struct hash *hashmap;
    struct hashmap *group;
    char *name;

    if (index->header == NULL || index->entries == NULL)
        return;
    hashmap = hashmap_load();
    for (bucket = 0; bucket < TDX_HASH_SIZE; bucket++) {
        current = index->header->hash[bucket].recno;
        while (current != -1) {
            if (!index_maybe_remap(index, current))
                return;
            entry = index->entries + current;
            name = NULL;
            if (hashmap != NULL) {
                group = hash_lookup(hashmap, &entry->hash);
                if (group != NULL)
                    name = group->name;
            }
            if (name == NULL)
                name = HashToText(entry->hash);
            tdx_index_print(name, entry, output);
            current = entry->next.recno;
        }
    }
    if (hashmap != NULL)
        hash_free(hashmap);
}
