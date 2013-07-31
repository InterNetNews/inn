/* $Id$ */
/* Test suite for overview API. */

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "inn/innconf.h"
#include "inn/hashtab.h"
#include "inn/messages.h"
#include "inn/overview.h"
#include "inn/vector.h"
#include "inn/libinn.h"
#include "libtest.h"
#include "inn/storage.h"

/* Used as the artificial token for all articles inserted into overview. */
static const TOKEN faketoken = { 1, 1, "" };

struct group {
    char *group;
    unsigned long count;
    unsigned long low;
    unsigned long high;
};

/* Used for walking the hash table and verifying all the group data. */
struct verify {
    struct overview *overview;
    bool status;
};

static const void *
group_key(const void *entry)
{
    const struct group *group = (const struct group *) entry;
    return group->group;
}

static bool
group_eq(const void *key, const void *entry)
{
    const char *first = key;
    const char *second;

    second = ((const struct group *) entry)->group;
    return !strcmp(first, second);
}

static void
group_del(void *entry)
{
    struct group *group = (struct group *) entry;

    free(group->group);
    free(group);
}

/* Build a stripped-down innconf struct that contains only those settings that
   the overview backend cares about.  (May still be missing additional bits
   for ovdb.)
   innconf->icdsynccount defines OVBUFF_SYNC_COUNT. */
static void
fake_innconf(void)
{
    if (innconf != NULL) {
        free(innconf->ovmethod);
        free(innconf->pathdb);
        free(innconf->pathetc);
        free(innconf->pathoverview);
        free(innconf->pathrun);
        free(innconf);
    }
    innconf = xmalloc(sizeof(*innconf));
    innconf->enableoverview = true;
    innconf->groupbaseexpiry = true;
    innconf->icdsynccount = 10;
    innconf->keepmmappedthreshold = 1024;
    innconf->nfsreader = false;
    innconf->overcachesize = 20;
    innconf->ovgrouppat = NULL;
    innconf->pathdb = xstrdup("ov-tmp");
    innconf->pathetc = xstrdup("etc");
    innconf->pathoverview = xstrdup("ov-tmp");
    innconf->pathrun = xstrdup("ov-tmp");
}

/* Initialize an empty buffindexed buffer. */
static void
overview_init_buffindexed(void)
{
    int fd, i;
    char zero[1024];

    fd = open("ov-tmp/buffer", O_CREAT | O_TRUNC | O_WRONLY, 0666);
    if (fd < 0)
        sysdie("Cannot create ov-tmp/buffer");
    memset(zero, 0, sizeof(zero));
    for (i = 0; i < 1024; i++)
        if (write(fd, zero, sizeof(zero)) < (ssize_t) sizeof(zero))
            sysdie("Cannot write to ov-tmp/buffer");
    close(fd);
}

/* Initialize the overview database. */
static struct overview *
overview_init(void)
{
    system("/bin/rm -rf ov-tmp");
    if (mkdir("ov-tmp", 0755))
        sysdie("Cannot mkdir ov-tmp");
    if (strcmp(innconf->ovmethod, "buffindexed") == 0)
        overview_init_buffindexed();
    return overview_open(OV_READ | OV_WRITE);
}

/* Check to be sure that the line wasn't too long, and then parse the
   beginning of the line from one of our data files, setting the article
   number (via the passed pointer) and returning a pointer to the beginning of
   the real overview data.  This function nul-terminates the group name and
   leaves it at the beginning of the buffer.  (Ugly interface, but it's just a
   test suite.) */
static char *
overview_data_parse(char *data, unsigned long *artnum)
{
    char *start;
    size_t length;

    length = strlen(data);
    if (data[length - 1] != '\n')
        die("Line too long in input data");
    data[length - 1] = '\0';

    start = strchr(data, ':');
    if (start == NULL)
        die("No colon found in input data");
    *start = '\0';
    start++;
    *artnum = strtoul(start, NULL, 10);
    if (artnum == 0)
        die("Cannot parse article number in input data");
    return start;
}

/* Load an empty overview database from a file, in the process populating a
   hash table with each group, the high water mark, and the count of messages
   that should be in the group.  Returns the hash table on success and dies on
   failure.  Takes the name of the data file to load and the overview
   struct. */
