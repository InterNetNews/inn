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
/* Use an independent connection while the writer stays open.  Resetting the
   count statement ends each read snapshot, so polling cannot pin old data. */
static int
wait_for_commit(int expected)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    char *path;
    struct timeval start, now;
    sqlite3_int64 count = -1;
    int status, result = 0;

    path = concatpath(innconf->pathoverview, "ovsqlite.db");
    status = sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL);
    free(path);
    if (status != SQLITE_OK)
        goto error;
    status = sqlite3_prepare_v2(db, "pragma journal_mode", -1, &stmt, NULL);
    if (status != SQLITE_OK || sqlite3_step(stmt) != SQLITE_ROW
        || sqlite3_column_text(stmt, 0) == NULL)
        goto error;
    if (strcmp((const char *) sqlite3_column_text(stmt, 0), "wal") != 0) {
        result = -1;
        goto done;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;
    status = sqlite3_prepare_v2(db, "select count(*) from artinfo", -1, &stmt,
                                NULL);
    if (status != SQLITE_OK)
        goto error;
    gettimeofday(&start, NULL);
    for (;;) {
        if (sqlite3_step(stmt) != SQLITE_ROW)
            goto error;
        count = sqlite3_column_int64(stmt, 0);
        sqlite3_reset(stmt);
        if (count == expected) {
            result = 1;
            break;
        }
        gettimeofday(&now, NULL);
        if (now.tv_sec - start.tv_sec > 2
            || (now.tv_sec - start.tv_sec == 2
                && now.tv_usec >= start.tv_usec))
            break;
        usleep(10000);
    }
    diag("independent committed rows: %lld/%d", (long long) count, expected);
    goto done;

error:
    diag("independent SQLite read failed: %s",
         db == NULL ? "no connection" : sqlite3_errmsg(db));
done:
    sqlite3_finalize(stmt);
    if (db != NULL)
        sqlite3_close(db);
    return result;
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

int
main(void)
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

    test_init(4);
    /* The fresh server's configured 30-second transaction limit cannot
       produce a false pass before this whole-writer deadline. */
    alarm(20);

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

    /* Test 3: idle commit makes rows visible before the writer disconnects. */
#ifdef HAVE_SQLITE3
    {
        int result = wait_for_commit(narticles);

        if (result < 0)
            skip(3, "SQLite did not enable WAL mode");
        else
            ok(3, result);
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
