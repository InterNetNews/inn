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

#include "libinn.h"
#include "storage.h"
#include "tdx-private.h"
#include "tdx-structure.h"

#ifndef NFSREADER
# define NFSREADER 0
#endif

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
static bool file_open_index(struct group_data *);
static bool file_open_data(struct group_data *);
static void *map_file(int fd, size_t length, const char *base,
                      const char *suffix);
static bool map_index(struct group_data *data);
static bool map_data(struct group_data *data);
static void unmap_file(void *data, off_t length, const char *base,
                       const char *suffix);
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
    strcpy(path, innconf->pathoverview);
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
    strcpy(p, group);
    path[length - 1] = '\0';
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
**  Open the index file for a group.
*/
static bool
file_open_index(struct group_data *data)
{
    struct stat st;

    if (data->indexfd >= 0)
        close(data->indexfd);
    data->indexfd = file_open(data->path, "IDX", data->writable, false);
    if (data->indexfd < 0)
        return false;
    if (fstat(data->indexfd, &st) < 0) {
        syswarn("tradindexed: cannot stat %s.IDX", data->path);
        close(data->indexfd);
        return false;
    }
    data->indexinode = st.st_ino;
    close_on_exec(data->indexfd, true);
    return true;
}


/*
**  Open the data file for a group.
*/
static bool
file_open_data(struct group_data *data)
{
    if (data->datafd >= 0)
        close(data->datafd);
    data->datafd = file_open(data->path, "DAT", data->writable, true);
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
**  Open the index file for a group.
*/
bool
tdx_data_open_files(struct group_data *data)
{
    if (!file_open_index(data))
        goto fail;
    if (!file_open_data(data))
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

    if (NFSREADER) {
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

    if (fstat(data->indexfd, &st) < 0) {
        syswarn("tradindexed: cannot stat %s.IDX", data->path);
        return false;
    }
    data->indexlen = st.st_size;
    data->index = map_file(data->indexfd, data->indexlen, data->path, "IDX");
    return (data->index == NULL) ? false : true;
}


/*
**  Memory map the data file.
*/
static bool
map_data(struct group_data *data)
{
    struct stat st;

    if (fstat(data->datafd, &st) < 0) {
        syswarn("tradindexed: cannot stat %s.DAT", data->path);
        return false;
    }
    data->datalen = st.st_size;
    data->data = map_file(data->datafd, data->datalen, data->path, "DAT");
    return (data->data == NULL) ? false : true;
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
    if (NFSREADER)
        free(data);
    else {
        if (munmap(data, length) < 0)
            syswarn("tradindexed: cannot munmap %s.%s", base, suffix);
    }
    return;
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
        unmap_file(data->index, data->indexlen, data->path, "IDX");
        map_index(data);
        data->high = high;
    }
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
        unmap_file(data->index, data->indexlen, data->path, "IDX");
        map_index(data);
        data->high = high;
    }
    if (start > data->high)
        return NULL;

    if (data->index == NULL)
        if (!map_index(data))
            return NULL;
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
        unmap_file(search->data->data, search->data->datalen,
                   search->data->path, ".IDX");
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
**  Returns true on success and false on failure, and sets data->base to the
**  new article base.  At the conclusion of this routine, the new index file
**  has been created, but it has not yet been moved into place; that is done
**  by tdx_data_pack_finish.
*/
bool
tdx_data_pack_start(struct group_data *data, ARTNUM artnum)
{
    ARTNUM base;
    unsigned long delta;
    int fd;
    char *idxfile;

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

    /* For convenience, memory map the old index file. */
    unmap_file(data->index, data->indexlen, data->path, "IDX");
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
        if (!file_open_index(data))
            return false;
        return true;
    }
}


/*
**  Close the data files for a group and free the data structure.
*/
void
tdx_data_close(struct group_data *data)
{
    unmap_file(data->index, data->indexlen, data->path, "IDX");
    unmap_file(data->data, data->datalen, data->path, "DAT");
    if (data->indexfd >= 0)
        close(data->indexfd);
    if (data->datafd >= 0)
        close(data->datafd);
    free(data->path);
    free(data);
}


/*
**  Delete the data files for a particular group, called when that group is
**  deleted from the server.
*/
void
tdx_data_delete(const char *group)
{
    char *path, *idx, *dat;

    path = group_path(group);
    idx = concat(path, ".IDX", (char *) 0);
    dat = concat(path, ".DAT", (char *) 0);
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
tdx_data_index_dump(struct group_data *data)
{
    ARTNUM current;
    struct index_entry *entry, *end;

    if (data->index == NULL)
        if (!map_index(data))
            return;

    current = data->base;
    end = data->index + (data->indexlen / sizeof(struct index_entry));
    for (entry = data->index; entry < end; entry++) {
        printf("%lu %lu %lu %lu %lu %s\n", current,
               (unsigned long) entry->offset, (unsigned long) entry->length,
               (unsigned long) entry->arrived, (unsigned long) entry->expires,
               TokenToText(entry->token));
        current++;
    }
}
