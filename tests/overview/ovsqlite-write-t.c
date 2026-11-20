/* Writer half of the ovsqlite direct reader integration test.
 *
 * Opens overview in OV_READ|OV_WRITE mode (connects to ovsqlite-server),
 * creates groups and inserts articles via the standard OV API, then closes.
 * The server creates the real database schema and handles compression.
 *
 * Written by Kevin Bowling in 2026.
 */

#include "portable/system.h"

#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifdef HAVE_SQLITE3
#    include <sqlite3.h>
#endif

#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/ov.h"
#include "inn/storage.h"
#include "tap/basic.h"

/* The same fake token used by overview-t.c. */
static const TOKEN faketoken = {1, 1, ""};

#ifdef HAVE_SQLITE3
/* The server commits a WAL transaction after 250 ms without a modifying
   request.  Stay clearly below that when deciding that no commit may have
   happened yet, and wait much longer than that for one to appear. */
#    define IDLE_SAFE_USEC 200000L
#    define COMMIT_WAIT_SEC 3
#    define BURST_ARTICLES 8
#    define BURST_GAP_USEC 100000
#    define BUSY_TIMEOUT_MSEC 2000

/* An independent connection to the database while the writer stays open.
   Resetting the count statement ends each read snapshot, so polling cannot
   pin old data. */
struct snapshot {
    sqlite3 *db;
    sqlite3_stmt *stmt;
};

static long
usec_since(const struct timeval *then)
{
    struct timeval now;

    gettimeofday(&now, NULL);
    return (now.tv_sec - then->tv_sec) * 1000000L
           + (now.tv_usec - then->tv_usec);
}

/* Return 1 if the database is in WAL mode, 0 if it is not, -1 on error. */
static int
snapshot_open(struct snapshot *snap)
{
    sqlite3_stmt *stmt = NULL;
    char *path;
    int status, wal = -1;

    snap->db = NULL;
    snap->stmt = NULL;
    path = concatpath(innconf->pathoverview, "ovsqlite.db");
    status = sqlite3_open_v2(path, &snap->db, SQLITE_OPEN_READONLY, NULL);
    free(path);
    if (status != SQLITE_OK)
        goto error;

    /* The writer or a checkpoint can hold a lock for a moment: wait for it
       instead of failing the test with SQLITE_BUSY. */
    sqlite3_busy_timeout(snap->db, BUSY_TIMEOUT_MSEC);
    status = sqlite3_prepare_v2(snap->db, "pragma journal_mode", -1, &stmt,
                                NULL);
    if (status != SQLITE_OK || sqlite3_step(stmt) != SQLITE_ROW
        || sqlite3_column_text(stmt, 0) == NULL)
        goto error;
    wal = strcmp((const char *) sqlite3_column_text(stmt, 0), "wal") == 0;
    sqlite3_finalize(stmt);
    if (!wal)
        return 0;
    status = sqlite3_prepare_v2(snap->db, "select count(*) from artinfo", -1,
                                &snap->stmt, NULL);
    if (status == SQLITE_OK)
        return 1;
    stmt = NULL;

error:
    diag("independent SQLite read failed: %s",
         snap->db == NULL ? "no connection" : sqlite3_errmsg(snap->db));
    sqlite3_finalize(stmt);
    return -1;
}

/* Count the committed articles with a fresh snapshot, or -1 on error. */
static long long
snapshot_count(struct snapshot *snap)
{
    long long count;

    if (sqlite3_step(snap->stmt) != SQLITE_ROW) {
        diag("independent SQLite read failed: %s", sqlite3_errmsg(snap->db));
        return -1;
    }
    count = sqlite3_column_int64(snap->stmt, 0);
    sqlite3_reset(snap->stmt);
    return count;
}

static void
snapshot_close(struct snapshot *snap)
{
    sqlite3_finalize(snap->stmt);
    if (snap->db != NULL)
        sqlite3_close(snap->db);
}

/* Poll until the snapshot shows the expected count, for at most
   COMMIT_WAIT_SEC seconds.  If group is not NULL, a read request through
   the writer connection (OVgroupstats() on that group) precedes every
   poll, and last_write is when the final write request was started.
   Return 1 if the count was reached, 0 if not, and -1 if it was reached
   but the requests were so far apart that the server could have committed
   in a gap, even if read requests postponed its deadline: then the check
   proves nothing.  A gap is measured from the start of a request to the
   end of the next one, so it never underestimates what the server saw.
   The wait for the commit itself is not a gap, as the reads continue
   through it. */