static struct hash *
overview_load(const char *data, struct overview *overview)
{
    struct hash *groups;
    struct group *group;
    FILE *overdata;
    char buffer[4096];
    char *start;
    unsigned long artnum;
    struct overview_group stats = { 0, 0, 0, NF_FLAG_OK };
    struct overview_data article;

    /* Run through the overview data.  Each time we see a group, we update our
       stored information about that group, which we'll use for verification
       later.  We store that in a local hash table. */
    groups = hash_create(32, hash_string, group_key, group_eq, group_del);
    if (groups == NULL)
        die("Cannot create a hash table");
    overdata = fopen(data, "r");
    if (overdata == NULL)
        sysdie("Cannot open %s for reading", data);
    while (fgets(buffer, sizeof(buffer), overdata) != NULL) {
        start = overview_data_parse(buffer, &artnum);

        /* The overview API adds the article number, so strip that out before
           storing the data (overview_data_parse leaves it in because we want
           it in for data validation). */
        start = strchr(start, '\t');
        if (start == NULL)
            die("No tab found after number in input data");
        *start = '\0';
        start++;

        /* See if we've already seen this group.  If not, create it in the
           overview and the hash table; otherwise, update our local hash table
           entry. */
        group = hash_lookup(groups, buffer);
        if (group == NULL) {
            group = xmalloc(sizeof(struct group));
            group->group = xstrdup(buffer);
            group->count = 1;
            group->low = artnum;
            group->high = artnum;
            if (!hash_insert(groups, group->group, group))
                die("Cannot insert %s into hash table", group->group);
            if (!overview_group_add(overview, group->group, &stats))
                die("Cannot insert group %s into overview", group->group);
        } else {
            group->count++;
            group->low = (artnum < group->low) ? artnum : group->low;
            group->high = (artnum > group->high) ? artnum : group->high;
        }

        /* Do the actual insert of the data.  Note that we set the arrival
           time and expires time in a deterministic fashion so that we can
           check later if that data is being stored properly. */
        article.number = artnum;
        article.overview = start;
        article.overlen = strlen(start);
        article.token = faketoken;
        article.arrived = artnum * 10;
        article.expires = (artnum % 5 == 0) ? artnum * 100 : artnum;
        if (!overview_add(overview, group->group, &article))
            die("Cannot insert %s:%lu into overview", group->group, artnum);
    }
    fclose(overdata);
    return groups;
}

/* Verify that all of the group data looks correct; this is low mark, high
   mark, and article count.  Returns true if all the data is right, false
   otherwise.  This function is meant to be called as a hash traversal
   function, which means that it will be called for each element in our local
   hash table of groups with the group struct as the first argument and a
   pointer to a struct verify as the second argument. */
static void
overview_verify_groups(void *data, void *cookie)
{
    struct group *group = (struct group *) data;
    struct verify *verify = cookie;
    struct overview_group stats;

    if (!overview_group(verify->overview, group->group, &stats)) {
        warn("Unable to get data for %s", group->group);
        verify->status = false;
        return;
    }
    if (stats.low != group->low) {
        warn("Low article wrong for %s: %lu != %lu", group->group,
             stats.low, group->low);
        verify->status = false;
    }
    if (stats.high != group->high) {
        warn("High article wrong for %s: %lu != %lu", group->group,
             stats.high, group->high);
        verify->status = false;
    }
    if (stats.count != group->count) {
        warn("Article count wrong for %s: %lu != %lu", group->group,
             stats.count, group->count);
        verify->status = false;
    }
    if (stats.flag != NF_FLAG_OK) {
        warn("Flag wrong for %s: %c != %c", group->group, stats.flag,
             NF_FLAG_OK);
        verify->status = false;
    }
}

/* Verify the components of the overview data for a particular entry. */
static bool
check_data(const char *group, ARTNUM artnum, const char *expected,
           struct overview_data *data)
{
    bool status = true;
    time_t expires UNUSED; // See below why it is still unused.
    char *saw;

    if (artnum != data->number) {
        warn("Incorrect article number in search for %s:%lu: %lu != %lu",
             group, artnum, data->number, artnum);
        status = false;
    }
    if (strlen(expected) != data->overlen - 2) {
        warn("Length wrong for %s:%lu: %lu != %lu", group, artnum,
             (unsigned long) data->overlen, (unsigned long) strlen(expected));
        status = false;
    }
    if (memcmp(&data->token, &faketoken, sizeof(faketoken)) != 0) {
        warn("Token wrong for %s:%lu", group, artnum);
        status = false;
    }
    if (memcmp(expected, data->overview, data->overlen - 2) != 0) {
        warn("Data mismatch for %s:%lu", group, artnum);
        saw = xstrndup(data->overview, data->overlen);
        warn("====\n%s====\n%s====", expected, saw);
        free(saw);
        status = false;
    }
    if (memcmp("\r\n", data->overview + data->overlen - 2, 2) != 0) {
        warn("Missing CRLF after data for %s:%lu", group, artnum);
        status = false;
    }
    if (data->arrived != (time_t) artnum * 10) {
        warn("Arrival time wrong for %s:%lu: %lu != %lu", group, artnum,
             (unsigned long) data->arrived, artnum * 10);
        status = false;
    }

