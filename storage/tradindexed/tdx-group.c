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
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <time.h>

#include "inn/hashtab.h"
#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/mmap.h"
#include "inn/qio.h"
#include "inn/vector.h"
#include "inn/libinn.h"
#include "inn/paths.h"
#include "tdx-private.h"
#include "tdx-structure.h"

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

/* Forward declaration. */
struct hashmap;

/* Internal prototypes. */
static int index_entry_count(size_t size);
static size_t index_file_size(int count);
static bool index_lock(int fd, enum inn_locktype type);
static bool index_lock_group(int fd, ptrdiff_t offset, enum inn_locktype);
static bool index_map(struct group_index *);
static bool index_maybe_remap(struct group_index *, long loc);
static void index_unmap(struct group_index *);
static bool index_expand(struct group_index *);
static long index_find(struct group_index *, const char *group);


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
    offset = offset * size + sizeof(struct group_header);
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
    if (!innconf->tradindexedmmap && index->writable) {
        warn("tradindexed: cannot open for writing without mmap");
        return false;
    }

    if (!innconf->tradindexedmmap) {
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


static bool
file_open_group_index(struct group_index *index, struct stat *st)
{
    int open_mode;

    index->header = NULL;
    open_mode = index->writable ? O_RDWR | O_CREAT : O_RDONLY;
    index->fd = open(index->path, open_mode, ARTFILE_MODE);
    if (index->fd < 0) {
        syswarn("tradindexed: cannot open %s", index->path);
        goto fail;
    }

    if (fstat(index->fd, st) < 0) {
        syswarn("tradindexed: cannot fstat %s", index->path);
        goto fail;
    }
    close_on_exec(index->fd, true);
    return true;

 fail:
    if (index->fd >= 0) {
        close(index->fd);
        index->fd = -1;
    }
    return false;
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
    int r;

    if (loc < index->count)
        return true;

    /* Don't remap if remapping wouldn't actually help. */
    r = fstat(index->fd, &st);
    if (r == -1) {
	if (errno == ESTALE) {
	    index_unmap(index);
	    if (!file_open_group_index(index, &st))
		return false;
	} else {
	    syswarn("tradindexed: cannot stat %s", index->path);
	    return false;
	}
    }
    count = index_entry_count(st.st_size);
    if (count < loc && index->header != NULL)
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
    if (!innconf->tradindexedmmap) {
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

    /* If mapping the index fails, we've already extended it but we haven't
       done anything with the new portion of the file.  That means that it's
       all zeroes, which means that it contains index entries who all think
       their next entry is entry 0.  We don't want to leave things in this
       state (particularly if this was the first expansion of the index file,
       in which case entry 0 points to entry 0 and our walking functions may
       go into infinite loops.  Undo the file expansion. */
    if (!index_map(index)) {
        index->count -= 1024;
        if (ftruncate(index->fd, index_file_size(index->count)) < 0) {
            syswarn("tradindexed: cannot shrink %s", index->path);
        }
        return false;
    }

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

    inn_msync_page(index->header, index_file_size(index->count), MS_ASYNC);
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
    struct stat st;

    index = xmalloc(sizeof(struct group_index));
    index->path = concatpath(innconf->pathoverview, "group.index");
    index->writable = writable;
    if (!file_open_group_index(index, &st)) {
	goto fail;
    }
    if ((size_t) st.st_size > sizeof(struct group_header)) {
        index->count = index_entry_count(st.st_size);
        if (!index_map(index))
            goto fail;
    } else {
        index->count = 0;
        if (index->writable) {
            if (st.st_size > 0)
                warn("tradindexed: recreating truncated %s", index->path);
            if (!index_expand(index))
                goto fail;
        } else {
            index->header = NULL;
            index->entries = NULL;
        }
    }
    return index;

 fail:
    tdx_index_close(index);
    return NULL;
}


/*
**  Given a group name hash, return an index into the hash table in the
**  group.index header.
*/
static long
index_bucket(HASH hash)
{
    unsigned int bucket;

    memcpy(&bucket, &hash, sizeof(bucket));
    return bucket % TDX_HASH_SIZE;
}


/*
**  Given a pointer to a group entry, return its location number.
*/
static long
entry_loc(const struct group_index *index, const struct group_entry *entry)
{
    return entry - index->entries;
}


/*
**  Splice out a particular group entry.  Takes the entry and a pointer to the
**  location where a pointer to it is stored.
*/
static void
entry_splice(struct group_entry *entry, int *parent)
{
    *parent = entry->next.recno;
    entry->next.recno = -1;
    inn_msync_page(parent, sizeof(*parent), MS_ASYNC);
}


/*
**  Add a new entry to the appropriate hash chain.
*/
static void
index_add(struct group_index *index, struct group_entry *entry)
{
    long bucket, loc;

    bucket = index_bucket(entry->hash);
    loc = entry_loc(index, entry);
    if (loc == index->header->hash[bucket].recno) {
        warn("tradindexed: refusing to add a loop for %ld in bucket %ld",
             loc, bucket);
        return;
    }
    entry->next.recno = index->header->hash[bucket].recno;
    index->header->hash[bucket].recno = entry_loc(index, entry);
    inn_msync_page(&index->header->hash[bucket], sizeof(struct loc), MS_ASYNC);
    inn_msync_page(entry, sizeof(*entry), MS_ASYNC);
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
    if (innconf->nfsreader && !index_maybe_remap(index, LONG_MAX))
	return -1;
    loc = index->header->hash[index_bucket(hash)].recno;

    while (loc >= 0 && loc < index->count) {
        struct group_entry *entry;

        if (loc > index->count && !index_maybe_remap(index, loc))
            return -1;
        entry = index->entries + loc;
        if (entry->deleted == 0)
            if (memcmp(&hash, &entry->hash, sizeof(hash)) == 0)
                return loc;
        if (loc == entry->next.recno) {
            syswarn("tradindexed: index loop for entry %ld", loc);
            return -1;
        }
        loc = entry->next.recno;
    }
    return -1;
}


/*
**  Add a given entry to the free list.
*/
static void
freelist_add(struct group_index *index, struct group_entry *entry)
{
    entry->next.recno = index->header->freelist.recno;
    index->header->freelist.recno = entry_loc(index, entry);
    inn_msync_page(&index->header->freelist, sizeof(struct loc), MS_ASYNC);
    inn_msync_page(entry, sizeof(*entry), MS_ASYNC);
}


/*
**  Find an entry by hash value (rather than group name) and splice it out of
**  whatever chain it might belong to.  This function is called by both
**  index_unlink and index_audit_group.  Locking must be done by the caller.
**  Returns the group location of the spliced group.
*/
static long
index_unlink_hash(struct group_index *index, HASH hash)
{
    int *parent;
    long current;

    parent = &index->header->hash[index_bucket(hash)].recno;
    current = *parent;

    while (current >= 0 && current < index->count) {
        struct group_entry *entry;

        if (current > index->count && !index_maybe_remap(index, current))
            return -1;
        entry = &index->entries[current];
        if (entry->deleted == 0)
            if (memcmp(&hash, &entry->hash, sizeof(hash)) == 0) {
                entry_splice(entry, parent);
                return current;
            }
        if (current == entry->next.recno) {
            syswarn("tradindexed: index loop for entry %ld", current);
            return -1;
        }
        parent = &entry->next.recno;
        current = *parent;
    }
    return -1;
}


/*
**  Like index_find, but also removes that entry out of whatever chain it
**  might belong to.  This function is called by tdx_index_delete.  Locking
**  must be done by the caller.
*/
static long
index_unlink(struct group_index *index, const char *group)
{
    HASH hash;

    hash = Hash(group, strlen(group));
    return index_unlink_hash(index, hash);
}


/*
**  Return the information stored about a given group in the group index.
*/
struct group_entry *
tdx_index_entry(struct group_index *index, const char *group)
{
    long loc;
    struct group_entry *entry;

    loc = index_find(index, group);
    if (loc == -1)
        return NULL;
    entry = index->entries + loc;
    if (innconf->tradindexedmmap && innconf->nfsreader)
	inn_msync_page(entry, sizeof *entry, MS_INVALIDATE);
    return entry;
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
            inn_msync_page(entry, sizeof(*entry), MS_ASYNC);
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
    index->header->freelist.recno = index->entries[loc].next.recno;
    inn_msync_page(&index->header->freelist, sizeof(struct loc), MS_ASYNC);

    /* Initialize the entry. */
    entry = &index->entries[loc];
    hash = Hash(group, strlen(group));
    entry->hash = hash;
    entry->low = (low == 0 && high != 0) ? high + 1 : low;
    entry->high = high;
    entry->deleted = 0;
    entry->base = 0;
    entry->count = 0;
    entry->flag = *flag;
    data = tdx_data_new(group, index->writable);
    if (!tdx_data_open_files(data))
        warn("tradindexed: unable to create data files for %s", group);
    entry->indexinode = data->indexinode;
    tdx_data_close(data);
    index_add(index, entry);

    index_lock(index->fd, INN_LOCK_UNLOCK);
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

    /* Lock the header for the entire operation, mostly as prevention against
       interfering with ongoing audits (which lock while they're running). */
    index_lock(index->fd, INN_LOCK_WRITE);

    /* Splice out the entry and mark it as deleted. */
    loc = index_unlink(index, group);
    if (loc == -1) {
        index_lock(index->fd, INN_LOCK_UNLOCK);
        return false;
    }
    entry = &index->entries[loc];
    entry->deleted = time(NULL);
    HashClear(&entry->hash);

    /* Add the entry to the free list. */
    freelist_add(index, entry);
    index_lock(index->fd, INN_LOCK_UNLOCK);

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
    if (index->fd >= 0) {
        close(index->fd);
        index->fd = -1;
    }
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

    /* Make sure we have the most current data files and that we have the
       right base article number. */
    if (entry->indexinode != data->indexinode) {
        if (!tdx_data_open_files(data))
            goto fail;
        if (entry->indexinode != data->indexinode)
            warn("tradindexed: index inode mismatch for %s",
                 HashToText(entry->hash));
        data->base = entry->base;
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
        inn_msync_page(entry, sizeof(*entry), MS_ASYNC);
        if (!tdx_data_pack_finish(data)) {
            entry->base = old_base;
            entry->indexinode = old_inode;
            inn_msync_page(entry, sizeof(*entry), MS_ASYNC);
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
    inn_msync_page(entry, sizeof(*entry), MS_ASYNC);
    index_lock_group(index->fd, offset, INN_LOCK_UNLOCK);
    return true;

 fail:
    index_lock_group(index->fd, offset, INN_LOCK_UNLOCK);
    return false;
}


/*
**  Start a rebuild of the group data for a newsgroup.  Right now, all this
**  does is lock the group index entry.
*/
bool
tdx_index_rebuild_start(struct group_index *index, struct group_entry *entry)
{
    ptrdiff_t offset;

    offset = entry - index->entries;
    return index_lock_group(index->fd, offset, INN_LOCK_WRITE);
}


/*
**  Finish a rebuild of the group data for a newsgroup.  Takes the old and new
**  entry and writes the data from the new entry into the group index, and
**  then unlocks it.
*/
bool
tdx_index_rebuild_finish(struct group_index *index, struct group_entry *entry,
                         struct group_entry *new)
{
    ptrdiff_t offset;
    ino_t new_inode;

    new_inode = new->indexinode;
    new->indexinode = entry->indexinode;
    *entry = *new;
    entry->indexinode = new_inode;
    new->indexinode = new_inode;
    inn_msync_page(entry, sizeof(*entry), MS_ASYNC);
    offset = entry - index->entries;
    index_lock_group(index->fd, offset, INN_LOCK_UNLOCK);
    return true;
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
    tdx_index_rebuild_start(index, entry);

    /* tdx_data_expire_start builds the new IDX and DAT files and fills in the
       struct group_entry that was passed to it.  tdx_data_rebuild_finish does
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
    inn_msync_page(entry, sizeof(*entry), MS_ASYNC);
    tdx_data_close(data);
    if (!tdx_data_rebuild_finish(group)) {
        entry->base = old_base;
        entry->indexinode = old_inode;
        inn_msync_page(entry, sizeof(*entry), MS_ASYNC);
        goto fail;
    }

    /* Almost done.  Update the group index.  If there are no articles in the
       group, the low water mark should be one more than the high water
       mark. */
    if (new_entry.low == 0)
        new_entry.low = new_entry.high + 1;
    tdx_index_rebuild_finish(index, entry, &new_entry);
    if (low != NULL)
        *low = entry->low;
    tdx_index_close(index);
    return true;

 fail:
    offset = entry - index->entries;
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
    char flag;
};

/* Holds information needed by hash traversal functions.  Right now, this is
   just the pointer to the group index and a flag saying whether to fix
   problems or not. */
struct audit_data {
    struct group_index *index;
    bool fix;
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
    char *activepath, *line;
    struct cvector *data = NULL;
    struct stat st;
    size_t hash_size;
    struct hashmap *group;
    HASH grouphash;

    activepath = concatpath(innconf->pathdb, INN_PATH_ACTIVE);
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
        data = cvector_split_space(line, data);
        if (data->count != 4) {
            warn("tradindexed: malformed active file line %s", line);
            continue;
        }
        group = xmalloc(sizeof(struct hashmap));
        group->name = xstrdup(data->strings[0]);
        group->flag = data->strings[3][0];
        grouphash = Hash(group->name, strlen(group->name));
        memcpy(&group->hash, &grouphash, sizeof(HASH));
        hash_insert(hash, &group->hash, group);
        line = QIOread(active);
    }
    if (data != NULL)
        cvector_free(data);
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
            entry->flag, (unsigned long) entry->deleted,
            (unsigned long) entry->indexinode);
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
            if (current == entry->next.recno) {
                warn("tradindexed: index loop for entry %ld", current);
                return;
            }
            current = entry->next.recno;
        }
    }
    if (hashmap != NULL)
        hash_free(hashmap);
}


