/*  $Id$
**
**  Overview data file handling for the tradindexed overview method.
**
**  Implements the handling of the .IDX and .DAT files for the tradindexed
**  overview method.  The .IDX files are flat arrays of binary structs
**  specifying the offset in the data file of the overview data for a given
**  article as well as the length of that data and some additional meta-data
**  about that article.  The .DAT files contain all of the overview data for
**  that group in wire format.
**
**  Externally visible functions have a tdx_ prefix; internal functions do
**  not.  (Externally visible unfortunately means everything that needs to be
**  visible outside of this object file, not just interfaces exported to
**  consumers of the overview API.)
*/

#include "config.h"
#include "clibrary.h"
#include "portable/mmap.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "inn/history.h"
#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/mmap.h"
#include "libinn.h"
#include "ov.h"
#include "ovinterface.h"
#include "storage.h"
#include "tdx-private.h"
#include "tdx-structure.h"

/* Returned to callers as an opaque data type, this holds the information
   needed to manage a search in progress. */
struct search {
    ARTNUM limit;
    ARTNUM current;
    struct group_data *data;
};

/* Internal prototypes. */
static char *group_path(const char *group);
static int file_open(const char *base, const char *suffix, bool writable,
                     bool append);
static bool file_open_index(struct group_data *, const char *suffix);
static bool file_open_data(struct group_data *, const char *suffix);
static void *map_file(int fd, size_t length, const char *base,
                      const char *suffix);
static bool map_index(struct group_data *data);
static bool map_data(struct group_data *data);
static void unmap_index(struct group_data *data);
static void unmap_data(struct group_data *data);
static ARTNUM index_base(ARTNUM artnum);


/*
**  Determine the path to the data files for a particular group and return
**  it.  Allocates memory which the caller is responsible for freeing.
*/
static char *
group_path(const char *group)
{
    char *path, *p;
    size_t length;
    const char *gp;

    length = strlen(innconf->pathoverview);
    for (gp = group; *gp != '\0'; gp++)
        if (*gp == '.')
            length += 2;
    length += 2 + 1 + strlen(group) + 1;
    path = xmalloc(length);
    strlcpy(path, innconf->pathoverview, length);
    p = path + strlen(innconf->pathoverview);
    for (gp = group; gp != NULL; gp = strchr(gp, '.')) {
        if (gp != group)
            gp++;
        if (*gp != '\0') {
            *p++ = '/';
            *p++ = *gp;
        }
    }
    *p++ = '/';
    strlcpy(p, group, length - (p - path));
    return path;
}


/*
**  Open a data file for a group.  Takes the base portion of the file, the
**  suffix, a bool saying whether or not the file is being opened for write,
**  and a bool saying whether to open it for append.  Returns the file
**  descriptor.
*/
static int
file_open(const char *base, const char *suffix, bool writable, bool append)
{
    char *file;
    int flags, fd;

    file = concat(base, ".", suffix, (char *) 0);
    flags = writable ? (O_RDWR | O_CREAT) : O_RDONLY;
    if (append)
        flags |= O_APPEND;
    fd = open(file, flags, ARTFILE_MODE);
    if (fd < 0 && writable && errno == ENOENT) {
        char *p = strrchr(file, '/');

        *p = '\0';
        if (!MakeDirectory(file, true)) {
            syswarn("tradindexed: cannot create directory %s", file);
            free(file);
            return -1;
        }
        *p = '/';
        fd = open(file, flags, ARTFILE_MODE);
    }
    if (fd < 0) {
        if (errno != ENOENT)
            syswarn("tradindexed: cannot open %s", file);
        free(file);
        return -1;
    }
    free(file);
    return fd;
}


/*
**  Open the index file for a group.  Takes an optional suffix to use instead
**  of IDX (used primarily for expiring).
*/
static bool
file_open_index(struct group_data *data, const char *suffix)
{
    struct stat st;

    if (suffix == NULL)
        suffix = "IDX";
    if (data->indexfd >= 0)
        close(data->indexfd);
    data->indexfd = file_open(data->path, suffix, data->writable, false);
    if (data->indexfd < 0)
        return false;
    if (fstat(data->indexfd, &st) < 0) {
        syswarn("tradindexed: cannot stat %s.%s", data->path, suffix);
        close(data->indexfd);
        return false;
    }
    data->indexinode = st.st_ino;
    close_on_exec(data->indexfd, true);
    return true;
}


