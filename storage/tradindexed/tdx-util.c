/*  $Id$
**
**  Utility for managing a tradindexed overview spool.
**
**  This utility can manipulate a tradindexed overview spool in various ways,
**  including some ways that are useful for recovery from crashes.  It allows
**  the user to view the contents of the various data structures that
**  tradindexed stores on disk.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#include "inn/buffer.h"
#include "inn/libinn.h"
#include "inn/history.h"
#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/newsuser.h"
#include "inn/ov.h"
#include "inn/paths.h"
#include "inn/vector.h"
#include "inn/wire.h"
#include "ovinterface.h"
#include "tdx-private.h"
#include "tdx-structure.h"

/*
**  Dump the main index data, either all of it or that for a particular group
**  if the group argument is non-NULL.
*/
static void
dump_index(const char *group)
{
    struct group_index *index;

    index = tdx_index_open(OV_READ);
    if (index == NULL)
        return;
    if (group == NULL)
        tdx_index_dump(index, stdout);
    else {
        const struct group_entry *entry;

        entry = tdx_index_entry(index, group);
        if (entry == NULL) {
            warn("cannot find group %s", group);
            return;
        }
        tdx_index_print(group, entry, stdout);
    }
    tdx_index_close(index);
}


/*
**  Dump the data index file for a particular group.
*/
static void
dump_group_index(const char *group)
{
    struct group_index *index;
    struct group_entry *entry;
    struct group_data *data;

    index = tdx_index_open(OV_READ);
    if (index == NULL)
        return;
    entry = tdx_index_entry(index, group);
    if (entry == NULL) {
        warn("cannot find group %s in the index", group);
        return;
    }
    data = tdx_data_open(index, group, entry);
    if (data == NULL) {
        warn("cannot open group %s", group);
        return;
    }
    tdx_data_index_dump(data, stdout);
    tdx_data_close(data);
    tdx_index_close(index);
}


/*
**  Dump the overview data for a particular group.  If number is 0, dump the
**  overview data for all current articles; otherwise, only dump the data for
**  that particular article.  Include the article number, token, arrived time,
**  and expires time (if any) in the overview data as additional fields
**  normally.  If overchan is true, instead write out the data in the format
**  that overchan would expect as input.
*/
static void
dump_overview(const char *group, ARTNUM low, ARTNUM high, bool overchan)
{
    struct group_index *index;
    struct group_data *data;
    struct group_entry *entry;
    struct article article;
    struct search *search;
    char *p;
    char datestring[256];

    index = tdx_index_open(OV_READ);
    if (index == NULL)
        return;
    entry = tdx_index_entry(index, group);
    if (entry == NULL) {
        warn("cannot find group %s", group);
        return;
    }
    data = tdx_data_open(index, group, entry);
    if (data == NULL) {
        warn("cannot open group %s", group);
        return;
    }
    data->refcount++;

    if (low == 0)
        low = entry->low;
    if (high == 0)
        high = entry->high;
    search = tdx_search_open(data, low, high, entry->high);

    if (search == NULL) {
        if (low == high)
            puts("Article not found");
        else
            warn("cannot open search in %s: %lu - %lu", group, low, high);
        return;
    }
    while (tdx_search(search, &article)) {
        if (overchan) {
            p = memchr(article.overview, '\t', article.overlen - 3);
            if (p == NULL)
                continue;
            p++;
            printf("%s %lu %lu ", TokenToText(article.token),
                   (unsigned long) article.arrived,
                   (unsigned long) article.expires);
            fwrite(p, article.overlen - 2 - (p - article.overview), 1, stdout);
        } else {
            fwrite(article.overview, article.overlen - 2, 1, stdout);
            printf("\tArticle: %lu\tToken: %s", article.number,
                   TokenToText(article.token));
            makedate(article.arrived, true, datestring, sizeof(datestring));
            printf("\tArrived: %s", datestring);
            if (article.expires != 0) {
                makedate(article.expires, true, datestring,
                         sizeof(datestring));
                printf("\tExpires: %s", datestring);
            }
        }
        printf("\n");
    }
    tdx_search_close(search);
    tdx_data_close(data);
    tdx_index_close(index);
}


/*
**  Check a string to see if its a valid number.
*/
static bool
check_number(const char *string)
{
    const char *p;

    for (p = string; *p != '\0'; p++)
        if (!CTYPE(isdigit, *p))
            return false;
    return true;
}


/*
**  Find the message ID in the group overview data and return a copy of it.
**  Caller is responsible for freeing.
*/
static char *
extract_messageid(const char *overview)
{
    const char *p, *end;
    int count;

    for (p = overview, count = 0; count < 4; count++) {
        p = strchr(p + 1, '\t');
        if (p == NULL)
            return NULL;
    }
    p++;
    end = strchr(p, '\t');
    if (end == NULL)
        return NULL;
    return xstrndup(p, end - p);
}


