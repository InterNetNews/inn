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

/*
**  Dump the main index file.
*/
static void
dump_index(void)
{
    struct group_index *index;

    index = tdx_index_open(OV_READ);
    if (index == NULL)
        return;
    tdx_index_dump(index);
    tdx_index_close(index);
}


/*
**  Dump the main index data for a particular group.
*/
static void
dump_group(const char *group)
{
    struct group_index *index;
    const struct group_entry *entry;

    index = tdx_index_open(OV_READ);
    if (index == NULL)
        return;
    entry = tdx_index_entry(index, group);
    if (entry == NULL) {
        warn("cannot find group %s", group);
        return;
    }
    tdx_index_print(group, entry);
    tdx_index_close(index);
}


/*
**  Dump the data index file for a particular group.
*/
static void
dump_group_index(const char *group)
{
    struct group_index *index;
    struct group_data *data;

    index = tdx_index_open(OV_READ);
    if (index == NULL)
        return;
    data = tdx_data_open(index, group);
    if (data == NULL) {
        warn("cannot open group %s", group);
        return;
    }
    tdx_data_index_dump(data);
    tdx_data_close(data);
    tdx_index_close(index);
}


/*
**  Dump the overview data for a particular group.  If number is 0, dump the
**  overview data for all current articles; otherwise, only dump the data for
**  that particular article.  Include the article number, token, arrived time,
**  and expires time (if any) in the overview data as additional fields.
*/
static void
dump_overview(const char *group, ARTNUM number)
{
    struct group_index *index;
    struct group_data *data;
    const struct group_entry *entry;
    struct article article;
    struct search *search;
    char datestring[256];

    index = tdx_index_open(OV_READ);
    if (index == NULL)
        return;
    data = tdx_data_open(index, group);
    if (data == NULL) {
        warn("cannot open group %s", group);
        return;
    }

    entry = tdx_index_entry(index, group);
    if (entry == NULL) {
        warn("cannot find group %s", group);
        return;
    }
    if (number != 0)
        search = tdx_search_open(data, number, number);
    else
        search = tdx_search_open(data, entry->low, entry->high);
    if (search == NULL) {
        if (number != 0)
            puts("Article not found");
        else
            warn("cannot open search in %s: %lu - %lu", group, entry->low,
                 entry->high);
        return;
    }
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
    tdx_index_close(index);
}


/*
**  Main routine.  Load inn.conf, parse the arguments, and dispatch to the
**  appropriate function.
*/
int
main(int argc, char *argv[])
{
    int option;
    char mode = '\0';
    const char *newsgroup = NULL;
    ARTNUM article = 0;

    error_program_name = "tdx-util";

    if (ReadInnConf() < 0)
        exit(1);

    /* Parse options. */
    opterr = 0;
    while ((option = getopt(argc, argv, "dg:i:n:o:p:")) != EOF) {
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
        case 'n':
            article = strtoul(optarg, NULL, 10);
            if (article == 0)
                die("invalid article number %s", optarg);
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
        dump_overview(newsgroup, article);
        break;
    default:
        die("a mode option must be specified");
        break;
    }
    exit(0);
}