/*
**  Open the data file for a group.  Takes an optional suffix to use instead
**  of DAT (used primarily for expiring).
*/
static bool
file_open_data(struct group_data *data, const char *suffix)
{
    if (suffix == NULL)
        suffix = "DAT";
    if (data->datafd >= 0)
        close(data->datafd);
    data->datafd = file_open(data->path, suffix, data->writable, true);
    if (data->datafd < 0)
        return false;
    close_on_exec(data->datafd, true);
    return true;
}


/*
**  Open a particular group.  Allocates a new struct group_data that should be
**  passed to tdx_data_close() when the caller is done with it.
*/
struct group_data *
tdx_data_new(const char *group, bool writable)
{
    struct group_data *data;

    data = xmalloc(sizeof(struct group_data));
    data->path = group_path(group);
    data->writable = writable;
    data->high = 0;
    data->base = 0;
    data->indexfd = -1;
    data->datafd = -1;
    data->index = NULL;
    data->data = NULL;
    data->indexlen = 0;
    data->datalen = 0;
    data->indexinode = 0;
    data->refcount = 0;

    return data;
}


/*
**  Open the index and data files for a group.
*/
bool
tdx_data_open_files(struct group_data *data)
{
    unmap_index(data);
    unmap_data(data);
    data->index = NULL;
    data->data = NULL;
    if (!file_open_index(data, NULL))
        goto fail;
    if (!file_open_data(data, NULL))
        goto fail;
    return true;

 fail:
    if (data->indexfd >= 0)
        close(data->indexfd);
    if (data->datafd >= 0)
        close(data->datafd);
    return false;
}


/*
**  Map a data file (either index or data), or read in all of the data in the
**  file if we're avoiding mmap.  Takes the base and suffix of the file for
**  error reporting.
*/
static void *
map_file(int fd, size_t length, const char *base, const char *suffix)
{
    char *data;

    if (length == 0)
        return NULL;

    if (!innconf->tradindexedmmap) {
        ssize_t status;

        data = xmalloc(length);
        status = read(fd, data, length);
        if ((size_t) status != length) {
            syswarn("tradindexed: cannot read data file %s.%s", base, suffix);
            free(data);
            return NULL;
        }
    } else {
        data = mmap(NULL, length, PROT_READ, MAP_SHARED, fd, 0);
        if (data == MAP_FAILED) {
            syswarn("tradindexed: cannot mmap %s.%s", base, suffix);
            return NULL;
        }
    }
    return data;
}


/*
**  Memory map the index file.
*/
static bool
map_index(struct group_data *data)
{
    struct stat st;
    int r;

    r = fstat(data->indexfd, &st);
    if (r == -1) {
	if (errno == ESTALE) {
	    r = file_open_index(data, NULL);
	} else {
	    syswarn("tradindexed: cannot stat %s.IDX", data->path);
	}
    }
    if (r == -1)
	return false;
    data->indexlen = st.st_size;
    data->index = map_file(data->indexfd, data->indexlen, data->path, "IDX");
    return (data->index == NULL && data->indexlen > 0) ? false : true;
}


/*
**  Memory map the data file.
*/
static bool
map_data(struct group_data *data)
{
    struct stat st;
    int r;

    r = fstat(data->datafd, &st);
    if (r == -1) {
	if (errno == ESTALE) {
	    r = file_open_data(data, NULL);
	} else {
	    syswarn("tradindexed: cannot stat %s.DAT", data->path);
	}
    }
    if (r == -1)
	return false;
    data->datalen = st.st_size;
    data->data = map_file(data->datafd, data->datalen, data->path, "DAT");
    return (data->data == NULL && data->indexlen > 0) ? false : true;
}