/*
**  Compare two file names assuming they're numbers, used to sort the list of
**  articles numerically.  Suitable for use as a comparison function for
**  qsort.
*/
static int
file_compare(const void *p1, const void *p2)
{
    const char *file1 = *((const char * const *) p1);
    const char *file2 = *((const char * const *) p2);
    ARTNUM n1, n2;

    n1 = strtoul(file1, NULL, 10);
    n2 = strtoul(file2, NULL, 10);
    if (n1 > n2)
        return 1;
    else if (n1 < n2)
        return -1;
    else
        return 0;
}


/*
**  Get a list of articles in a directory, sorted by article number.
*/
static struct vector *
article_list(const char *directory)
{
    DIR *articles;
    struct dirent *file;
    struct vector *list;

    list = vector_new();
    articles = opendir(directory);
    if (articles == NULL)
        sysdie("cannot open directory %s", directory);
    while ((file = readdir(articles)) != NULL) {
        if (!check_number(file->d_name))
            continue;
        vector_add(list, file->d_name);
    }
    closedir(articles);

    qsort(list->strings, list->count, sizeof(list->strings[0]), file_compare);
    return list;
}


/*
**  Create a new newsgroup in the overview database.  Takes the group flag and
**  the high and low water marks.
*/
static void
group_create(const char *group, ARTNUM low, ARTNUM high, char flag)
{
    struct group_index *index;

    index = tdx_index_open(true);
    if (index == NULL)
        die("cannot open group index");
    if (low == 0)
        low = 1;
    if (!tdx_index_add(index, group, low, high, &flag))
        die("cannot create group %s", group);
    tdx_index_close(index);
}


/*
**  Rebuild the overview data for a particular group.  Takes a path to a
**  directory containing all the articles, as individual files, that should be
**  in that group.  The names of the files should be the article numbers in
**  the group.
*/
static void
group_rebuild(const char *group, const char *path)
{
    char *filename, *histpath, *article, *wireformat, *p;
    size_t size, length, file;
    int flags;
    struct buffer *overview = NULL;
    struct vector *extra, *files;
    struct history *history;
    struct group_index *index;
    struct group_data *data;
    struct group_entry *entry, info;
    struct article artdata;
    struct stat st;

    index = tdx_index_open(OV_READ);
    if (index == NULL)
        die("cannot open group index");
    entry = tdx_index_entry(index, group);
    if (entry == NULL) {
        if (!tdx_index_add(index, group, 1, 0, NF_FLAG_OK_STRING))
            die("cannot create group %s", group);
        entry = tdx_index_entry(index, group);
        if (entry == NULL)
            die("cannot find group %s", group);
    }
    info = *entry;
    data = tdx_data_rebuild_start(group);
    if (data == NULL)
        die("cannot start data rebuild for %s", group);
    if (!tdx_index_rebuild_start(index, entry))
        die("cannot start index rebuild for %s", group);

    histpath = concatpath(innconf->pathdb, INN_PATH_HISTORY);
    flags = HIS_RDONLY | HIS_ONDISK;
    history = HISopen(histpath, innconf->hismethod, flags);
    if (history == NULL)
        sysdie("cannot open history %s", histpath);
    free(histpath);

    extra = overview_extra_fields(true);
    files = article_list(path);

    info.count = 0;
    info.high = 0;
    info.low = 0;
    for (file = 0; file < files->count; file++) {
        filename = concatpath(path, files->strings[file]);
        article = ReadInFile(filename, &st);
        size = st.st_size;
        if (article == NULL) {
            syswarn("cannot read in %s", filename);
            free(filename);
            continue;
        }

        /* Check to see if the article is not in wire format.  If it isn't,
           convert it.  We only check the first line ending. */
        p = strchr(article, '\n');
        if (p != NULL && (p == article || p[-1] != '\r')) {
            wireformat = wire_from_native(article, size, &length);
            free(article);
            article = wireformat;
            size = length;
        }

        artdata.number = strtoul(files->strings[file], NULL, 10);
        if (artdata.number > info.high)
            info.high = artdata.number;
        if (artdata.number < info.low || info.low == 0)
            info.low = artdata.number;
        info.count++;
        overview = overview_build(artdata.number, article, size, extra,
                                  overview);
        artdata.overview = overview->data;
        artdata.overlen = overview->left;
        p = extract_messageid(overview->data);
        if (p == NULL) {
            warn("cannot find message ID in %s", filename);
            free(filename);
            free(article);
            continue;
        }
        if (HISlookup(history, p, &artdata.arrived, NULL, &artdata.expires,
                      &artdata.token)) {
            if (!tdx_data_store(data, &artdata))
                warn("cannot store data for %s", filename);
        } else {
            warn("cannot find article %s in history", p);
        }
        free(p);
        free(filename);
        free(article);
    }
    vector_free(files);
    vector_free(extra);

    info.indexinode = data->indexinode;
    info.base = data->base;
    if (!tdx_index_rebuild_finish(index, entry, &info))
        die("cannot update group index for %s", group);
    if (!tdx_data_rebuild_finish(group))
        die("cannot finish rebuilding data for group %s", group);
    tdx_data_close(data);
    HISclose(history);
}