    /* expires is always 0 for right now because the underlying API doesn't
       return it; this will change when the new API has been pushed all the
       way down to the overview implementations.  */
    expires = (artnum % 5 == 0) ? artnum * 100 : artnum;
    if (data->expires != 0) {
        warn("Expires time wrong for %s:%lu: %lu != %lu", group, artnum,
             (unsigned long) data->expires, 0UL);
        status = false;
    }
    return status;
}

/* Read through the data again, looking up each article as we go and verifying
   that the data stored in overview is the same as the data we put there.  Do
   this two ways each time, once via overview_token and once via
   overview_search.  Return true if everything checks out, false otherwise.
   Takes the path to the data file and the overview struct. */
static bool
overview_verify_data(const char *data, struct overview *overview)
{
    FILE *overdata;
    char buffer[4096];
    char *start;
    unsigned long artnum;
    TOKEN token;
    void *search;
    struct overview_data article;
    bool status = true;

    overdata = fopen(data, "r");
    if (overdata == NULL)
        sysdie("Cannot open %s for reading", data);
    while (fgets(buffer, sizeof(buffer), overdata) != NULL) {
        start = overview_data_parse(buffer, &artnum);

        /* Now check that the overview data is correct for that group. */
        if (!overview_token(overview, buffer, artnum, &token)) {
            warn("No overview data found for %s:%lu", buffer, artnum);
            status = false;
            continue;
        }
        if (memcmp(&token, &faketoken, sizeof(token)) != 0) {
            warn("Token wrong for %s:%lu", buffer, artnum);
            status = false;
        }

        /* Do the same thing, except use search. */
        search = overview_search_open(overview, buffer, artnum, artnum);
        if (search == NULL) {
            warn("Unable to open search for %s:%lu", buffer, artnum);
            status = false;
            continue;
        }
        if (!overview_search(overview, search, &article)) {
            warn("No overview data found for %s:%lu", buffer, artnum);
            status = false;
            continue;
        }
        if (!check_data(buffer, artnum, start, &article))
            status = false;
        if (overview_search(overview, search, &article)) {
            warn("Unexpected article found for %s:%lu", buffer, artnum);
            status = false;
        }
        overview_search_close(overview, search);
    }
    fclose(overdata);
    return status;
}

/* Try an overview search and verify that all of the data is returned in the
   right order.  The first group mentioned in the provided data file will be
   the group the search is done in, and the search will cover all articles
   from the second article to the second-to-the-last article in the group.
   Returns true if everything checks out, false otherwise. */
static bool
overview_verify_search(const char *data, struct overview *overview)
{
    unsigned long artnum, i;
    unsigned long start = 0;
    unsigned long end = 0;
    unsigned long last = 0;
    struct vector *expected;
    char *group, *line;
    FILE *overdata;
    char buffer[4096];
    void *search;
    struct overview_data article;
    bool status = true;

    overdata = fopen(data, "r");
    if (overdata == NULL)
        sysdie("Cannot open %s for reading", data);
    expected = vector_new();
    if (fgets(buffer, sizeof(buffer), overdata) == NULL)
        die("Unexpected end of file in %s", data);
    overview_data_parse(buffer, &artnum);
    group = xstrdup(buffer);
    while (fgets(buffer, sizeof(buffer), overdata) != NULL) {
        line = overview_data_parse(buffer, &artnum);
        if (strcmp(group, buffer) != 0)
            continue;
        vector_add(expected, line);
        if (start == 0)
            start = artnum;
        end = last;
        last = artnum;
    }
    search = overview_search_open(overview, group, start, end);
    if (search == NULL) {
        warn("Unable to open search for %s:%lu", buffer, start);
        free(group);
        vector_free(expected);
        return false;
    }
    i = 0;
    while (overview_search(overview, search, &article)) {
        if (!check_data(group, article.number, expected->strings[i], &article))
            status = false;
        i++;
    }
    overview_search_close(overview, search);
    if (article.number != end) {
        warn("End of search in %s wrong: %lu != %lu", group, article.number,
             end);
        status = false;
    }
    if (i != expected->count - 1) {
        warn("Didn't see all expected entries in %s", group);
        status = false;
    }
    free(group);
    vector_free(expected);
    return status;
}