/*
**  Unmap a data file or free the memory copy if we're not using mmap.  Takes
**  the memory to free or unmap, the length for munmap, and the name base and
**  suffix for error reporting.
*/
static void
unmap_file(void *data, off_t length, const char *base, const char *suffix)
{
    if (data == NULL)
        return;
    if (!innconf->tradindexedmmap)
        free(data);
    else {
        if (munmap(data, length) < 0)
            syswarn("tradindexed: cannot munmap %s.%s", base, suffix);
    }
    return;
}

/*
**  Unmap the index file.
*/
static void
unmap_index(struct group_data *data)
{
    unmap_file(data->index, data->indexlen, data->path, "IDX");
    data->index = NULL;
}


/*
**  Unmap the data file.
*/
static void
unmap_data(struct group_data *data)
{
    unmap_file(data->data, data->datalen, data->path, "DAT");
    data->data = NULL;
}

/*
**  Determine if the file handle associated with the index table is stale
*/
static bool
stale_index(struct group_data *data)
{
    struct stat st;
    int r;

    r = fstat(data->indexfd, &st);
    return r == -1 && errno == ESTALE;
}


/*
**  Determine if the file handle associated with the data table is stale
*/
static bool
stale_data(struct group_data *data)
{
    struct stat st;
    int r;

    r = fstat(data->datafd, &st);
    return r == -1 && errno == ESTALE;
}


/*
**  Retrieves the article metainformation stored in the index table (all the
**  stuff we can return without opening the data file).  Takes the article
**  number and returns a pointer to the index entry.  Also takes the high
**  water mark from the group index; this is used to decide whether to attempt
**  remapping of the index file if the current high water mark is too low.
*/
const struct index_entry *
tdx_article_entry(struct group_data *data, ARTNUM article, ARTNUM high)
{
    struct index_entry *entry;
    ARTNUM offset;

    if (article > data->high && high > data->high) {
        unmap_index(data);
        map_index(data);
        data->high = high;
    } else if (innconf->nfsreader && stale_index(data))
        unmap_index(data);
    if (data->index == NULL)
        if (!map_index(data))
            return NULL;

    if (article < data->base)
        return NULL;
    offset = article - data->base;
    if (offset >= data->indexlen / sizeof(struct index_entry))
        return NULL;
    entry = data->index + offset;
    if (entry->length == 0)
        return NULL;
    return entry;
}


/*
**  Begin an overview search.  In addition to the bounds of the search, we
**  also take the high water mark from the group index; this is used to decide
**  whether or not to attempt remapping of the index file if the current high
**  water mark is too low.
*/
struct search *
tdx_search_open(struct group_data *data, ARTNUM start, ARTNUM end, ARTNUM high)
{
    struct search *search;

    if (end < data->base)
        return NULL;
    if (end < start)
        return NULL;

    if (end > data->high && high > data->high) {
        unmap_index(data);
        map_index(data);
        data->high = high;
    }
    if (start > data->high)
        return NULL;

    if (innconf->nfsreader && stale_index(data))
	unmap_index(data);
    if (data->index == NULL)
        if (!map_index(data))
            return NULL;
    if (innconf->nfsreader && stale_data(data))
	unmap_data(data);
    if (data->data == NULL)
        if (!map_data(data))
            return NULL;

    search = xmalloc(sizeof(struct search));
    search->limit = end - data->base;
    search->current = (start < data->base) ? data->base : start - data->base;
    search->data = data;
    search->data->refcount++;

    return search;
}


/*
**  Return the next record in a search.
*/
bool
tdx_search(struct search *search, struct article *artdata)
{
    struct index_entry *entry;
    size_t max;

    if (search == NULL || search->data == NULL)
        return false;
    if (search->data->index == NULL || search->data->data == NULL)
        return false;

    max = (search->data->indexlen / sizeof(struct index_entry)) - 1;
    entry = search->data->index + search->current;
    while (search->current <= search->limit && search->current <= max) {
        if (entry->length != 0)
            break;
        search->current++;
        entry++;
    }
    if (search->current > search->limit || search->current > max)
        return false;

    /* Make sure that the offset into the data file is sensible, and try
       remapping the data file if the portion the offset is pointing to isn't
       currently mapped.  Otherwise, warn about possible corruption and return
       a miss. */
    if (entry->offset + entry->length > search->data->datalen) {
        unmap_data(search->data);
        if (!map_data(search->data))
            return false;
    }
    if (entry->offset + entry->length > search->data->datalen) {
        warn("Invalid entry for article %lu in %s.IDX: offset %lu length %lu",
             search->current + search->data->base, search->data->path,
             (unsigned long) entry->offset, (unsigned long) entry->length);
        return false;
    }

    artdata->number = search->current + search->data->base;
    artdata->overview = search->data->data + entry->offset;
    artdata->overlen = entry->length;
    artdata->token = entry->token;
    artdata->arrived = entry->arrived;
    artdata->expires = entry->expires;

    search->current++;
    return true;
}