/*
**  Audit a particular group entry location to ensure that it points to a
**  valid entry within the group index file.  Takes a pointer to the location,
**  the number of the location, a pointer to the group entry if any (if not,
**  the location is assumed to be part of the header hash table), and a flag
**  saying whether to fix problems that are found.
*/
static void
index_audit_loc(struct group_index *index, int *loc, long number,
                struct group_entry *entry, bool fix)
{
    bool error = false;

    if (*loc >= index->count) {
        warn("tradindexed: out of range index %d in %s %ld",
             *loc, (entry == NULL ? "bucket" : "entry"), number);
        error = true;
    }
    if (*loc < 0 && *loc != -1) {
        warn("tradindexed: invalid negative index %d in %s %ld",
             *loc, (entry == NULL ? "bucket" : "entry"), number);
        error = true;
    }
    if (entry != NULL && *loc == number) {
        warn("tradindexed: index loop for entry %ld", number);
        error = true;
    }

    if (fix && error) {
        *loc = -1;
        inn_msync_page(loc, sizeof(*loc), MS_ASYNC);
    }
}


/*
**  Check an entry to see if it was actually deleted.  Make sure that all the
**  information is consistent with a deleted group if it's not and the fix
**  flag is set.
*/
static void
index_audit_deleted(struct group_entry *entry, long number, bool fix)
{
    if (entry->deleted != 0 && !HashEmpty(entry->hash)) {
        warn("tradindexed: entry %ld has a delete time but a non-zero hash",
             number);
        if (fix) {
            HashClear(&entry->hash);
            inn_msync_page(entry, sizeof(*entry), MS_ASYNC);
        }
    }
}


