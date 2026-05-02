/* Reader half of the ovsqlite direct reader integration test.
 *
 * Opens overview in OV_READ mode (triggers direct_open for WAL databases),
 * reads back data written by the writer half, and verifies correctness.
 * No ovsqlite-server is needed -- this is the whole point of direct reader.
 *
 * Important: OVadd determines group:artnum from the Xref header in the
 * overview data, NOT from the prefix in the test data file.  For most
 * articles these match, but some test articles intentionally differ
 * (e.g., file says news.groups:260 but Xref says news.groups:60).
 * We must parse Xref to know what was actually stored.
 */

#include "portable/system.h"

#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#include "inn/hashtab.h"
#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/ov.h"
#include "inn/storage.h"
#include "inn/vector.h"
#include "tap/basic.h"

static const TOKEN faketoken = {1, 1, ""};

struct group {
    char *group;
    unsigned long count;
    unsigned long low;
    unsigned long high;
};

static const void *
group_key(const void *entry)
{
    return ((const struct group *) entry)->group;
}

static bool
group_eq(const void *key, const void *entry)
{
    return !strcmp((const char *) key,
                  ((const struct group *) entry)->group);
}

static void
group_del(void *entry)
{
    struct group *g = (struct group *) entry;
    free(g->group);
    free(g);
}

/* Parse the Xref field from an overview line to find group:artnum pairs.
 * The overview line format includes "Xref: hostname group:artnum ..."
 * as one of the tab-separated fields.
 *
 * For each group:artnum in the Xref, calls the callback with the group
 * name, artnum, the file-format artnum, and the full line from the file.
 * Returns the number of Xref entries processed. */
struct xref_entry {
    char group[256];
    unsigned long artnum;
};

/* Find the last Xref field in an overview line and extract group:artnum
 * pairs.  Returns the number of entries found (up to max_entries). */
static int
parse_xref(const char *data, size_t datalen, struct xref_entry *entries,
           int max_entries)
{
    const char *xref = NULL;
    const char *p;
    int n = 0;

    /* Find last "Xref: " preceded by a tab. */
    for (p = data; p < data + datalen - 6; p++) {
        if (*p == 'X' && memcmp(p, "Xref: ", 6) == 0
            && (p == data || p[-1] == '\t'))
            xref = p;
    }
    if (!xref)
        return 0;

    /* Skip "Xref: hostname " */
    xref += 6;
    while (xref < data + datalen && *xref != ' ')
        xref++;
    if (xref < data + datalen)
        xref++;

    /* Parse group:artnum pairs. */
    while (xref < data + datalen && n < max_entries) {
        const char *colon, *end;
        size_t glen;

        while (xref < data + datalen && isspace((unsigned char) *xref))
            xref++;
        if (xref >= data + datalen)
            break;

        colon = memchr(xref, ':', data + datalen - xref);
        if (!colon)
            break;
        glen = colon - xref;
        if (glen >= sizeof(entries[0].group))
            break;
        memcpy(entries[n].group, xref, glen);
        entries[n].group[glen] = '\0';

        entries[n].artnum = strtoul(colon + 1, NULL, 10);
        if (entries[n].artnum == 0)
            break;
        n++;

        /* Advance to next space or end. */
        end = memchr(xref, ' ', data + datalen - xref);
        if (!end)
            end = memchr(xref, '\t', data + datalen - xref);
        if (!end)
            break;
        xref = end + 1;
    }
    return n;
}

/* Build expected group stats from the data file, using Xref-derived
 * artnums (which is what OVadd actually stores). */
