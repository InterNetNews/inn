/* Unit tests for ovsqlite direct reader edge cases.
 *
 * Tests behavior that can't be exercised by the integration test
 * (ovsqlite-integ.t) which requires a running ovsqlite-server.
 * These tests create a database directly via the sqlite3 API.
 */

#include "portable/system.h"

#include <sys/stat.h>

#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/ov.h"
#include "inn/storage.h"
#include "tap/basic.h"

#ifdef HAVE_SQLITE3

#    include <sqlite3.h>

#    define OVSQLITE_DB_FILE "ovsqlite.db"
#    define TEST_DIR         "ov-tmp"

static const char *schema =
    "create table misc (key text primary key, value not null) without rowid;"
    "create table groupinfo (groupid integer primary key,"
    "    low integer not null default 1, high integer not null default 0,"
    "    \"count\" integer not null default 0,"
    "    expired integer not null default 0,"
    "    deleted integer not null default 0,"
    "    groupname blob not null, flag_alias blob not null,"
    "    unique (deleted, groupname));"
    "create table artinfo (groupid integer references groupinfo (groupid)"
    "    on update cascade on delete restrict,"
    "    artnum integer, arrived integer not null, expires integer not null,"
    "    token blob not null, overview blob not null,"
    "    primary key (groupid, artnum)) without rowid;";


/* Create a minimal database with the given journal mode. */
static bool
create_db(const char *dbpath, bool enable_wal)
{
    sqlite3 *db;
    int status;
    char *errmsg;

    status = sqlite3_open_v2(dbpath, &db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                             NULL);
    if (status != SQLITE_OK)
        return false;

    status = sqlite3_exec(db, schema, 0, NULL, &errmsg);
    if (status != SQLITE_OK) {
        sqlite3_free(errmsg);
        sqlite3_close(db);
        return false;
    }

    sqlite3_exec(db,
                 "insert into misc (key, value) values ('version', 1);"
                 "insert into misc (key, value) values ('compress', 0);",
                 0, NULL, NULL);

    if (enable_wal)
        sqlite3_exec(db, "pragma journal_mode = 'WAL';", 0, NULL, NULL);

    sqlite3_close(db);
    return true;
}

static void
write_config(const char *dir, bool walmode)
{
    char *path;
    FILE *f;

    path = concatpath(dir, "ovsqlite.conf");
    f = fopen(path, "w");
    free(path);
    fprintf(f, "walmode: %s\n", walmode ? "true" : "false");
    fprintf(f, "readercachesize: 1000\n");
    fclose(f);
}

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
    memset(innconf, 0, sizeof(*innconf));
    innconf->enableoverview = true;
    innconf->ovmethod = xstrdup("ovsqlite");
    innconf->pathdb = xstrdup(TEST_DIR);
    innconf->pathetc = xstrdup(TEST_DIR);
    innconf->pathoverview = xstrdup(TEST_DIR);
    innconf->pathrun = xstrdup(TEST_DIR);
}

static void
setup(void)
{
    if (system("/bin/rm -rf " TEST_DIR) < 0)
        sysdie("Cannot rm " TEST_DIR);
    if (mkdir(TEST_DIR, 0755))
        sysdie("Cannot mkdir " TEST_DIR);
}

int
main(void)
{
    char *dbpath;

    test_init(4);

    /*
     * Test 1-2: WAL mode gating.
     * direct_open() checks pragma journal_mode and only activates direct
     * reader if the database is actually in WAL mode.  With a non-WAL
     * database, it falls back to the server path (which fails here since
     * no server is running).
     */
    setup();
    dbpath = concatpath(TEST_DIR, OVSQLITE_DB_FILE);
    create_db(dbpath, false);
    free(dbpath);
    write_config(TEST_DIR, true); /* config says WAL, but DB isn't */
    fake_innconf();
    ok(1, !OVopen(OV_READ)); /* should fail: not WAL, no server */

    /* With WAL enabled on the database, direct_open succeeds. */
    setup();
    dbpath = concatpath(TEST_DIR, OVSQLITE_DB_FILE);
    create_db(dbpath, true);
    free(dbpath);
    write_config(TEST_DIR, true);
    fake_innconf();
    ok(2, OVopen(OV_READ));
    OVclose();

    /*
     * Test 3-4: Write operations rejected in direct reader mode.
     * The direct reader opens SQLITE_OPEN_READONLY and sets query_only=1.
     * Write API calls should return false.
     */
    fake_innconf();
    OVopen(OV_READ);
    {
        char group[] = "new.group";
        char flag[] = "y";

        ok(3, !OVgroupadd(group, 0, 0, flag));
    }
    {
        TOKEN token = {0, 0, ""};
        char data[] = "test";

        ok(4, OVadd(token, data, 4, 0, 0) != OVADDCOMPLETED);
    }
    OVclose();

    if (system("/bin/rm -rf " TEST_DIR) < 0)
        sysdie("Cannot rm " TEST_DIR);
    return 0;
}

#else /* ! HAVE_SQLITE3 */

int
main(void)
{
    skip_all("SQLite support not compiled");
    return 0;
}

#endif