/*
**  Audit the group header for any inconsistencies.  This checks the
**  reachability of all of the group entries, makes sure that deleted entries
**  are on the free list, and otherwise checks the linked structure of the
**  whole file.  The data in individual entries is not examined.  If the
**  second argument is true, also attempt to fix inconsistencies.
*/
static void
index_audit_header(struct group_index *index, bool fix)
{
    long bucket, current;
    struct group_entry *entry;
    int *parent, *next;
    bool *reachable;

    /* First, walk all of the regular hash buckets, making sure that all of
       the group location pointers are valid and sane, that all groups that
       have been deleted are correctly marked as such, and that all groups are
       in their correct hash chain.  Build reachability information as we go,
       used later to ensure that all group entries are reachable. */
    reachable = xcalloc(index->count, sizeof(bool));
    for (bucket = 0; bucket < TDX_HASH_SIZE; bucket++) {
        parent = &index->header->hash[bucket].recno;
        index_audit_loc(index, parent, bucket, NULL, fix);
        current = *parent;
        while (current >= 0 && current < index->count) {
            entry = &index->entries[current];
            next = &entry->next.recno;
            if (entry->deleted == 0 && bucket != index_bucket(entry->hash)) {
                warn("tradindexed: entry %ld is in bucket %ld instead of its"
                     " correct bucket %ld", current, bucket,
                     index_bucket(entry->hash));
                if (fix) {
                    entry_splice(entry, parent);
                    next = parent;
                }
            } else {
                if (reachable[current])
                    warn("tradindexed: entry %ld is reachable from multiple"
                         " paths", current);
                reachable[current] = true;
            }
            index_audit_deleted(entry, current, fix);
            index_audit_loc(index, &entry->next.recno, current, entry, fix);
            if (entry->deleted != 0) {
                warn("tradindexed: entry %ld is deleted but not in the free"
                     " list", current);
                if (fix) {
                    entry_splice(entry, parent);
                    next = parent;
                    reachable[current] = false;
                }
            }
            if (*next == current)
                break;
            parent = next;
            current = *parent;
        }
    }

    /* Now, walk the free list.  Make sure that each group in the free list is
       actually deleted, and update the reachability information. */
    index_audit_loc(index, &index->header->freelist.recno, 0, NULL, fix);
    parent = &index->header->freelist.recno;
    current = *parent;
    while (current >= 0 && current < index->count) {
        entry = &index->entries[current];
        index_audit_deleted(entry, current, fix);
        reachable[current] = true;
        if (!HashEmpty(entry->hash) && entry->deleted == 0) {
            warn("tradindexed: undeleted entry %ld in free list", current);
            if (fix) {
                entry_splice(entry, parent);
                reachable[current] = false;
            }
        }
        index_audit_loc(index, &entry->next.recno, current, entry, fix);
        if (entry->next.recno == current)
            break;
        parent = &entry->next.recno;
        current = *parent;
    }

    /* Finally, check all of the unreachable entries and if fix is true, try
       to reattach them in the appropriate location. */
    for (current = 0; current < index->count; current++)
        if (!reachable[current]) {
            warn("tradindexed: unreachable entry %ld", current);
            if (fix) {
                entry = &index->entries[current];
                if (!HashEmpty(entry->hash) && entry->deleted == 0)
                    index_add(index, entry);
                else {
                    HashClear(&entry->hash);
                    entry->deleted = 0;
                    freelist_add(index, entry);
                }
            }
        }

    /* All done. */
    free(reachable);
}