/* Try an overview search and verify that all of the data is returned in the
   right order.  The search will cover everything from article 1 to the
   highest numbered article plus one.  There were some problems with a search
   low water mark lower than the base of the group.  Returns true if
   everything checks out, false otherwise. */
static bool
overview_verify_full_search(const char *data, struct overview *overview)
{
    unsigned long artnum, i;
    unsigned long end = 0;
    struct vector *expected;
    char *line;
    char *group = NULL;
    FILE *overdata;
    char buffer[4096];
    void *search;
    struct overview_data article;
    bool status = true;

    overdata = fopen(data, "r");
    if (overdata == NULL)
        sysdie("Cannot open %s for reading", data);
    expected = vector_new();
    while (fgets(buffer, sizeof(buffer), overdata) != NULL) {
        line = overview_data_parse(buffer, &artnum);
        if (group == NULL)
            group = xstrdup(buffer);
        vector_add(expected, line);
        end = artnum;
    }
    search = overview_search_open(overview, group, 1, end + 1);
    if (search == NULL) {
        warn("Unable to open full search for %s", group);
        free(group);
        vector_free(expected);
        return false;
    }
    i = 0;
    while (overview_search(overview, search, &article)) {
        if (!check_data(group, article.number, expected->strings[i], &article))
            status = false;
        i++;
    }
    overview_search_close(overview, search);
    if (article.number != end) {
        warn("End of search in %s wrong: %lu != %lu", group, article.number,
             end);
        status = false;
    }
    if (i != expected->count) {
        warn("Didn't see all expected entries in %s", group);
        status = false;
    }
    free(group);
    vector_free(expected);
    return status;
}

/* Run the tests on a particular overview setup.  Expects to be called
   multiple times with different inn.conf configurations to test the various
   iterations of overview support.  Takes the current test number and returns
   the next test number. */
static int
overview_tests(int n)
{
    struct hash *groups;
    struct overview *overview;
    struct verify verify;
    void *search;
    struct overview_data data;
    TOKEN token;

    overview = overview_init();
    if (overview == NULL)
        die("Opening the overview database failed, cannot continue");
    ok(n++, true);

    groups = overview_load("overview/basic", overview);
    ok(n++, true);
    verify.status = true;
    verify.overview = overview;
    hash_traverse(groups, overview_verify_groups, &verify);
    ok(n++, verify.status);
    ok(n++, overview_verify_data("overview/basic", overview));
    ok(n++, overview_verify_search("overview/basic", overview));
    hash_free(groups);
    overview_close(overview);
    ok(n++, true);

    overview = overview_init();
    if (overview == NULL)
        die("Opening the overview database failed, cannot continue");
    ok(n++, true);

    groups = overview_load("overview/reversed", overview);
    ok(n++, true);
    verify.status = true;
    verify.overview = overview;
    hash_traverse(groups, overview_verify_groups, &verify);
    ok(n++, verify.status);
    ok(n++, overview_verify_data("overview/basic", overview));
    ok(n++, overview_verify_search("overview/basic", overview));
    hash_free(groups);
    overview_close(overview);
    ok(n++, true);

    overview = overview_init();
    if (overview == NULL)
        die("Opening the overview database failed, cannot continue");
    ok(n++, true);

    groups = overview_load("overview/high-numbered", overview);
    ok(n++, true);
    ok(n++, overview_verify_data("overview/high-numbered", overview));
    ok(n++, overview_verify_full_search("overview/high-numbered", overview));
    if (strcmp(innconf->ovmethod, "buffindexed") == 0) {
        skip_block(n, 6, "buffindexed doesn't support cancel");
        n += 6;
    } else {
        ok(n++, overview_cancel(overview, "example.test", 7498));
        ok(n++, !overview_token(overview, "example.test", 7498, &token));
        search = overview_search_open(overview, "example.test", 7498, 7499);
        ok(n++, search != NULL);
        ok(n++, overview_search(overview, search, &data));
        ok_int(n++, 7499, data.number);
        ok(n++, !overview_search(overview, search, &data));
        overview_search_close(overview, search);
    }
    hash_free(groups);
    overview_close(overview);
    ok(n++, true);

    overview = overview_init();
    if (overview == NULL)
        die("Opening the overview database failed, cannot continue");
    ok(n++, true);

    groups = overview_load("overview/bogus", overview);
    ok(n++, true);
    ok(n++, overview_verify_data("overview/bogus", overview));
    hash_free(groups);
    overview_close(overview);
    system("/bin/rm -rf ov-tmp");
    ok(n++, true);
    return n;
}

