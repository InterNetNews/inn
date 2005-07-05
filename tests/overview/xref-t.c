/* $Id$ */
/* Test suite for storing overview data based on the Xref header. */

#include "config.h"
#include "clibrary.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/overview.h"
#include "inn/wire.h"
#include "inn/vector.h"
#include "libinn.h"
#include "libtest.h"
#include "storage.h"

/* Used as the artificial token for all articles inserted into overview. */
static const TOKEN faketoken = { 1, 1, "" };

/* Build a stripped-down innconf struct that contains only those settings that
   the overview backend cares about. */
static void
fake_innconf(void)
{
    innconf = xmalloc(sizeof(*innconf));
    innconf->enableoverview = true;
    innconf->groupbaseexpiry = true;
    innconf->keepmmappedthreshold = 1024;
    innconf->overcachesize = 20;
    innconf->ovgrouppat = NULL;
    innconf->ovmethod = xstrdup("tradindexed");
    innconf->patharticles = xstrdup("spool");
    innconf->pathdb = xstrdup("db");
    innconf->pathetc = xstrdup("etc");
    innconf->pathoverview = xstrdup("ov-tmp");
    innconf->pathspool = xstrdup("spool");
    innconf->pathrun = xstrdup("ov-tmp");
    innconf->storeonxref = true;
    innconf->tradindexedmmap = true;
}

/* Initialize the overview database. */
static struct overview *
overview_init(void)
{
    system("/bin/rm -rf ov-tmp");
    if (mkdir("ov-tmp", 0755))
        sysdie("Cannot mkdir ov-tmp");
    if (mkdir("spool", 0755))
        sysdie("Cannot mkdir spool");
    return overview_open(OV_READ | OV_WRITE);
}

/* Check to be sure that the line wasn't too long, and then parse the
   beginning of the line from one of our data files.  Returns a vector of
   group/number pairs from the first data element, stores in start a
   pointer to the beginning of the regular overview data, and stores in Xref a
   pointer to the beginning of the Xref data (which must be the end of the
   overview record). */
static struct cvector *
overview_data_parse(char *data, char **start, char **xref)
{
    size_t length;
    struct cvector *groups;

    length = strlen(data);
    if (data[length - 1] != '\n')
        die("Line too long in input data");
    data[length - 1] = '\0';

    *start = strchr(data, '\t');
    if (*start == NULL)
        die("No tab found in input data");
    **start = '\0';
    (*start)++;
    groups = cvector_split(data, ',', NULL);
    *xref = strstr(*start, "Xref: ");
    if (*xref == NULL)
        die("No Xref found in input data");
    return groups;
}

/* Load overview data from a file.  The first field of each line is a
   comma-separated list of newsgroup name and number pairs, separated by a
   colon, that should correspond to where the Xref data will put this article.
   After storing each line, try to retrieve it and make sure that it was
   stored in the right locations.  Returns the total count of records
   stored. */
static int
overview_load(int n, const char *data, struct overview *overview)
{
    FILE *overdata;
    char buffer[4096];
    char *start, *xref, *p;
    struct cvector *groups;
    const char *group, *result;
    unsigned long artnum;
    size_t count, i;
    void *search;
    struct overview_data article;

    overdata = fopen(data, "r");
    if (overdata == NULL)
        sysdie("Cannot open %s for reading", data);
    count = 0;
    while (fgets(buffer, sizeof(buffer), overdata) != NULL) {
        groups = overview_data_parse(buffer, &start, &xref);
        count += groups->count;

        /* Insert of the data.  Note that we set the arrival time and expires
           time in a deterministic fashion so that we can check later if that
           data is being stored properly. */
        article.overview = start;
        article.overlen = strlen(start);
        article.token = faketoken;
        article.arrived = count;
        article.expires = count;
        ok(n++, overview_add_xref(overview, xref, &article));

        /* Now check each group and article combination to be sure it was
           stored properly. */
        for (i = 0; i < groups->count; i++) {
            group = groups->strings[i];
            p = (char *) strchr(group, ':');
            if (p == NULL)
                die("Malformed data around %s", group);
            *p = '\0';
            artnum = strtoul(p + 1, NULL, 10);
            search = overview_search_open(overview, group, artnum, artnum);
            if (search == NULL || !overview_search(overview, search, &article))
                ok_block(n, 4, false);
            else {
                ok(n++, true);
                ok_int(n++, artnum, article.number);
                result = strchr(article.overview, '\t');
                if (result == NULL)
                    die("Malformed result from overview_search");
                result++;
                ok(n++, memcmp(start, result, strlen(start)) == 0);

                /* The expected size of the overview data is the size of the
                   input data, plus the length of the initial number and tab,
                   plus the CRLF at the end of the line. */
                ok_int(n++, strlen(start) + (result - article.overview) + 2,
                       article.overlen);
            }
        }
    }
    fclose(overdata);
    return count;
}