/*
**  Audit a particular group entry for any inconsistencies.  This doesn't
**  check any of the structure, or whether the group is deleted, just the data
**  as stored in the group data files (mostly by calling tdx_data_audit to do
**  the real work).  Note that while the low water mark may be updated, the
**  high water mark is left unchanged.
*/
static void
index_audit_group(struct group_index *index, struct group_entry *entry,
                  struct hash *hashmap, bool fix)
{
    struct hashmap *group;
    ptrdiff_t offset;

    offset = entry - index->entries;
    index_lock_group(index->fd, offset, INN_LOCK_WRITE);
    group = hash_lookup(hashmap, &entry->hash);
    if (group == NULL) {
        warn("tradindexed: group %ld not found in active file",
             entry_loc(index, entry));
        if (fix) {
            index_unlink_hash(index, entry->hash);
            HashClear(&entry->hash);
            entry->deleted = time(NULL);
            freelist_add(index, entry);
        }
    } else {
        if (entry->flag != group->flag) {
            entry->flag = group->flag;
            inn_msync_page(entry, sizeof(*entry), MS_ASYNC);
        }
        tdx_data_audit(group->name, entry, fix);
    }
    index_lock_group(index->fd, offset, INN_LOCK_UNLOCK);
}


/*
**  Check to be sure that a given group exists in the overview index, and if
**  missing, adds it.  Assumes that the index isn't locked, since it calls the
**  normal functions for adding new groups (this should only be called after
**  the index has already been repaired, for the same reason).  Called as a
**  hash traversal function, walking the hash table of groups from the active
**  file.
*/
static void
index_audit_active(void *value, void *cookie)
{
    struct hashmap *group = value;
    struct audit_data *data = cookie;
    struct group_entry *entry;

    entry = tdx_index_entry(data->index, group->name);
    if (entry == NULL) {
        warn("tradindexed: group %s missing from overview", group->name);
        if (data->fix)
            tdx_index_add(data->index, group->name, 0, 0, &group->flag);
    }
}


