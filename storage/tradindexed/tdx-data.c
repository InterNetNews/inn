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
static int file_open(const char *base, const char *suffix, bool writable);
static void *map_file(int fd, size_t length, const char *base,
                      const char *suffix);
static bool map_index(struct group_data *data);
static bool map_data(struct group_data *data);
static void unmap_file(void *data, off_t length, const char *base,
                       const char *suffix);


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
    p[length - 1] = '\0';
    return path;
}


/*
**  Open a data file for a group.  Takes the base portion of the file, the
**  suffix, and a bool saying whether or not the file is being opened for
**  write.  Returns the file descriptor
*/
static int
file_open(const char *base, const char *suffix, bool writable)
{
    char *file;
    int flags, fd;

    file = concat(base, ".", suffix, (char *) 0);
    flags = writable ? (O_RDWR | O_CREAT) : O_RDONLY;
    fd = open(file, flags, ARTFILE_MODE);
    if (fd < 0 && writable && errno == ENOENT) {
        char *p = strrchr(file, '/');

        *p = '\0';
        if (!MakeDirectory(file, true)) {
            syswarn("tradindexed: cannot create directory %s", file);
            return -1;
        }
        *p = '/';
        fd = open(file, flags, ARTFILE_MODE);
    }
    if (fd < 0) {
        if (errno == ENOENT)
            syswarn("tradindexed: cannot open %s", file);
        return -1;
    }
    return fd;
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
    data->index = NULL;
    data->data = NULL;

    return data;
}


/*
**  Open the index file for a group.
*/
bool
tdx_data_open_files(struct group_data *data)
{
    struct stat st;

    if (data->indexfd >= 0)
        close(data->indexfd);
    data->indexfd = file_open(data->path, "IDX", data->writable);
    if (data->indexfd < 0)
        goto fail;
    if (fstat(data->indexfd, &st) < 0) {
        syswarn("tradindexed: cannot stat %s.IDX", data->path);
        goto fail;
    }
    data->indexinode = st.st_ino;
    data->indexlen = st.st_size;
    close_on_exec(data->indexfd, true);

    if (data->datafd >= 0)
        close(data->datafd);
    data->datafd = file_open(data->path, "DAT", data->writable);
    if (data->datafd < 0)
        goto fail;
    if (fstat(data->datafd, &st) < 0) {
        syswarn("tradindexed: cannot stat %s.DAT", data->path);
        goto fail;
    }
    data->datalen = st.st_size;
    close_on_exec(data->datafd, true);

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
    data->index = map_file(data->indexfd, data->indexlen, data->path, "IDX");
    return (data->index == NULL) ? false : true;
}


/*
**  Memory map the data file.
*/
static bool
map_data(struct group_data *data)
{
    data->data = map_file(data->datafd, data->datalen, data->path, "IDX");
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
**  number and returns a pointer to the index entry.
*/
struct index_entry *
tdx_article_entry(struct group_data *data, ARTNUM article)
{
    struct index_entry *entry;
    ARTNUM offset;

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
**  Begin an overview search.  We need both the group index and the open group
**  data, since we have to get base information and double-check that we still
**  have the right index files.
*/
struct search *
tdx_search_open(struct group_data *data, ARTNUM low, ARTNUM high)
{
    struct search *search;

    if (high < data->base)
        return NULL;
    if (low > data->high)
        return NULL;

    if (data->index == NULL)
        if (!map_index(data))
            return NULL;
    if (data->data == NULL)
        if (!map_data(data))
            return NULL;

    search = xmalloc(sizeof(struct search));
    search->limit = high - data->base;
    search->current = (low < data->base) ? data->base : low - data->base;
    search->data = data;

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

    max = (search->data->indexlen / sizeof(struct index_entry)) - 1;
    entry = search->data->index + search->current;
    while (entry->length == 0) {
        search->current++;
        if (search->current > search->limit || search->current > max)
            break;
        entry++;
    }
    if (search->current > search->limit || search->current > max)
        return false;

    /* Make sure that the offset into the data file is sensible.  Otherwise,
       warn about possible corruption and return a miss. */
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
    free(search);
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