static struct hash *
load_expected(const char *path)
{
    struct hash *groups;
    struct group *g;
    FILE *f;
    char buffer[4096];
    struct xref_entry entries[16];
    int i, nxref;
    size_t len;

    groups = hash_create(32, hash_string, group_key, group_eq, group_del);
    f = fopen(path, "r");
    if (f == NULL)
        sysdie("Cannot open %s", path);

    while (fgets(buffer, sizeof(buffer), f) != NULL) {
        len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n')
            len--;

        nxref = parse_xref(buffer, len, entries, 16);
        for (i = 0; i < nxref; i++) {
            g = hash_lookup(groups, entries[i].group);
            if (g == NULL) {
                g = xmalloc(sizeof(*g));
                g->group = xstrdup(entries[i].group);
                g->count = 1;
                g->low = entries[i].artnum;
                g->high = entries[i].artnum;
                hash_insert(groups, g->group, g);
            } else {
                g->count++;
                if (entries[i].artnum < g->low)
                    g->low = entries[i].artnum;
                if (entries[i].artnum > g->high)
                    g->high = entries[i].artnum;
            }
        }
    }
    fclose(f);
    return groups;
}

/* Verify group stats via the OV API. */
static void
verify_group(void *data, void *cookie)
{
    struct group *g = (struct group *) data;
    bool *status = (bool *) cookie;
    int low, high, count, flag;

    if (!OVgroupstats(g->group, &low, &high, &count, &flag)) {
        warn("No stats for %s", g->group);
        *status = false;
        return;
    }
    if ((unsigned long) low != g->low) {
        warn("Low wrong for %s: %d != %lu", g->group, low, g->low);
        *status = false;
    }
    if ((unsigned long) high != g->high) {
        warn("High wrong for %s: %d != %lu", g->group, high, g->high);
        *status = false;
    }
    if ((unsigned long) count != g->count) {
        warn("Count wrong for %s: %d != %lu", g->group, count, g->count);
        *status = false;
    }
}

/* Verify per-article data: getartinfo returns correct token, search returns
 * correct overview data and arrival time.
 *
 * We verify using the Xref-derived artnum (what OVadd actually stored).
 * The arrival time is based on the file-prefix artnum (what the writer
 * passed as arrived = artnum * 10). */
static bool
verify_articles(const char *path)
{
    FILE *f;
    char buffer[4096];
    char *start, *colon;
    unsigned long file_artnum;
    struct xref_entry entries[16];
    int nxref, i;
    size_t linelen;
    TOKEN token;
    void *search;
    ARTNUM overnum;
    char *data;
    int len;
    time_t arrived;
    bool status = true;

    f = fopen(path, "r");
    if (f == NULL)
        sysdie("Cannot open %s", path);

    while (fgets(buffer, sizeof(buffer), f) != NULL) {
        linelen = strlen(buffer);
        if (linelen > 0 && buffer[linelen - 1] == '\n')
            linelen--;

        /* Parse the file-format artnum (used for arrival time). */
        colon = strchr(buffer, ':');
        if (!colon)
            continue;
        start = colon + 1;
        file_artnum = strtoul(start, NULL, 10);

        /* Parse Xref to get the actual stored artnum(s). */
        nxref = parse_xref(buffer, linelen, entries, 16);
        for (i = 0; i < nxref; i++) {
            /* getartinfo should find this article. */
            if (!OVgetartinfo(entries[i].group, entries[i].artnum, &token)) {
                warn("No artinfo for %s:%lu", entries[i].group,
                     entries[i].artnum);
                status = false;
                continue;
            }
            if (memcmp(&token, &faketoken, sizeof(token)) != 0) {
                warn("Token wrong for %s:%lu", entries[i].group,
                     entries[i].artnum);
                status = false;
            }

            /* Search for this article and verify arrival time. */
            search = OVopensearch(entries[i].group, entries[i].artnum,
                                  entries[i].artnum);
            if (search == NULL) {
                warn("Cannot open search for %s:%lu", entries[i].group,
                     entries[i].artnum);
                status = false;
                continue;
            }
            if (!OVsearch(search, &overnum, &data, &len, &token, &arrived)) {
                warn("No search result for %s:%lu", entries[i].group,
                     entries[i].artnum);
                status = false;
                OVclosesearch(search);
                continue;
            }

            /* Verify the arrival time matches what the writer stored.
             * The writer uses file_artnum * 10 as the arrival time. */
            if ((unsigned long) arrived != file_artnum * 10) {
                warn("Arrived wrong for %s:%lu: %lu != %lu",
                     entries[i].group, entries[i].artnum,
                     (unsigned long) arrived, file_artnum * 10);
                status = false;
            }

            /* There should be no more results. */
            if (OVsearch(search, &overnum, &data, &len, &token, &arrived)) {
                warn("Extra article for %s:%lu", entries[i].group,
                     entries[i].artnum);
                status = false;
            }
            OVclosesearch(search);
        }
    }
    fclose(f);
    return status;
}

