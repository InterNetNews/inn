/*
**  Convert an existing history database to the hissqlite (SQLite) backend.
**
**  Usage: hissqlite-convert [-m srcmethod] <srcpath> <dstpath>
**
**  Reads the source history (default method "hisv6") via HISwalk and
**  bulk-loads every entry into a freshly created hissqlite database keyed by
**  hash.  This is a faithful migration (the makedbz analog): it preserves
**  exact timestamps AND remembered (token-less) entries, which a from-spool
**  rebuild (makehistory) cannot reconstruct.
**
**  Insertion is by HASH, not Message-ID: the history text stores only the MD5,
**  and the original Message-ID is unrecoverable, so HISwrite (which re-hashes
**  a Message-ID) cannot be used.  We therefore create the canonical schema via
**  the hissqlite backend, then bulk-insert by hash through a raw SQLite
**  connection.
**
**  Written by Kevin Bowling in 2026.
*/

#include "portable/system.h"

#include "inn/history.h"
#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/storage.h"

#ifndef HAVE_SQLITE3

int
main(void)
{
    die("INN was built without SQLite support");
}

#else

#    include <sqlite3.h>
#    include <sys/stat.h>

/* Commit the load transaction every this many rows. */
#    define COMMIT_EVERY 50000

struct convstate {
    sqlite3 *db;
    sqlite3_stmt *insert;
    unsigned long count; /* rows successfully inserted */
    unsigned long dups;  /* duplicate-hash rows skipped (anomalous) */
};

static void
checked(sqlite3 *db, int status, const char *what)
{
    if (status != SQLITE_OK)
        die("%s: %s", what, sqlite3_errmsg(db));
}

static void
exec(sqlite3 *db, const char *sql)
{
    char *errmsg = NULL;

    if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
        die("%s: %s", sql, errmsg ? errmsg : "(unknown)");
}

/*
**  HISwalk callback: insert one source entry into the hissqlite DB by hash.
**  token == NULL marks a remembered entry (stored with a NULL token column).
*/
static bool
convert_cb(void *cookie, const HASH *hash, time_t arrived, time_t posted,
           time_t expires, const TOKEN *token)
{
    struct convstate *st = cookie;
    sqlite3_stmt *ins = st->insert;
    int rc;

    sqlite3_bind_blob(ins, 1, hash, sizeof(HASH), SQLITE_STATIC);
    sqlite3_bind_int64(ins, 2, (sqlite3_int64) arrived);
    sqlite3_bind_int64(ins, 3, (sqlite3_int64) posted);
    sqlite3_bind_int64(ins, 4, (sqlite3_int64) expires);
    if (token == NULL)
        sqlite3_bind_null(ins, 5);
    else
        sqlite3_bind_blob(ins, 5, token, sizeof(TOKEN), SQLITE_STATIC);

    rc = sqlite3_step(ins);
    sqlite3_reset(ins);
    if (rc == SQLITE_DONE) {
        st->count++;
    } else if ((rc & 0xFF) == SQLITE_CONSTRAINT) {
        /* Duplicate hash in the source: anomalous for a valid history, but not
           fatal -- count it rather than silently dropping it. */
        st->dups++;
        return true;
    } else {
        /* A real error (I/O, disk full, corruption): abort.  The half-built
           database is a temp file that is never renamed into place. */
        die("insert failed after %lu entries: %s", st->count,
            sqlite3_errmsg(st->db));
    }

    if (st->count % COMMIT_EVERY == 0) {
        exec(st->db, "commit");
        exec(st->db, "begin");
        if (st->count % (COMMIT_EVERY * 20) == 0)
            notice("%lu entries converted...", st->count);
    }
    return true;
}