/*
**  End an overview search.
*/
void
tdx_search_close(struct search *search)
{
    if (search->data != NULL) {
        search->data->refcount--;
        if (search->data->refcount == 0)
            tdx_data_close(search->data);
    }
    free(search);
}


/*
**  Given an article number, return an index base appropriate for that article
**  number.  This includes a degree of slop so that we don't have to
**  constantly repack if the article numbers are clustered around a particular
**  value but don't come in order.
*/
ARTNUM
index_base(ARTNUM artnum)
{
    return (artnum > 128) ? (artnum - 128) : 1;
}


/*
**  Store the data for a single article into the overview files for a group.
**  Assumes any necessary repacking has already been done.  If the base value
**  in the group_data structure is 0, assumes this is the first time we've
**  written overview information to this group and sets it appropriately.
*/
bool
tdx_data_store(struct group_data *data, const struct article *article)
{
    struct index_entry entry;
    off_t offset;

    if (!data->writable)
        return false;
    if (data->base == 0)
        data->base = index_base(article->number);
    if (data->base > article->number) {
        warn("tradindexed: cannot add %lu to %s.IDX, base == %lu",
             article->number, data->path, data->base);
        return false;
    }

    /* Write out the data and fill in the index entry. */
    memset(&entry, 0, sizeof(entry));
    if (xwrite(data->datafd, article->overview, article->overlen) < 0) {
        syswarn("tradindexed: cannot append %lu of data for %lu to %s.DAT",
                (unsigned long) article->overlen, article->number,
                data->path);
        return false;
    }
    entry.offset = lseek(data->datafd, 0, SEEK_CUR);
    if (entry.offset < 0) {
        syswarn("tradindexed: cannot get offset for article %lu in %s.DAT",
                article->number, data->path);
        return false;
    }
    entry.length = article->overlen;
    entry.offset -= entry.length;
    entry.arrived = article->arrived;
    entry.expires = article->expires;
    entry.token = article->token;

    /* Write out the index entry. */
    offset = (article->number - data->base) * sizeof(struct index_entry);
    if (xpwrite(data->indexfd, &entry, sizeof(entry), offset) < 0) {
        syswarn("tradindexed: cannot write index record for %lu in %s.IDX",
                article->number, data->path);
        return false;
    }
    return true;
}


/*
**  Start the process of packing a group (rewriting its index file so that it
**  uses a different article base).  Takes the article number of an article
**  that needs to be written to the index file and is below the current base.
**  Returns the true success and false on failure, and sets data->base to the
**  new article base and data->indexinode to the new inode number.  At the
**  conclusion of this routine, the new index file has been created, but it
**  has not yet been moved into place; that is done by tdx_data_pack_finish.
*/
bool
tdx_data_pack_start(struct group_data *data, ARTNUM artnum)
{
    ARTNUM base;
    unsigned long delta;
    int fd;
    char *idxfile;
    struct stat st;

    if (!data->writable)
        return false;
    if (data->base <= artnum) {
        warn("tradindexed: tdx_data_pack_start called unnecessarily");
        return false;
    }

    /* Open the new index file. */
    base = index_base(artnum);
    delta = data->base - base;
    fd = file_open(data->path, "IDX-NEW", true, false);
    if (fd < 0)
        return false;
    if (fstat(fd, &st) < 0) {
        warn("tradindexed: cannot stat %s.IDX-NEW", data->path);
        goto fail;
    }

    /* For convenience, memory map the old index file. */
    unmap_index(data);
    if (!map_index(data))
        goto fail;

    /* Write the contents of the old index file to the new index file. */
    if (lseek(fd, delta * sizeof(struct index_entry), SEEK_SET) < 0) {
        syswarn("tradindexed: cannot seek in %s.IDX-NEW", data->path);
        goto fail;
    }
    if (xwrite(fd, data->index, data->indexlen) < 0) {
        syswarn("tradindexed: cannot write to %s.IDX-NEW", data->path);
        goto fail;
    }
    if (close(fd) < 0) {
        syswarn("tradindexed: cannot close %s.IDX-NEW", data->path);
        goto fail;
    }
    data->base = base;
    data->indexinode = st.st_ino;
    return true;

 fail:
    if (fd >= 0) {
        close(fd);
        idxfile = concat(data->path, ".IDX-NEW", (char *) 0);
        if (unlink(idxfile) < 0)
            syswarn("tradindexed: cannot unlink %s", idxfile);
        free(idxfile);
    }
    return false;
}