/* Verify that a multi-article search returns articles in order for the
 * first group found in the data file. */
static bool
verify_search_order(const char *path)
{
    FILE *f;
    char buffer[4096];
    struct xref_entry entries[16];
    int nxref;
    size_t len;
    char *group = NULL;
    unsigned long low = 0, high = 0;
    unsigned long article_count = 0;
    void *search;
    ARTNUM overnum, last = 0;
    char *data;
    int dlen;
    TOKEN token;
    time_t arrived;
    unsigned long found = 0;
    bool status = true;

    f = fopen(path, "r");
    if (f == NULL)
        sysdie("Cannot open %s", path);

    /* Scan the file to find the range for the first group. */
    while (fgets(buffer, sizeof(buffer), f) != NULL) {
        len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n')
            len--;
        nxref = parse_xref(buffer, len, entries, 16);
        if (nxref == 0)
            continue;
        if (group == NULL) {
            group = xstrdup(entries[0].group);
            low = entries[0].artnum;
            high = entries[0].artnum;
            article_count = 1;
        } else if (strcmp(group, entries[0].group) == 0) {
            if (entries[0].artnum < low)
                low = entries[0].artnum;
            if (entries[0].artnum > high)
                high = entries[0].artnum;
            article_count++;
        }
    }
    fclose(f);

    if (!group) {
        warn("No group found in %s", path);
        return false;
    }

    search = OVopensearch(group, low, high);
    if (search == NULL) {
        warn("Cannot open search for %s:%lu-%lu", group, low, high);
        free(group);
        return false;
    }

    while (OVsearch(search, &overnum, &data, &dlen, &token, &arrived)) {
        if (overnum <= last && last != 0) {
            warn("Out of order in %s: %lu after %lu", group,
                 (unsigned long) overnum, (unsigned long) last);
            status = false;
        }
        last = overnum;
        found++;
    }
    OVclosesearch(search);

    if (found != article_count) {
        warn("Wrong count in search for %s: %lu != %lu", group, found,
             article_count);
        status = false;
    }

    free(group);
    return status;
}

int
main(void)
{
    struct hash *groups;
    bool status;

    test_init(5);

    if (access("../data/overview/basic", F_OK) == 0) {
        if (chdir("../data") < 0)
            sysbail("cannot chdir to ../data");
    } else if (access("data/overview/basic", F_OK) == 0) {
        if (chdir("data") < 0)
            sysbail("cannot chdir to data");
    } else if (access("tests/data/overview/basic", F_OK) == 0) {
        if (chdir("tests/data") < 0)
            sysbail("cannot chdir to tests/data");
    }

    /* Test 1: open in direct reader mode (OV_READ triggers direct_open). */
    ok(1, OVopen(OV_READ));

    /* Test 2: group stats match expected values. */
    groups = load_expected("overview/basic");
    status = true;
    hash_traverse(groups, verify_group, &status);
    ok(2, status);

    /* Test 3: per-article data integrity (token, overview, arrived). */
    ok(3, verify_articles("overview/basic"));

    /* Test 4: multi-article search returns articles in order. */
    ok(4, verify_search_order("overview/basic"));

    hash_free(groups);
    OVclose();

    /* Test 5: close and reopen works. */
    ok(5, OVopen(OV_READ));
    OVclose();

    return 0;
}