int
main(int argc, char *argv[])
{
    const char *srcmethod = "hisv6";
    const char *srcpath, *dstpath;
    char *finaldb, *tempbase, *tempdb;
    struct history *src, *tmp;
    struct convstate st;
    struct stat sb;
    int opt;

    message_program_name = "hissqlite-convert";

    while ((opt = getopt(argc, argv, "m:")) != -1) {
        switch (opt) {
        case 'm':
            srcmethod = optarg;
            break;
        default:
            die("usage: hissqlite-convert [-m srcmethod] <srcpath> <dstpath>");
        }
    }
    if (argc - optind != 2)
        die("usage: hissqlite-convert [-m srcmethod] <srcpath> <dstpath>");
    srcpath = argv[optind];
    dstpath = argv[optind + 1];

    /* The created database honors the hissqlite* tunables (page size in
       particular) from inn.conf, matching what innd will later use. */
    if (!innconf_read(NULL))
        exit(1);

    /* Refuse to overwrite an existing destination: a re-run or a typo must not
       clobber or merge into a live history. */
    finaldb = concat(dstpath, ".sqlite", (char *) NULL);
    if (stat(finaldb, &sb) == 0)
        die("destination %s already exists; refusing to overwrite", finaldb);

    /* Build into a temp file and rename it into place only on full success, so
       a crash or error never leaves a usable-but-incomplete destination (this
       is also what makes journal_mode=off below safe).  Clear any stale temp
       from a previous failed run. */
    tempbase = concat(dstpath, ".new", (char *) NULL);
    tempdb = concat(tempbase, ".sqlite", (char *) NULL);
    unlink(tempdb);

    /* Create the temp DB with the canonical hissqlite schema, then reopen the
       raw SQLite file for a fast by-hash bulk load. */
    tmp = HISopen(tempbase, "hissqlite", HIS_CREAT | HIS_RDWR);
    if (tmp == NULL)
        die("cannot create destination %s", tempdb);
    HISclose(tmp);

    memset(&st, 0, sizeof(st));
    if (sqlite3_open(tempdb, &st.db) != SQLITE_OK)
        die("cannot open %s: %s", tempdb, sqlite3_errmsg(st.db));

    /* Bulk-load tuning: no per-row durability (safe -- a failure leaves the
       temp file, never the final), and drop the secondary index so it is
       built once at the end.  (The CREATE INDEX statement mirrors
       hissqlite-init.sql.) */
    exec(st.db, "pragma synchronous = off");
    exec(st.db, "pragma journal_mode = off");
    /* A large page cache keeps the random-MD5 B-tree hot during the load, so
       inserts don't constantly evict and re-fetch leaf pages.  (The first innd
       open flips the finished DB to WAL.) */
    exec(st.db, "pragma cache_size = -262144"); /* 256 MiB (negative = KiB) */
    exec(st.db, "drop index if exists hist_remember");

    /* Plain INSERT (not OR IGNORE): a duplicate hash returns SQLITE_CONSTRAINT
       so convert_cb can count it instead of silently dropping the row. */
    checked(st.db,
            sqlite3_prepare_v3(st.db,
                               "insert into hist"
                               "(hash, arrived, posted, expires, token)"
                               " values(?1, ?2, ?3, ?4, ?5)",
                               -1, SQLITE_PREPARE_PERSISTENT, &st.insert,
                               NULL),
            "prepare insert");

    src = HISopen(srcpath, srcmethod, HIS_RDONLY);
    if (src == NULL)
        die("cannot open source %s (method %s)", srcpath, srcmethod);

    exec(st.db, "begin");
    if (!HISwalk(src, NULL, &st, convert_cb))
        die("HISwalk failed: %s", HISerror(src));
    exec(st.db, "commit");
    HISclose(src);

    sqlite3_finalize(st.insert);
    notice("rebuilding index...");
    exec(st.db, "create index hist_remember on hist(posted, arrived)"
                " where token is null");
    if (sqlite3_close(st.db) != SQLITE_OK)
        die("cannot close %s", tempdb);

    /* Atomically publish the completed database. */
    if (rename(tempdb, finaldb) != 0)
        sysdie("cannot rename %s to %s", tempdb, finaldb);

    if (st.dups > 0)
        warn("%lu duplicate hashes in the source were skipped", st.dups);
    notice("converted %lu entries into %s", st.count, finaldb);
    free(finaldb);
    free(tempbase);
    free(tempdb);
    return 0;
}

#endif /* HAVE_SQLITE3 */