static int
wait_for_commit(struct snapshot *snap, long long expected, const char *group,
                const struct timeval *last_write)
{
    struct timeval start, request = {0, 0};
    long long count;
    long gap, longest = 0;
    int lo, hi, n, flag;

    if (group != NULL)
        request = *last_write;
    gettimeofday(&start, NULL);
    for (;;) {
        if (group != NULL) {
            struct timeval began;

            gettimeofday(&began, NULL);
            if (!OVgroupstats((char *) group, &lo, &hi, &n, &flag))
                return 0;
            gap = usec_since(&request);
            if (gap > longest)
                longest = gap;
            request = began;
        }
        count = snapshot_count(snap);
        if (count == expected) {
            if (group != NULL) {
                gap = usec_since(&request);
                if (gap > longest)
                    longest = gap;
                if (longest >= IDLE_SAFE_USEC) {
                    diag("requests were up to %ld ms apart", longest / 1000);
                    return -1;
                }
            }
            return 1;
        }
        if (count < 0 || usec_since(&start) >= COMMIT_WAIT_SEC * 1000000L)
            break;
        usleep(group != NULL ? 50000 : 10000);
    }
    diag("independent committed rows: %lld/%lld", count, expected);
    return 0;
}

/* Add articles to one existing group, each BURST_GAP_USEC after the
   previous one, so that the whole burst lasts much longer than the idle
   limit and no gap reaches it.  After each article, the independent
   snapshot must still show only the rows committed before the burst.
   Timestamps are taken around the requests, so the idle time the server
   saw never exceeds the spans measured here.  Return 1 if no row became
   visible, 0 if rows became visible although the idle limit cannot have
   been reached (or on error), -1 if the host was too slow to tell.  Set
   last_write to when the final article was sent. */
static int
write_burst(struct snapshot *snap, const char *group, long long before,
            int *added, struct timeval *last_write)
{
    char line[512];
    struct timeval started, previous = {0, 0};
    int lo, hi, n, flag, i;
    unsigned long artnum;
    long long count;
    long span;

    *added = 0;
    if (!OVgroupstats((char *) group, &lo, &hi, &n, &flag))
        return 0;
    for (i = 0; i < BURST_ARTICLES; i++) {
        artnum = (unsigned long) hi + 1 + i;
        snprintf(line, sizeof line,
                 "Burst %d\tburst@example.com\tFri, 01 Feb 2002 15:30:16 "
                 "-0800\t<burst%d@example.com>\t\t100\t5\tXref: "
                 "inn.example.com %s:%lu",
                 i, i, group, artnum);
        gettimeofday(&started, NULL);
        if (OVadd(faketoken, line, strlen(line), artnum * 10, 0)
            != OVADDCOMPLETED)
            die("Cannot add burst article %s:%lu", group, artnum);
        (*added)++;
        *last_write = started;
        /* Time since the previous article was sent, at the latest. */
        span = i > 0 ? usec_since(&previous) : 0;
        previous = started;
        usleep(BURST_GAP_USEC);
        count = snapshot_count(snap);
        if (count < 0)
            return 0;
        /* Time since this article was sent, at the latest. */
        if (usec_since(&started) > span)
            span = usec_since(&started);
        if (count != before) {
            diag("burst: %lld committed rows after article %d, expected %lld"
                 " (idle time at most %ld ms)",
                 count, i + 1, before, span / 1000);
            return span < IDLE_SAFE_USEC ? 0 : -1;
        }
    }
    return 1;
}
#endif

/* Parse a line from the overview data file.  Nul-terminates the group name
 * at the beginning of the buffer and returns a pointer to the start of the
 * overview data (the artnum field).  Sets *artnum via the passed pointer.
 * This is the same parsing logic used by overview-t.c. */
static char *
overview_data_parse(char *data, unsigned long *artnum)
{
    char *start;

    if (data[strlen(data) - 1] != '\n')
        die("Line too long in input data");

    start = strchr(data, ':');
    if (start == NULL)
        die("No colon found in input data");
    *start = '\0';
    start++;
    *artnum = strtoul(start, NULL, 10);
    if (*artnum == 0)
        die("Cannot parse article number in input data");
    return start;
}

static void
enter_data_directory(void)
{
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
}

#ifdef HAVE_SQLITE3
/* Burst mode, run by ovsqlite-integ.t on the finished database, which must
   not change any more for the other tests: a burst of writes in the first
   group of the test data is not committed while writes keep coming, and the
   commit is not postponed by read requests once they stop.  With WAL, the
   idle commit or the server shutdown stores the articles even if the writer
   dies. */