/* Verify that the correct total number of records have been stored. */
static void
overview_verify_count(int n, struct overview *overview, int count)
{
    int seen = 0;
    struct overview_group group;

    if (!overview_group(overview, "example.test1", &group))
        die("Cannot get group count for example.test1");
    seen += group.count;
    if (!overview_group(overview, "example.test2", &group))
        die("Cannot get group count for example.test2");
    seen += group.count;
    if (!overview_group(overview, "example.test3", &group))
        die("Cannot get group count for example.test3");
    seen += group.count;
    ok_int(n, count, seen);
}

int
main(void)
{
    int n, count;
    struct overview *overview;
    struct overview_group group = { 0, 0, 0, 'y' };
    bool value;
    char *article, *wire;
    size_t size;
    ARTHANDLE handle;
    TOKEN token;

    if (access("../data/overview/xref", F_OK) == 0)
        chdir("../data");
    else if (access("data/overview/xref", F_OK) == 0)
        chdir("data");
    else if (access("tests/data/overview/xref", F_OK) == 0)
        chdir("tests/data");

    /* 7 group/article pairs plus one check for each insert plus the final
       check for the count. */
    test_init(7 * 4 + 4 + 5);

    fake_innconf();
    overview = overview_init();
    if (!overview_group_add(overview, "example.test1", &group))
        die("Cannot add group example.test1");
    if (!overview_group_add(overview, "example.test2", &group))
        die("Cannot add group example.test2");
    if (!overview_group_add(overview, "example.test3", &group))
        die("Cannot add group example.test3");

    count = overview_load(1, "overview/xref", overview);
    n = count * 4 + 4 + 1;
    overview_verify_count(n++, overview, count);

    /* In order to test cancelling based on Xref, we have to store one of the
       articles into a real storage method to get a token.  Cheat on where the
       article is stored, since overview doesn't care. */
    value = true;
    if (!SMsetup(SM_RDWR, &value))
        die("Cannot set up storage manager");
    if (!SMinit())
        die("Cannot initialize storage manager: %s", SMerrorstr);
    article = ReadInFile("articles/xref", NULL);
    if (article == NULL)
        sysdie("Cannot read articles/xref");
    wire = wire_from_native(article, strlen(article), &size);
    free(article);
    handle.type = TOKEN_EMPTY;
    handle.data = wire;
    handle.iov = xmalloc(sizeof(struct iovec));
    handle.iov->iov_base = wire;
    handle.iov->iov_len = size;
    handle.iovcnt = 1;
    handle.len = size;
    handle.groups = (char *) "example.test:1";
    handle.groupslen = strlen("example.test:1");
    token = SMstore(handle);
    free(wire);
    free(handle.iov);
    if (token.type == TOKEN_EMPTY)
        die("Cannot store article: %s", SMerrorstr);
    ok(n++, overview_cancel_xref(overview, token));
    ok(n++, !overview_token(overview, "example.test1", 4, &token));
    ok(n++, !overview_token(overview, "example.test3", 2, &token));
    ok(n++, overview_token(overview, "example.test3", 3, &token));

    system("/bin/rm -rf ov-tmp spool");
    return 0;
}