/* Run the tests on a particular overview setup.  This is a copy of
   overview_tests that closes and reopens the overview without mmap to test
   tradindexedmmap.  Takes the current test and returns the next test
   number. */
static int
overview_mmap_tests(int n)
{
    struct hash *groups;
    struct overview *overview;
    struct verify verify;

    overview = overview_init();
    if (overview == NULL)
        die("Opening the overview database failed, cannot continue");
    ok(n++, true);

    groups = overview_load("overview/basic", overview);
    ok(n++, true);
    overview_close(overview);
    innconf->tradindexedmmap = false;
    overview = overview_open(OV_READ);
    if (overview == NULL)
        die("Opening the overview database failed, cannot continue");
    verify.status = true;
    verify.overview = overview;
    hash_traverse(groups, overview_verify_groups, &verify);
    ok(n++, verify.status);
    ok(n++, overview_verify_data("overview/basic", overview));
    ok(n++, overview_verify_search("overview/basic", overview));
    hash_free(groups);
    overview_close(overview);
    ok(n++, true);

    innconf->tradindexedmmap = true;
    overview = overview_init();
    if (overview == NULL)
        die("Opening the overview database failed, cannot continue");
    ok(n++, true);

    groups = overview_load("overview/reversed", overview);
    ok(n++, true);
    overview_close(overview);
    innconf->tradindexedmmap = false;
    overview = overview_open(OV_READ);
    if (overview == NULL)
        die("Opening the overview database failed, cannot continue");
    verify.status = true;
    verify.overview = overview;
    hash_traverse(groups, overview_verify_groups, &verify);
    ok(n++, verify.status);
    ok(n++, overview_verify_data("overview/basic", overview));
    ok(n++, overview_verify_search("overview/basic", overview));
    hash_free(groups);
    overview_close(overview);
    ok(n++, true);

    innconf->tradindexedmmap = true;
    overview = overview_init();
    if (overview == NULL)
        die("Opening the overview database failed, cannot continue");
    ok(n++, true);

    groups = overview_load("overview/high-numbered", overview);
    ok(n++, true);
    overview_close(overview);
    innconf->tradindexedmmap = false;
    overview = overview_open(OV_READ);
    if (overview == NULL)
        die("Opening the overview database failed, cannot continue");
    ok(n++, overview_verify_data("overview/high-numbered", overview));
    ok(n++, overview_verify_full_search("overview/high-numbered", overview));
    hash_free(groups);
    overview_close(overview);
    ok(n++, true);

    innconf->tradindexedmmap = true;
    overview = overview_init();
    if (overview == NULL)
        die("Opening the overview database failed, cannot continue");
    ok(n++, true);

    groups = overview_load("overview/bogus", overview);
    ok(n++, true);
    overview_close(overview);
    innconf->tradindexedmmap = false;
    overview = overview_open(OV_READ);
    if (overview == NULL)
        die("Opening the overview database failed, cannot continue");
    ok(n++, overview_verify_data("overview/bogus", overview));
    hash_free(groups);
    overview_close(overview);
    system("/bin/rm -rf ov-tmp");
    ok(n++, true);
    return n;
}

int
main(void)
{
    int n = 1;

    if (access("../data/overview/basic", F_OK) == 0)
        chdir("../data");
    else if (access("data/overview/basic", F_OK) == 0)
        chdir("data");
    else if (access("tests/data/overview/basic", F_OK) == 0)
        chdir("tests/data");

    /* Cancels can't be tested with mmap, so there are only 21 tests there. */
    test_init(27 * 2 + 21);

    fake_innconf();
    innconf->ovmethod = xstrdup("tradindexed");
    innconf->tradindexedmmap = true;
    diag("tradindexed with mmap");
    n = overview_tests(1);

    diag("tradindexed without mmap");
    n = overview_mmap_tests(n);

    free(innconf->ovmethod);
    innconf->ovmethod = xstrdup("buffindexed");
    diag("buffindexed");
    n = overview_tests(n);

    return 0;
}