static void
run_burst_tests(void)
{
    struct snapshot snap;
    FILE *f;
    char group[256], *colon;
    long long before;
    struct timeval last_write = {0, 0};
    int wal, burst, commit, added = 0;

    enter_data_directory();
    f = fopen("overview/basic", "r");
    if (f == NULL)
        sysdie("Cannot open overview/basic");
    if (fgets(group, sizeof group, f) == NULL
        || (colon = strchr(group, ':')) == NULL)
        die("Cannot read a group name from overview/basic");
    *colon = '\0';
    fclose(f);

    ok(1, OVopen(OV_READ | OV_WRITE));
    wal = snapshot_open(&snap);
    if (wal <= 0) {
        if (wal < 0)
            ok(2, false);
        else
            skip(2, "SQLite did not enable WAL mode");
        skip(3, wal < 0 ? "no independent connection"
                        : "SQLite did not enable WAL mode");
    } else if ((before = snapshot_count(&snap)) < 0) {
        diag("cannot count the committed articles before the burst");
        ok(2, false);
        ok(3, false);
    } else {
        /* Test 2: every gap is shorter than the idle limit, but the burst
           lasts longer than it, so nothing may be committed meanwhile. */
        burst = write_burst(&snap, group, before, &added, &last_write);
        if (burst < 0)
            skip(2, "too slow to tell");
        else
            ok(2, burst == 1);

        /* Test 3: once writes stop, the rows become visible although
           read requests keep arriving through the writer connection.
           A long pause between requests would let even a server that
           wrongly lets reads postpone the commit commit meanwhile, so the
           result then proves nothing. */
        commit = wait_for_commit(&snap, before + added, group, &last_write);
        if (commit < 0)
            skip(3, "too slow to tell");
        else
            ok(3, commit == 1);
    }
    snapshot_close(&snap);
    OVclose();
    ok(4, true);
}
#endif

int
main(int argc, char *argv[])
{
    FILE *f;
    char buffer[4096];
    char *start;
    unsigned long artnum;
    char flag[] = NF_FLAG_OK_STRING;
    /* Track which groups we've already added. */
    char *seen_groups[64];
    int ngroups = 0;
    int narticles = 0;
    int i;
    bool seen;

    if (argc > 1 && strcmp(argv[1], "burst") == 0) {
#ifdef HAVE_SQLITE3
        test_init(4);
        alarm(30);
        run_burst_tests();
        return 0;
#else
        skip_all("SQLite support not compiled");
#endif
    }
    test_init(4);
    /* The fresh server's configured 30-second transaction limit cannot
       produce a false pass before this whole-writer deadline. */
    alarm(20);

    enter_data_directory();

    /* Test 1: open overview in read/write mode (connects to server). */
    ok(1, OVopen(OV_READ | OV_WRITE));

    /* Load test data from the same file used by overview-t.c. */
    f = fopen("overview/basic", "r");
    if (f == NULL)
        sysdie("Cannot open overview/basic");

    while (fgets(buffer, sizeof(buffer), f) != NULL) {
        start = overview_data_parse(buffer, &artnum);

        /* Add group if we haven't seen it yet. */
        seen = false;
        for (i = 0; i < ngroups; i++) {
            if (strcmp(seen_groups[i], buffer) == 0) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            if (ngroups >= 64)
                die("Too many groups in test data");
            seen_groups[ngroups] = xstrdup(buffer);
            ngroups++;
            if (!OVgroupadd(buffer, 0, 0, flag))
                die("Cannot add group %s", buffer);
        }

        /* OVadd expects the overview data *without* the "artnum\t" prefix
         * and without trailing \r\n -- it adds those itself.  But it does
         * need the Xref field present so it can determine group:artnum.
         *
         * In the data file, the format after overview_data_parse is:
         *   "artnum\tSubject...\tXref: host group:artnum\n"
         *
         * We need to pass the data starting from the first tab character
         * (skipping the artnum), without the trailing \n.  OVadd will
         * prepend "artnum\t" and append "\r\n" before storing. */
        {
            char *tab = strchr(start, '\t');
            int len;

            if (tab == NULL)
                die("No tab after artnum in data for %s:%lu", buffer, artnum);
            tab++; /* point past the tab to the actual overview fields */
            len = strlen(tab);
            /* Strip trailing \n. */
            if (len > 0 && tab[len - 1] == '\n')
                len--;

            if (OVadd(faketoken, tab, len, artnum * 10,
                      (artnum % 5 == 0) ? artnum * 100 : artnum)
                != OVADDCOMPLETED)
                die("Cannot add %s:%lu", buffer, artnum);
        }
        narticles++;
    }
    fclose(f);

    /* Test 2: all articles were loaded. */
    ok(2, narticles > 0);

    /* Test 3: idle commit makes rows visible before the writer disconnects.
       Direct readers see only committed rows, so use an independent
       connection while the writer stays open.  This needs WAL mode. */
#ifdef HAVE_SQLITE3
    {
        struct snapshot snap;
        int wal;

        wal = snapshot_open(&snap);
        if (wal == 0)
            skip(3, "SQLite did not enable WAL mode");
        else
            ok(3, wal > 0 && wait_for_commit(&snap, narticles, NULL, NULL));
        snapshot_close(&snap);
    }
#else
    skip(3, "SQLite support not compiled");
#endif

    /* Test 4: close succeeds. */
    OVclose();
    ok(4, true);

    for (i = 0; i < ngroups; i++)
        free(seen_groups[i]);

    alarm(0);
    return 0;
}