/*
**  Finish the process of packing a group by replacing the new index with the
**  old index.  Also reopen the index file and update indexinode to keep our
**  caller from having to close and reopen the index file themselves.
*/
bool
tdx_data_pack_finish(struct group_data *data)
{
    char *newidx, *idx;

    if (!data->writable)
        return false;
    newidx = concat(data->path, ".IDX-NEW", (char *) 0);
    idx = concat(data->path, ".IDX", (char *) 0);
    if (rename(newidx, idx) < 0) {
        syswarn("tradindexed: cannot rename %s to %s", newidx, idx);
        unlink(newidx);
        free(newidx);
        free(idx);
        return false;
    } else {
        free(newidx);
        free(idx);
        if (!file_open_index(data, NULL))
            return false;
        return true;
    }
}


/*
**  Open the data files for a group data rebuild, and return a struct
**  group_data for the new files.  Calling this function doesn't interfere
**  with the existing data for the group.  Either tdx_data_rebuild_abort or
**  tdx_data_rebuild_finish should be called on the returned struct group_data
**  when the caller is done.
*/
struct group_data *
tdx_data_rebuild_start(const char *group)
{
    struct group_data *data;

    data = tdx_data_new(group, true);
    tdx_data_delete(group, "-NEW");
    if (!file_open_index(data, "IDX-NEW"))
        goto fail;
    if (!file_open_data(data, "DAT-NEW"))
        goto fail;
    return data;

 fail:
    tdx_data_delete(group, "-NEW");
    tdx_data_close(data);
    return NULL;
}


/*
**  Finish a rebuild by renaming the new index and data files to their
**  permanent names.
*/
bool
tdx_data_rebuild_finish(const char *group)
{
    char *base, *newidx, *bakidx, *idx, *newdat, *dat;
    bool saved = false;

    base = group_path(group);
    idx = concat(base, ".IDX", (char *) 0);
    newidx = concat(base, ".IDX-NEW", (char *) 0);
    bakidx = concat(base, ".IDX-BAK", (char *) 0);
    dat = concat(base, ".DAT", (char *) 0);
    newdat = concat(base, ".DAT-NEW", (char *) 0);
    free(base);
    if (rename(idx, bakidx) < 0) {
        syswarn("tradindexed: cannot rename %s to %s", idx, bakidx);
        goto fail;
    } else {
        saved = true;
    }
    if (rename(newidx, idx) < 0) {
        syswarn("tradindexed: cannot rename %s to %s", newidx, idx);
        goto fail;
    }
    if (rename(newdat, dat) < 0) {
        syswarn("tradindexed: cannot rename %s to %s", newdat, dat);
        goto fail;
    }
    if (unlink(bakidx) < 0)
        syswarn("tradindexed: cannot remove backup %s", bakidx);
    free(idx);
    free(newidx);
    free(bakidx);
    free(dat);
    free(newdat);
    return true;

 fail:
    if (saved && rename(bakidx, idx) < 0)
        syswarn("tradindexed: cannot restore old index %s", bakidx);
    free(idx);
    free(newidx);
    free(bakidx);
    free(dat);
    free(newdat);
    return false;
}


