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

#include "libinn.h"
#include "ov.h"
#include "tdx-private.h"
#include "tdx-structure.h"

static void
dump_index(void)
{
    struct group_index *index;

    index = tdx_group_index_open(OV_READ);
    if (index == NULL)
        return;
    tdx_group_index_dump(index);
    tdx_group_index_close(index);
}

static void
dump_group(const char *group)
{
    struct group_index *index;
    struct group_entry *entry;

    index = tdx_group_index_open(OV_READ);
    if (index == NULL)
        return;
    entry = tdx_group_index_entry(index, group);
    if (entry == NULL)
        return;
    tdx_group_index_print(group, entry);
    tdx_group_index_close(index);
}

static void
dump_group_index(const char *group)
{
    struct group_index *index;
    struct group_data *data;

    index = tdx_group_index_open(OV_READ);
    if (index == NULL)
        return;
    data = tdx_data_open(index, group);
    if (data == NULL)
        return;
    tdx_data_index_dump(data);
    tdx_data_close(data);
    tdx_group_index_close(index);
}

static void
dump_overview(const char *group)
{
    struct group_index *index;
    struct group_data *data;
    struct group_entry *entry;
    struct article article;
    struct search *search;
    char datestring[256];

    index = tdx_group_index_open(OV_READ);
    if (index == NULL)
        return;
    data = tdx_data_open(index, group);
    if (data == NULL)
        return;

    entry = tdx_group_index_entry(index, group);
    if (entry == NULL)
        return;
    search = tdx_search_open(data, entry->low, entry->high);
    if (search == NULL)
        return;
    while (tdx_search(search, &article)) {
        fwrite(article.overview, article.overlen - 2, 1, stdout);
        printf("\tArticle: %lu\tToken: %s", article.number,
               TokenToText(article.token));
        makedate(article.arrived, true, datestring, sizeof(datestring));
        printf("\tArrived: %s", datestring);
        if (article.expires != 0) {
            makedate(article.expires, true, datestring, sizeof(datestring));
            printf("\tExpires: %s", datestring);
        }
        printf("\n");
    }
    tdx_search_close(search);
    tdx_data_close(data);
    tdx_group_index_close(index);
}

int
main(int argc, char *argv[])
{
    int option;
    char mode = '\0';
    const char *newsgroup = NULL;

    error_program_name = "tdx-util";

    if (ReadInnConf() < 0)
        exit(1);

    /* Parse options. */
    opterr = 0;
    while ((option = getopt(argc, argv, "dg:i:o:")) != EOF) {
        switch (option) {
        case 'd':
            if (mode != '\0')
                die("only one mode option allowed");
            mode = 'd';
            break;
        case 'g':
            if (mode != '\0')
                die("only one mode option allowed");
            mode = 'g';
            newsgroup = optarg;
            break;
        case 'i':
            if (mode != '\0')
                die("only one mode option allowed");
            mode = 'i';
            newsgroup = optarg;
            break;
        case 'o':
            if (mode != '\0')
                die("only one mode option allowed");
            mode = 'o';
            newsgroup = optarg;
            break;
        case 'p':
            innconf->pathoverview = xstrdup(optarg);
            break;
        default:
            die("invalid option %c", optopt);
            break;
        }
    }

    /* Run the specified function. */
    switch (mode) {
    case 'd':
        dump_index();
        break;
    case 'g':
        dump_group_index(newsgroup);
        break;
    case 'i':
        dump_group(newsgroup);
        break;
    case 'o':
        dump_overview(newsgroup);
        break;
    default:
        die("a mode option must be specified");
        break;
    }
    exit(0);
}
