/* $Id$ */
/* tradindexed test suite. */

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <sys/stat.h>

#include "inn/hashtab.h"
#include "libinn.h"
#include "libtest.h"
#include "ov.h"
#include "storage.h"

#include "../storage/ov3/tradindexed.h"

/* Used as the artificial token for all articles inserted into overview. */
static const TOKEN faketoken = { 1, 1, "" };

struct group {
    char *group;
    unsigned long count;
    unsigned long low;
    unsigned long high;
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
   the tradindexed overview method cares about. */
static void
fake_innconf(void)
{
    innconf = xmalloc(sizeof(*innconf));
    innconf->pathoverview = xstrdup("tdx-tmp");
    innconf->overcachesize = 20;
    innconf->groupbaseexpiry = true;
}

/* Initialize the overview database. */
static bool
overview_init(void)
{
    fake_innconf();
    if (access("data/basic", F_OK) < 0)
        if (access("overview/data/basic", F_OK) == 0)
            if (chdir("overview") != 0)
                sysdie("Cannot cd to overview");
    if (mkdir("tdx-tmp", 0755) != 0 && errno != EEXIST)
        sysdie("Cannot mkdir tdx-tmp");
    return tradindexed_open(OV_READ | OV_WRITE);
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

    if (data[strlen(data) - 1] != '\n')
        die("Line too long in input data");
    data[strlen(data) - 1] = '\0';

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
   failure.  Takes the name of the data file to load. */
static struct hash *
overview_load(const char *data)
{
    struct hash *groups;
    struct group *group;
    FILE *overview;
    char buffer[4096];
    char flag[] = "y";
    char *start;
    unsigned long artnum;

    /* Run through the overview data.  Each time we see a group, we update our
       stored information about that group, which we'll use for verification
       later.  We store that in a local hash table. */
    groups = hash_create(32, hash_string, group_key, group_eq, group_del);
    if (groups == NULL)
        die("Cannot create a hash table");
    overview = fopen(data, "r");
    if (overview == NULL)
        sysdie("Cannot open %s for reading", data);
    while (fgets(buffer, sizeof(buffer), overview) != NULL) {
        start = overview_data_parse(buffer, &artnum);

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
            if (!tradindexed_groupadd(group->group, 0, 0, flag))
                die("Cannot insert group %s into overview", group->group);
        } else {
            group->count++;
            group->low = (artnum < group->low) ? artnum : group->low;
            group->high = (artnum > group->high) ? artnum : group->high;
        }

        /* Do the actual insert of the data.  Note that we set the arrival
           time and expires time in a deterministic fashion so that we can
           check later if that data is being stored properly. */
        if (!tradindexed_add(group->group, artnum, faketoken, start,
                             strlen(start), artnum * 10,
                             (artnum % 5 == 0) ? artnum * 100 : artnum))
            die("Cannot insert %s:%lu into overview", group->group, artnum);
    }
    fclose(overview);
    return groups;
}

/* Verify that all of the group data looks correct; this is low mark, high
   mark, and article count.  Returns true if all the data is right, false
   otherwise.  This function is meant to be called as a hash traversal
   function, which means that it will be called for each element in our local
   hash table of groups with the group struct as the first argument and a
   pointer to a status as the second argument. */
static void
overview_verify_groups(void *data, void *cookie)
{
    struct group *group = (struct group *) data;
    bool *status = (bool *) cookie;
    int low, high, count, flag;

    if (!tradindexed_groupstats(group->group, &low, &high, &count, &flag)) {
        warn("Unable to get data for %s", group->group);
        *status = false;
        return;
    }
    if ((unsigned long) low != group->low) {
        warn("Low article wrong for %s: %lu != %lu", group->group,
             (unsigned long) low, group->low);
        *status = false;
    }
    if ((unsigned long) high != group->high) {
        warn("High article wrong for %s: %lu != %lu", group->group,
             (unsigned long) high, group->high);
        *status = false;
    }
    if ((unsigned long) count != group->count) {
        warn("Article count wrong for %s: %lu != %lu", group->group,
             (unsigned long) count, group->count);
        *status = false;
    }
    if (flag != 'y') {
        warn("Flag wrong for %s: %c != y", group->group, (char) flag);
        *status = false;
    }
}

/* Verify the components of the overview data for a particular entry. */
static bool
check_data(const char *group, unsigned long artnum, const char *expected,
           const char *seen, int length, TOKEN token)
{
    bool status = true;

    if (strlen(expected) != (size_t) length) {
        warn("Length wrong for %s:%lu: %d != %lu", group, artnum, length,
             (unsigned long) strlen(expected));
        status = false;
    }
    if (memcmp(&token, &faketoken, sizeof(token)) != 0) {
        warn("Token wrong for %s:%lu", group, artnum);
        status = false;
    }
    if (memcmp(expected, seen, length) != 0) {
        warn("Data mismatch for %s:%lu", group, artnum);
        warn("====\n%s\n====\n%s\n====", expected, seen);
        status = false;
    }
    return status;
}

/* Read through the data again, looking up each article as we go and verifying
   that the data stored in overview is the same as the data we put there.  Do
   this two ways each time, once via getartinfo and once via opensearch.
   Return true if everything checks out, false otherwise.  Takes the path to
   the data file. */
static bool
overview_verify_data(const char *data)
{
    FILE *overdata;
    char buffer[4096];
    char *start;
    unsigned long artnum, overnum;
    char *overview;
    int length;
    TOKEN token;
    bool status = true;
    void *search;
    time_t arrived;

    overdata = fopen(data, "r");
    if (overdata == NULL)
        sysdie("Cannot open %s for reading", data);
    while (fgets(buffer, sizeof(buffer), overdata) != NULL) {
        start = overview_data_parse(buffer, &artnum);

        /* Now check that the overview data is correct for that group. */
        if (!tradindexed_getartinfo(buffer, artnum, NULL, NULL, &token)) {
            warn("No overview data found for %s:%lu", buffer, artnum);
            status = false;
            continue;
        }
        if (memcmp(&token, &faketoken, sizeof(token)) != 0) {
            warn("Token wrong for %s:%lu", buffer, artnum);
            status = false;
        }

        /* Do the same thing, except use search. */
        search = tradindexed_opensearch(buffer, artnum, artnum);
        if (search == NULL) {
            warn("Unable to open search for %s:%lu", buffer, artnum);
            status = false;
            continue;
        }
        if (!tradindexed_search(search, &overnum, &overview, &length, &token,
                                &arrived)) {
            warn("No overview data found for %s:%lu", buffer, artnum);
            status = false;
            continue;
        }
        if (overnum != artnum) {
            warn("Incorrect article number in search for %s:%lu: %lu != %lu",
                 buffer, artnum, overnum, artnum);
            status = false;
        }
        if (!check_data(buffer, artnum, start, overview, length, token))
            status = false;
        if ((unsigned long) arrived != artnum * 10) {
            warn("Arrival time wrong for %s:%lu: %lu != %lu", buffer, artnum,
                 (unsigned long) arrived, artnum * 10);
            status = false;
        }
        if (tradindexed_search(search, &overnum, &overview, &length, &token,
                               &arrived)) {
            warn("Unexpected article found for %s:%lu", buffer, artnum);
            status = false;
        }
        tradindexed_closesearch(search);
    }
    fclose(overdata);
    return status;
}

int
main(void)
{
    struct hash *groups;
    bool status;

    puts("10");

    if (!overview_init())
        die("Opening the overview database failed, cannot continue");
    ok(1, true);

    groups = overview_load("data/basic");
    ok(2, true);
    status = true;
    hash_traverse(groups, overview_verify_groups, &status);
    ok(3, status);
    ok(4, overview_verify_data("data/basic"));
    tradindexed_close();
    system("/bin/rm -r tdx-tmp");
    ok(5, true);

    if (!overview_init())
        die("Opening the overview database failed, cannot continue");
    ok(6, true);

    groups = overview_load("data/reversed");
    ok(7, true);
    status = true;
    hash_traverse(groups, overview_verify_groups, &status);
    ok(8, status);
    ok(9, overview_verify_data("data/basic"));
    tradindexed_close();
    system("/bin/rm -r tdx-tmp");
    ok(10, true);

    return 0;
}