/*
**  Do the main work of expiring a group.  Step through each article in the
**  group, only writing the unexpired entries out to the new group.  There's
**  probably some room for optimization here for newsgroups that don't expire
**  so that the files don't have to be rewritten, or newsgroups where all the
**  data at the end of the file is still good and just needs to be moved
**  as-is.
*/
bool
tdx_data_expire_start(const char *group, struct group_data *data,
                      struct group_entry *index, struct history *history)
{
    struct group_data *new_data;
    struct search *search;
    struct article article;
    ARTNUM high;

    new_data = tdx_data_rebuild_start(group);
    if (new_data == NULL)
        return false;
    index->indexinode = new_data->indexinode;

    /* Try to make sure that the search range is okay for even an empty group
       so that we can treat all errors on opening a search as errors. */
    high = index->high > 0 ? index->high : data->base;
    new_data->high = high;
    search = tdx_search_open(data, data->base, high, high);
    if (search == NULL)
        goto fail;

    /* Loop through all of the articles in the group, adding the ones that are
       still valid to the new index. */
    while (tdx_search(search, &article)) {
        ARTHANDLE *ah;

        if (!SMprobe(EXPENSIVESTAT, &article.token, NULL) || OVstatall) {
            ah = SMretrieve(article.token, RETR_STAT);
            if (ah == NULL)
                continue;
            SMfreearticle(ah);
        } else {
            if (!OVhisthasmsgid(history, article.overview))
                continue;
        }
        if (innconf->groupbaseexpiry)
            if (OVgroupbasedexpire(article.token, group, article.overview,
                                   article.overlen, article.arrived,
                                   article.expires))
                continue;
        if (!tdx_data_store(new_data, &article))
            goto fail;
        if (index->base == 0) {
            index->base = new_data->base;
            index->low = article.number;
        }
        if (article.number > index->high)
            index->high = article.number;
        index->count++;
    }

    /* Done; the rest happens in tdx_data_rebuild_finish. */
    tdx_data_close(new_data);
    return true;

 fail:
    tdx_data_delete(group, "-NEW");
    tdx_data_close(new_data);
    return false;
}


/*
**  Close the data files for a group and free the data structure.
*/
void
tdx_data_close(struct group_data *data)
{
    unmap_index(data);
    unmap_data(data);
    if (data->indexfd >= 0)
        close(data->indexfd);
    if (data->datafd >= 0)
        close(data->datafd);
    free(data->path);
    free(data);
}


/*
**  Delete the data files for a particular group, called when that group is
**  deleted from the server.  Takes an optional suffix, which if present is
**  appended to the ends of the file names (used by expire to delete the -NEW
**  versions of the files).
*/
void
tdx_data_delete(const char *group, const char *suffix)
{
    char *path, *idx, *dat;

    path = group_path(group);
    idx = concat(path, ".IDX", suffix, (char *) 0);
    dat = concat(path, ".DAT", suffix, (char *) 0);
    if (unlink(idx) < 0 && errno != ENOENT)
        syswarn("tradindexed: cannot unlink %s", idx);
    if (unlink(dat) < 0 && errno != ENOENT)
        syswarn("tradindexed: cannot unlink %s", dat);
    free(dat);
    free(idx);
    free(path);
}


/*
**  RECOVERY AND AUDITING
**
**  All code below this point is not used in the normal operations of the
**  overview method.  Instead, it's code to dump various data structures or
**  audit them for consistency, used by recovery tools and inspection tools.
*/

/*
**  Dump the index file for a given group in human-readable format.
*/
void
tdx_data_index_dump(struct group_data *data, FILE *output)
{
    ARTNUM current;
    struct index_entry *entry, *end;

    if (data->index == NULL)
        if (!map_index(data))
            return;

    current = data->base;
    end = data->index + (data->indexlen / sizeof(struct index_entry));
    for (entry = data->index; entry < end; entry++) {
        fprintf(output, "%lu %lu %lu %lu %lu %s\n", current,
                (unsigned long) entry->offset, (unsigned long) entry->length,
                (unsigned long) entry->arrived,
                (unsigned long) entry->expires, TokenToText(entry->token));
        current++;
    }
}