/*
**  Parse an article number or range, returning the low and high values in the
**  provided arguments.  Returns true if the number or range was parsed
**  successfully, false otherwise.  Allows such constructs as "-", "20-", or
**  "-50" and leaves the unspecified ends of the range set to zero.
**
**  This code is similar to code in nnrpd and possibly should move into libinn
**  as common code.
*/
static bool
parse_range(char *range, ARTNUM *low, ARTNUM *high)
{
    char *hyphen, *end;

    *low = 0;
    *high = 0;
    hyphen = strchr(range, '-');
    if (hyphen == NULL) {
        *low = strtoul(range, &end, 10);
        if (*low == 0 || *end != '\0')
            return false;
        *high = *low;
        return true;
    } else {
        *hyphen = '\0';
        if (*range != '\0') {
            *low = strtoul(range, &end, 10);
            if (*low == 0 || *end != '\0')
                return false;
        }
        if (hyphen[1] != '\0') {
            *high = strtoul(hyphen + 1, &end, 10);
            if (*high == 0 || *end != '\0')
                return false;
        }
        *hyphen = '-';
        return true;
    }
}


/*
**  Main routine.  Load inn.conf, parse the arguments, and dispatch to the
**  appropriate function.
*/
int
main(int argc, char *argv[])
{
    int option;
    char flag = 'y';
    char mode = '\0';
    const char *newsgroup = NULL;
    const char *path = NULL;
    ARTNUM artlow = 0;
    ARTNUM arthigh = 0;

    message_program_name = "tdx-util";

    if (!innconf_read(NULL))
        exit(1);

    /* Parse options. */
    opterr = 0;
    while ((option = getopt(argc, argv, "a:f:n:p:AFR:cgiOo")) != EOF) {
        switch (option) {
        case 'a':
            if (!parse_range(optarg, &artlow, &arthigh))
                die("invalid article number or range %s", optarg);
            break;
        case 'f':
            flag = optarg[0];
            break;
        case 'n':
            newsgroup = optarg;
            break;
        case 'p':
            innconf->pathoverview = xstrdup(optarg);
            break;
        case 'A':
            if (mode != '\0')
                die("only one mode option allowed");
            mode = 'A';
            break;
        case 'F':
            if (mode != '\0')
                die("only one mode option allowed");
            mode = 'F';
            break;
        case 'R':
            if (mode != '\0')
                die("only one mode option allowed");
            mode = 'R';
            path = optarg;
            break;
        case 'c':
            if (mode != '\0')
                die("only one mode option allowed");
            mode = 'c';
            break;
        case 'g':
            if (mode != '\0')
                die("only one mode option allowed");
            mode = 'g';
            break;
        case 'i':
            if (mode != '\0')
                die("only one mode option allowed");
            mode = 'i';
            break;
        case 'O':
            if (mode != '\0')
                die("only one mode option allowed");
            mode = 'O';
            break;
        case 'o':
            if (mode != '\0')
                die("only one mode option allowed");
            mode = 'o';
            break;
        default:
            die("invalid option %c", optopt);
            break;
        }
    }

    /* Some modes require a group be specified. */
    if (strchr("cgoOR", mode) != NULL && newsgroup == NULL)
        die("group must be specified for -%c", mode);

    /* Run the specified function. */
    switch (mode) {
    case 'A':
        tdx_index_audit(false);
        break;
    case 'F':
        if (getenv("INN_TESTSUITE") == NULL)
            ensure_news_user_grp(true, true);
        tdx_index_audit(true);
        break;
    case 'R':
        if (getenv("INN_TESTSUITE") == NULL)
            ensure_news_user_grp(true, true);
        group_rebuild(newsgroup, path);
        break;
    case 'c':
        if (getenv("INN_TESTSUITE") == NULL)
            ensure_news_user_grp(true, true);
        group_create(newsgroup, artlow, arthigh, flag);
        break;
    case 'i':
        dump_index(newsgroup);
        break;
    case 'g':
        dump_group_index(newsgroup);
        break;
    case 'O':
        dump_overview(newsgroup, artlow, arthigh, true);
        break;
    case 'o':
        dump_overview(newsgroup, artlow, arthigh, false);
        break;
    default:
        die("a mode option must be specified");
        break;
    }
    exit(0);
}