/*
**  Audit the group index for any inconsistencies.  If the argument is true,
**  also attempt to fix those inconsistencies.
*/
void
tdx_index_audit(bool fix)
{
    struct group_index *index;
    struct stat st;
    off_t expected;
    int count;
    struct hash *hashmap;
    long bucket;
    struct group_entry *entry;
    struct audit_data data;

    index = tdx_index_open(true);
    if (index == NULL)
        return;

    /* Keep a lock on the header through the whole audit process.  This will
       stall any newgroups or rmgroups, but not normal article reception.  We
       don't want the structure of the group entries changing out from under
       us, although we don't mind if the data does until we're validating that
       particular group. */
    index_lock(index->fd, INN_LOCK_WRITE);

    /* Make sure the size looks sensible. */
    if (fstat(index->fd, &st) < 0) {
        syswarn("tradindexed: cannot fstat %s", index->path);
        return;
    }
    count = index_entry_count(st.st_size);
    expected = index_file_size(count);
    if (expected != st.st_size) {
        syswarn("tradindexed: %ld bytes of trailing trash in %s",
                (unsigned long) (st.st_size - expected), index->path);
        if (fix)
            if (ftruncate(index->fd, expected) < 0)
                syswarn("tradindexed: cannot truncate %s", index->path);
    }
    index_maybe_remap(index, count);

    /* Okay everything is now mapped and happy.  Validate the header. */
    index_audit_header(index, fix);
    index_lock(index->fd, INN_LOCK_UNLOCK);

    /* Walk all the group entries and check them individually.  To do this, we
       need to map hashes to group names, so load a hash of the active file to
       do that resolution. */
    hashmap = hashmap_load();
    if (hashmap == NULL) {
        warn("tradindexed: cannot hash active file");
        return;
    }
    data.index = index;
    data.fix = fix;
    hash_traverse(hashmap, index_audit_active, &data);
    for (bucket = 0; bucket < index->count; bucket++) {
        entry = &index->entries[bucket];
        if (HashEmpty(entry->hash) || entry->deleted != 0)
            continue;
        index_audit_group(index, entry, hashmap, fix);
    }
    hash_free(hashmap);
}