/*
**  Audit a specific index entry for a particular article.  If there's
**  anything wrong with it, we delete it; to repair a particular group, it's
**  best to just regenerate it from scratch.
*/
static void
entry_audit(struct group_data *data, struct index_entry *entry,
            const char *group, ARTNUM article, bool fix)
{
    struct index_entry new_entry;
    off_t offset;

    if (entry->length < 0) {
        warn("tradindexed: negative length %d in %s:%lu", entry->length,
             group, article);
        if (fix)
            goto clear;
        return;
    }
    if (entry->offset > data->datalen || entry->length > data->datalen) {
        warn("tradindexed: offset %lu or length %lu out of bounds for %s:%lu",
             (unsigned long) entry->offset, (unsigned long) entry->length,
             group, article);
        if (fix)
            goto clear;
        return;
    }
    if (entry->offset + entry->length > data->datalen) {
        warn("tradindexed: offset %lu plus length %lu out of bounds for"
             " %s:%lu", (unsigned long) entry->offset,
             (unsigned long) entry->length, group, article);
        if (fix)
            goto clear;
        return;
    }
    if (!overview_check(data->data + entry->offset, entry->length, article)) {
        warn("tradindexed: malformed overview data for %s:%lu", group,
             article);
        if (fix)
            goto clear;
    }
    return;

 clear:
    new_entry = *entry;
    new_entry.offset = 0;
    new_entry.length = 0;
    offset = (entry - data->index) * sizeof(struct index_entry);
    if (xpwrite(data->indexfd, &new_entry, sizeof(new_entry), offset) != 0)
        warn("tradindexed: unable to repair %s:%lu", group, article);
}


/*
**  Audit the data for a particular group.  Takes the index entry from the
**  group.index file and optionally corrects any problems with the data or the
**  index entry based on the contents of the data.
*/
void
tdx_data_audit(const char *group, struct group_entry *index, bool fix)
{
    struct group_data *data;
    struct index_entry *entry;
    long count;
    off_t expected;
    unsigned long entries, current;
    ARTNUM low = 0;
    bool changed = false;

    data = tdx_data_new(group, true);
    if (!tdx_data_open_files(data))
        return;
    if (!map_index(data))
        goto end;
    if (!map_data(data))
        goto end;

    /* Check the inode of the index. */
    if (data->indexinode != index->indexinode) {
        warn("tradindexed: index inode mismatch for %s: %lu != %lu", group,
             (unsigned long) data->indexinode,
             (unsigned long) index->indexinode);
        if (fix) {
            index->indexinode = data->indexinode;
            changed = true;
        }
    }

    /* Check the index size. */
    entries = data->indexlen / sizeof(struct index_entry);
    expected = entries * sizeof(struct index_entry);
    if (data->indexlen != expected) {
        warn("tradindexed: %lu bytes of trailing trash in %s.IDX",
             (unsigned long)(data->indexlen - expected), data->path);
        if (fix) {
            unmap_index(data);
            if (ftruncate(data->indexfd, expected) < 0)
                syswarn("tradindexed: cannot truncate %s.IDX", data->path);
            if (!map_index(data))
                goto end;
        }
    }

    /* Now iterate through all of the index entries.  In addition to checking
       each one individually, also count the number of valid entries to check
       the count in the index and verify that the low water mark is
       correct. */
    for (current = 0, count = 0; current < entries; current++) {
        entry = &data->index[current];
        if (entry->length == 0)
            continue;
        entry_audit(data, entry, group, index->base + current, fix);
        if (entry->length != 0) {
            if (low == 0)
                low = index->base + current;
            count++;
        }
    }
    if (index->low != low && entries != 0) {
        warn("tradindexed: low water mark incorrect for %s: %lu != %lu",
             group, low, index->low);
        if (fix) {
            index->low = low;
            changed = true;
        }
    }
    if (index->count != count) {
        warn("tradindexed: count incorrect for %s: %lu != %lu", group,
             (unsigned long) count, (unsigned long) index->count);
        if (fix) {
            index->count = count;
            changed = true;
        }
    }

    /* All done.  Close things down and flush the data we changed, if
       necessary. */
    if (changed)
        mapcntl(index, sizeof(*index), MS_ASYNC);

 end:
    tdx_data_close(data);
}
