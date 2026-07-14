/*
**  The SQLite history method (hissqlite).
**
**  A transactional replacement for hisv6/dbz.  Schema 1: a single WITHOUT
**  ROWID table clustered on the 16-byte MD5 of the Message-ID, so a
**  Message-ID -> token lookup is one clustered-leaf access.  WAL +
**  synchronous=NORMAL, autocommit writes; readers use the WAL direct path.
**
**  innd owns its write connection directly and is the steady-state writer; the
**  only other writer is expire, which runs in place (it reopens the live DB
**  read/write instead of rebuilding and swapping a file, so innd is not
**  paused).  SQLite's WAL write lock and busy_timeout serialize the two
**  without a broker server process (unlike ovsqlite): this works because
**  writes autocommit, so the lock is held only per statement and the periodic,
**  light expire does not contend with innd for long.  See hissqlite(5).
**
**  Written by Kevin Bowling in 2026.
*/

#include "portable/system.h"

#include "../hisinterface.h"
#include "hissqlite.h"
#include "inn/messages.h"

#ifdef HAVE_SQLITE3

#    include "hissqlite-private.h"
#    include "inn/innconf.h"

/*
**  Record a SQLite error against the history handle.
*/
static void
hissqlite_seterror(struct hissqlite *h, const char *context)
{
    if (h->history != NULL)
        his_seterror(h->history,
                     concat("hissqlite: ", context, ": ",
                            h->db ? sqlite3_errmsg(h->db) : "(no db)", NULL));
}

/*
**  Bind the MD5 of a Message-ID as the hash key (parameter 1).
*/
static void
bind_key(sqlite3_stmt *stmt, const char *key)
{
    HASH hash = HashMessageID(key);

    sqlite3_bind_blob(stmt, 1, &hash, sizeof(HASH), SQLITE_TRANSIENT);
}

/*
**  Copy a column blob into a fixed-size destination, verifying its length
**  first.  A wrong-length blob means a corrupt row; report it (so corruption
**  is visible) and return false rather than over-reading or silently producing
**  a bogus value.
*/
static bool
copy_blob(struct hissqlite *h, sqlite3_stmt *stmt, int col, void *dst,
          size_t size, const char *what)
{
    if (sqlite3_column_bytes(stmt, col) != (int) size) {
        /* A length mismatch is the error itself; the generic seterror would
           append an unrelated sqlite3_errmsg, so report "what" on its own. */
        if (h->history != NULL)
            his_seterror(h->history,
                         concat("hissqlite: ", what, (char *) NULL));
        return false;
    }
    memcpy(dst, sqlite3_column_blob(stmt, col), size);
    return true;
}

/*
**  Steady-state writes are NOT batched: each HISwrite/HISremember/HISreplace
**  autocommits (one implicit transaction per statement).  With
**  synchronous=NORMAL a commit is a buffered WAL append, not an fsync, so
**  there is no fsync cost to amortise; batching would hold the write lock
**  across the batch, which is exactly wrong for the two-writer model (innd +
**  a separate expire process).  The lock hold of an open batch is not the
**  time to write N rows but the wall-clock time until N articles arrive, so
**  at low traffic even a small batch could starve expire past its
**  busy_timeout.  Autocommit holds the lock for a single statement, so innd
**  and expire interleave cleanly, and there is no open transaction to leave
**  dangling on error.
**
**  Bulk loading is the exception: committing per row costs throughput
**  (the per-transaction WAL append + wal-index update dominates a random-key
**  load).  The history API already has a bulk-rebuild hint for exactly this:
**  HIS_INCORE, which makehistory passes and which hisv6 honors by building
**  the whole dbz index in core and flushing at close,  sole writer assumed,
**  durability deferred, a crash means rerunning the rebuild.  hissqlite
**  translates the same hint into its own terms: HISwrite runs in explicit
**  transactions of HISSQLITE_BULK_BATCH rows.  The batch commits when full,
**  on HISsync and on HISclose, so every write that returned true is
**  committed by close.
*/
static bool
batch_commit(struct hissqlite *h)
{
    if (!h->batch_open)
        return true;
    h->batch_open = false;
    h->batch_pending = 0;
    if (sqlite3_exec(h->db, "commit", NULL, NULL, NULL) != SQLITE_OK) {
        /* The batch's writes are lost; make that loud, then roll back so the
           connection is not left inside a wedged transaction. */
        hissqlite_seterror(h, "batch commit");
        sqlite3_exec(h->db, "rollback", NULL, NULL, NULL);
        return false;
    }
    return true;
}

static void
batch_begin(struct hissqlite *h)
{
    if (h->batch_size == 0 || h->batch_open)
        return;
    /* On failure no transaction is open, so the write below simply
       autocommits: safe degradation, and the write path reports any real
       database error itself. */
    if (sqlite3_exec(h->db, "begin", NULL, NULL, NULL) == SQLITE_OK) {
        h->batch_open = true;
        h->batch_pending = 0;
    }
}

static bool
batch_advance(struct hissqlite *h)
{
    if (!h->batch_open)
        return true;
    if (++h->batch_pending < h->batch_size)
        return true;
    return batch_commit(h);
}

/*
**  Track the WAL frame count so HISsync can decide when to checkpoint.
*/
static int
hissqlite_wal_hook(void *arg, sqlite3 *db UNUSED, const char *name UNUSED,
                   int pages)
{
    ((struct hissqlite *) arg)->wal_pages = pages;
    return SQLITE_OK;
}

/*
**  A PASSIVE checkpoint never blocks: it writes back as many WAL frames as the
**  oldest reader allows and lets the WAL be reused (not grown) on the next
**  write.  Safe to call from innd's HISsync timer without stalling accepts.
**  Disk reclaim (TRUNCATE) is left to close, where readers are expected gone.
*/
static void
hissqlite_wal_passive(struct hissqlite *h)
{
    int log = 0, ckpt = 0, rc;

    rc = sqlite3_wal_checkpoint_v2(h->db, NULL, SQLITE_CHECKPOINT_PASSIVE,
                                   &log, &ckpt);
    if (rc != SQLITE_OK) {
        /* A real failure (e.g. I/O error): surface it so a WAL that stops
           shrinking -- and could fill the disk -- is visible, not silent. */
        warn("hissqlite: WAL checkpoint failed: %s", sqlite3_errmsg(h->db));
        return;
    }
    if (log == ckpt) {
        /* Fully flushed; the WAL resets on the next write. */
        h->wal_pages = 0;
    }
    /* else a reader still pins older frames; wal_pages stays high and we retry
       on the next HISsync (not logged per-call -- that is normal, transient).
     */
}

/*
**  Has the schema already been created?  HIS_CREAT must initialise a brand-new
**  database but must NOT re-run the DDL (or re-stamp the version) on an
**  existing one -- otherwise opening an existing DB with HIS_CREAT fails on
**  "table hist already exists" and would mask a version mismatch.
*/
static bool
hissqlite_initialized(struct hissqlite *h)
{
    sqlite3_stmt *stmt;
    bool exists = false;

    if (sqlite3_prepare_v2(h->db,
                           "select 1 from sqlite_master where type = 'table' "
                           "and name = 'hist'",
                           -1, &stmt, NULL)
        == SQLITE_OK) {
        exists = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
    }
    return exists;
}

/*
**  Refuse to open a database whose schema version we do not understand, so a
**  mismatch fails cleanly rather than as obscure later query errors.
*/
static bool
hissqlite_check_version(struct hissqlite *h)
{
    sqlite3_stmt *stmt;
    long version = -1;
    char buf[160];

    if (sqlite3_prepare_v2(h->db,
                           "select value from misc where key = 'version'", -1,
                           &stmt, NULL)
        != SQLITE_OK) {
        hissqlite_seterror(h, "prepare version check");
        return false;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW)
        version = (long) sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    if (version == HISSQLITE_SCHEMA_VERSION)
        return true;
    snprintf(buf, sizeof(buf),
             "hissqlite: %s has schema version %ld, expected %d", h->path,
             version, HISSQLITE_SCHEMA_VERSION);
    his_seterror(h->history, xstrdup(buf));
    return false;
}

/*
**  Is the database in WAL journal mode?  A direct reader requires it; a
**  non-WAL reader would take SHARED locks that conflict with the writer's
**  commits.
*/
static bool
hissqlite_is_wal(struct hissqlite *h)
{
    sqlite3_stmt *stmt;
    bool wal = false;

    if (sqlite3_prepare_v2(h->db, "pragma journal_mode", -1, &stmt, NULL)
        == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *mode = (const char *) sqlite3_column_text(stmt, 0);
            wal = (mode != NULL && strcmp(mode, "wal") == 0);
        }
        sqlite3_finalize(stmt);
    }
    return wal;
}

/*
**  Open (or create) the database named by h->path and prepare the statement
**  sets, using h->flags.  Factored out of hissqlite_open so the deferred-path
**  pattern -- HISopen(NULL, ...) followed by HISCTLS_PATH, as makehistory uses
**  -- can drive the real open once the path is known.  On failure it reports
**  the error, tears down any half-open database, and returns false; the handle
**  itself is left for the caller to free or (for HISCTLS_PATH) retry.
*/
static bool
hissqlite_doopen(struct hissqlite *h)
{
    char *errmsg = NULL;
    char pragma[64];
    unsigned long pagesize = HISSQLITE_DEF_PAGE_SIZE;
    unsigned long cachesize = HISSQLITE_DEF_CACHE_SIZE;
    unsigned long mmapsize = HISSQLITE_DEF_MMAP_SIZE;
    unsigned long rcachesize = HISSQLITE_DEF_RCACHE_SIZE;
    int oflags;

    /* Performance tunables come from inn.conf when it has been read (innd,
       expire, makehistory, the converter); offline callers and unit tests that
       open history without innconf_read() fall back to the built-in defaults.
     */
    if (innconf != NULL) {
        pagesize = innconf->hissqlitepagesize;
        cachesize = innconf->hissqlitecachesize;
        mmapsize = innconf->hissqlitemmapsize;
        rcachesize = innconf->hissqlitereadercachesize;
    }

    if (!(h->flags & HIS_RDWR)) {
        /* Read-only open (HIS_RDONLY is 0, but callers may OR in hint flags
           such as HIS_ONDISK, so test for the absence of HIS_RDWR rather than
           exact equality).  WAL direct reader: open read-only, verify WAL,
           prepare read stmts. */
        if (sqlite3_open_v2(h->path, &h->db, SQLITE_OPEN_READONLY, NULL)
            != SQLITE_OK) {
            hissqlite_seterror(h, "open read-only");
            goto fail;
        }
        if (!hissqlite_is_wal(h)) {
            hissqlite_seterror(h, "history is not in WAL mode; direct reader"
                                  " unavailable (NFS history needs a server)");
            goto fail;
        }
        if (sqlite_helper_init(&hissqlite_read_helper,
                               (sqlite3_stmt **) &h->read, h->db,
                               SQLITE_PREPARE_PERSISTENT, &errmsg)
            != SQLITE_OK) {
            hissqlite_seterror(h, "prepare read statements");
            goto fail;
        }
        if (!hissqlite_check_version(h)) {
            sqlite_helper_term(&hissqlite_read_helper,
                               (sqlite3_stmt **) &h->read);
            goto fail;
        }
        /* Reader cache + mmap from inn.conf.  Each nnrpd has its own cache, so
           it is kept small by default (history is not nnrpd's hot path --
           overview is); readers lean on the shared OS page cache instead. */
        snprintf(pragma, sizeof(pragma), "pragma cache_size = -%lu;",
                 rcachesize);
        sqlite3_exec(h->db, pragma, NULL, NULL, NULL);
        snprintf(pragma, sizeof(pragma), "pragma mmap_size = %lu;", mmapsize);
        sqlite3_exec(h->db, pragma, NULL, NULL, NULL);
        h->direct_reader = true;
        return true;
    }

    oflags = SQLITE_OPEN_READWRITE;
    if (h->flags & HIS_CREAT)
        oflags |= SQLITE_OPEN_CREATE;
    if (sqlite3_open_v2(h->path, &h->db, oflags, NULL) != SQLITE_OK) {
        hissqlite_seterror(h, "open read/write");
        goto fail;
    }
    sqlite3_extended_result_codes(h->db, 1);

    if ((h->flags & HIS_CREAT) && !hissqlite_initialized(h)) {
        /* Brand-new database: run the DDL (unnamed init section of
           hissqlite-init.sql) and stamp the schema version.  Skipped for an
           existing DB so HIS_CREAT is idempotent.  Mirrors ovsqlite-server.c.
         */
        hissqlite_init_t init;

        /* page_size only takes effect before the first table is written, so
           apply it here, ahead of the DDL.  (innconf validates it.) */
        snprintf(pragma, sizeof(pragma), "pragma page_size = %lu;", pagesize);
        sqlite3_exec(h->db, pragma, NULL, NULL, NULL);

        if (sqlite_helper_init(&hissqlite_init_helper, (sqlite3_stmt **) &init,
                               h->db, 0, &errmsg)
            != SQLITE_OK) {
            hissqlite_seterror(h, "create schema");
            goto fail;
        }
        sqlite3_bind_int64(init.set_version, 1, HISSQLITE_SCHEMA_VERSION);
        if (sqlite3_step(init.set_version) != SQLITE_DONE)
            hissqlite_seterror(h, "set version");
        sqlite3_reset(init.set_version);
        sqlite_helper_term(&hissqlite_init_helper, (sqlite3_stmt **) &init);
    }

    /* Prepare writer statements; the unnamed init section applies the WAL /
       synchronous / busy_timeout pragmas.  PERSISTENT: these are long-lived.
     */
    if (sqlite_helper_init(&hissqlite_main_helper, (sqlite3_stmt **) &h->main,
                           h->db, SQLITE_PREPARE_PERSISTENT, &errmsg)
        != SQLITE_OK) {
        hissqlite_seterror(h, "prepare main statements");
        goto fail;
    }
    if (!hissqlite_check_version(h)) {
        sqlite_helper_term(&hissqlite_main_helper, (sqlite3_stmt **) &h->main);
        goto fail;
    }
    /* Track WAL growth so HISsync can keep it bounded (the .sql init section
       has already put us in WAL mode). */
    sqlite3_wal_hook(h->db, hissqlite_wal_hook, h);

    /* Writer cache + mmap from inn.conf, applied after the .sql WAL pragmas.
     */
    snprintf(pragma, sizeof(pragma), "pragma cache_size = -%lu;", cachesize);
    sqlite3_exec(h->db, pragma, NULL, NULL, NULL);
    snprintf(pragma, sizeof(pragma), "pragma mmap_size = %lu;", mmapsize);
    sqlite3_exec(h->db, pragma, NULL, NULL, NULL);

    /* Bulk rebuild (makehistory): batch the writes; see the batch_* helpers
       above. */
    if (h->flags & HIS_INCORE)
        h->batch_size = HISSQLITE_BULK_BATCH;
    return true;

fail:
    if (errmsg != NULL)
        sqlite3_free(errmsg);
    if (h->db != NULL) {
        sqlite3_close_v2(h->db); /* _v2 zombie-closes even if a stmt lingers */
        h->db = NULL;
    }
    return false;
}

void *
hissqlite_open(const char *path, int flags, struct history *history)
{
    struct hissqlite *h;

    h = xcalloc(1, sizeof(*h));
    h->history = history;
    h->flags = flags;

    /* makehistory opens with a NULL path and supplies the real one afterwards
       via HISCTLS_PATH; defer the database open until then.  Until the path is
       set the handle is unusable, exactly as with hisv6. */
    if (path == NULL)
        return h;

    h->path = concat(path, ".sqlite", (char *) NULL);
    if (!hissqlite_doopen(h)) {
        free(h->path);
        free(h);
        return NULL;
    }
    return h;
}

bool
hissqlite_sync(void *history)
{
    struct hissqlite *h = history;
    bool ok = true;

    /* HISsync is deliberately NOT a durability barrier, matching dbz:
       hisv6_sync does fflush(3) + msync(MS_ASYNC), neither of which waits for
       the disk, so dbz history is only kernel-buffered after a sync (survives
       an innd crash, lost on power loss).  Under synchronous=NORMAL each
       autocommit already write(2)s the WAL into the kernel page cache, so
       hissqlite sits continuously in that same state: there is nothing
       application-buffered for HISsync to flush.  (Per-commit durability would
       be synchronous=FULL, a possible future knob, not an fsync here.)  innd
       calls HISsync on a timer, so it is simply the convenient place to keep
       the WAL bounded: once it has grown past the threshold, do a non-blocking
       PASSIVE checkpoint.

       Under HISCTLS_WRITEBATCH there IS something buffered: commit the open
       batch so a bulk loader that calls HISsync gets its writes flushed. */
    if (!h->direct_reader) {
        ok = batch_commit(h);
        if (h->wal_pages >= HISSQLITE_WAL_CKPT_PAGES)
            hissqlite_wal_passive(h);
    }
    return ok;
}

bool
hissqlite_close(void *history)
{
    struct hissqlite *h = history;
    bool ok = true;

    /* A deferred handle (HISopen(NULL) whose HISCTLS_PATH was never set or
       failed) never opened a database, so there is nothing to checkpoint or
       close; just free it.  Skipping this would call
       sqlite3_wal_checkpoint_v2() on a NULL connection, which is not a no-op
       like sqlite3_close_v2(NULL). */
    if (h->db == NULL) {
        free(h->path);
        free(h);
        return true;
    }

    /* Flush and reclaim the WAL on a clean shutdown: PASSIVE first (never
       blocks, even if direct-reader processes still hold the DB open), then
       TRUNCATE only if every frame was checkpointed (no readers pinning it) so
       we don't hang waiting for readers to disconnect.  Mirrors ovsqlite. */
    if (!h->direct_reader) {
        int log = 0, ckpt = 0;

        /* Commit any open bulk-ingest batch so every HISwrite that returned
           true is in the database before the final checkpoint. */
        if (!batch_commit(h))
            ok = false;
        if (sqlite3_wal_checkpoint_v2(h->db, NULL, SQLITE_CHECKPOINT_PASSIVE,
                                      &log, &ckpt)
                == SQLITE_OK
            && log == ckpt)
            sqlite3_wal_checkpoint_v2(h->db, NULL, SQLITE_CHECKPOINT_TRUNCATE,
                                      NULL, NULL);
        sqlite_helper_term(&hissqlite_main_helper, (sqlite3_stmt **) &h->main);
    } else {
        sqlite_helper_term(&hissqlite_read_helper, (sqlite3_stmt **) &h->read);
    }

    if (sqlite3_close_v2(h->db) != SQLITE_OK)
        ok = false;
    free(h->path);
    free(h);
    return ok;
}

bool
hissqlite_lookup(void *history, const char *key, time_t *arrived,
                 time_t *posted, time_t *expires, struct token *token)
{
    struct hissqlite *h = history;
    sqlite3_stmt *stmt = h->direct_reader ? h->read.lookup : h->main.lookup;
    bool found = false;
    int status;

    bind_key(stmt, key);
    status = sqlite3_step(stmt);
    if (status == SQLITE_ROW) {
        found = true;
        if (arrived != NULL)
            *arrived = (time_t) sqlite3_column_int64(stmt, 0);
        if (posted != NULL)
            *posted = (time_t) sqlite3_column_int64(stmt, 1);
        if (expires != NULL)
            *expires = (time_t) sqlite3_column_int64(stmt, 2);
        if (token != NULL) {
            if (sqlite3_column_type(stmt, 3) == SQLITE_NULL) {
                /* Remembered entry: found, but no article. */
                memset(token, 0, sizeof(TOKEN));
                token->type = TOKEN_EMPTY;
            } else if (!copy_blob(h, stmt, 3, token, sizeof(TOKEN),
                                  "corrupt token blob in lookup")) {
                /* Wrong-length token = corruption.  Treat as a lookup failure
                   (reported above) rather than masking it as a no-article. */
                found = false;
            }
        }
    } else if (status != SQLITE_DONE) {
        hissqlite_seterror(h, "lookup");
    }
    sqlite3_reset(stmt);
    return found;
}

bool
hissqlite_check(void *history, const char *key)
{
    struct hissqlite *h = history;
    sqlite3_stmt *stmt = h->direct_reader ? h->read.check : h->main.check;
    bool found;
    int status;

    /* Existence only: real OR remembered both count (refuse re-offers).
       his.c keeps an in-memory existence cache in front of this. */
    bind_key(stmt, key);
    status = sqlite3_step(stmt);
    found = (status == SQLITE_ROW);
    if (status != SQLITE_ROW && status != SQLITE_DONE)
        hissqlite_seterror(h, "check");
    sqlite3_reset(stmt);
    return found;
}

bool
hissqlite_write(void *history, const char *key, time_t arrived, time_t posted,
                time_t expires, const struct token *token)
{
    struct hissqlite *h = history;
    sqlite3_stmt *stmt = h->main.write;
    bool ok;

    batch_begin(h);
    bind_key(stmt, key);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64) arrived);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64) posted);
    sqlite3_bind_int64(stmt, 4, (sqlite3_int64) expires);
    sqlite3_bind_blob(stmt, 5, token, sizeof(TOKEN), SQLITE_TRANSIENT);
    ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok)
        hissqlite_seterror(h, "write");
    else if (sqlite3_changes(h->db) == 0 && h->history != NULL)
        /* The .write INSERT used ON CONFLICT DO NOTHING, so a duplicate hash
           changed nothing.  Report it (his_seterror logs a warning) but still
           return success, so a makehistory rebuild over a spool with duplicate
           Message-IDs warns and continues instead of aborting -- as hisv6 does
           (hisv6.c hisv6_writedbz, DBZSTORE_EXISTS). */
        his_seterror(h->history,
                     concat("hissqlite: duplicate message-id, write ignored: ",
                            key, (char *) NULL));
    sqlite3_reset(stmt);
    /* After the sqlite3_changes() check: a batch COMMIT must not slip between
       the INSERT and that read.  A failed write leaves the batch open; its
       earlier, successful writes commit on the next flush. */
    if (ok && !batch_advance(h))
        ok = false;
    return ok;
}

bool
hissqlite_remember(void *history, const char *key, time_t arrived,
                   time_t posted)
{
    struct hissqlite *h = history;
    sqlite3_stmt *stmt = h->main.remember;
    bool ok;

    /* INSERT ... ON CONFLICT DO NOTHING: never downgrade a real article. */
    bind_key(stmt, key);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64) arrived);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64) posted);
    ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok)
        hissqlite_seterror(h, "remember");
    sqlite3_reset(stmt);
    return ok;
}

bool
hissqlite_replace(void *history, const char *key, time_t arrived,
                  time_t posted, time_t expires, const struct token *token)
{
    struct hissqlite *h = history;
    sqlite3_stmt *stmt = h->main.replace;
    bool ok;

    /* The only token-changing op.  A NULL token downgrades real->remembered
       (prunehistory); a non-NULL token upgrades remembered->real. */
    bind_key(stmt, key);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64) arrived);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64) posted);
    sqlite3_bind_int64(stmt, 4, (sqlite3_int64) expires);
    if (token == NULL)
        sqlite3_bind_null(stmt, 5);
    else
        sqlite3_bind_blob(stmt, 5, token, sizeof(TOKEN), SQLITE_TRANSIENT);
    ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok)
        hissqlite_seterror(h, "replace");
    sqlite3_reset(stmt);
    return ok;
}

bool
hissqlite_walk(void *history, const char *reason UNUSED, void *cookie,
               bool (*callback)(void *, const HASH *, time_t, time_t, time_t,
                                const TOKEN *))
{
    struct hissqlite *h = history;
    sqlite3_stmt *stmt = h->direct_reader ? h->read.walk : h->main.walk;
    bool ok = true;
    int status;

    /* WAL gives a consistent read snapshot, unlike hisv6, no
       ICCpause/straggler re-scan is needed even while innd writes. */
    while ((status = sqlite3_step(stmt)) == SQLITE_ROW) {
        HASH hash;
        TOKEN token, *tp;
        time_t arrived, posted, expires;

        if (!copy_blob(h, stmt, 0, &hash, sizeof(HASH),
                       "corrupt hash blob in walk")) {
            ok = false;
            break;
        }
        arrived = (time_t) sqlite3_column_int64(stmt, 1);
        posted = (time_t) sqlite3_column_int64(stmt, 2);
        expires = (time_t) sqlite3_column_int64(stmt, 3);
        if (sqlite3_column_type(stmt, 4) == SQLITE_NULL) {
            tp = NULL; /* Remembered entry: callback sees no token. */
        } else {
            if (!copy_blob(h, stmt, 4, &token, sizeof(TOKEN),
                           "corrupt token blob in walk")) {
                ok = false;
                break;
            }
            tp = &token;
        }
        if (!(*callback)(cookie, &hash, arrived, posted, expires, tp)) {
            ok = false;
            break;
        }
    }
    if (status != SQLITE_DONE && ok) {
        hissqlite_seterror(h, "walk");
        ok = false;
    }
    sqlite3_reset(stmt);
    return ok;
}

/* A pending in-place action collected during the pass-1 scan, so we do not
   mutate the table through the same cursor we are scanning. */
struct expire_action {
    HASH hash;
    TOKEN token;
    time_t expires;
    bool transition; /* true: real -> remembered; false: keep, rewrite token */
};

bool
hissqlite_expire(void *history, const char *path UNUSED,
                 const char *reason UNUSED, bool writing, void *cookie,
                 time_t threshold,
                 bool (*decide)(void *, time_t, time_t, time_t,
                                struct token *))
{
    struct hissqlite *h = history;
    struct expire_action *acts;
    HASH last;
    bool ok = true, more = true, first = true;

    /*
     * hisv6 rewrites the whole file; we mutate in place.  Pass 1: evaluate
     * every token-bearing entry via the policy callback.  A real article whose
     * retention has passed is UPDATEd to remembered (token=NULL) NOT deleted
     * so the Message-ID is not re-accepted.  Pass 2: delete remembered
     * entries older than the /remember/ posting-time threshold.
     *
     * The keep/remove decision is made entirely by the caller's callback
     * (expire.c's EXPdoline), including the groupbaseexpiry case where article
     * retention is driven by expireover plus storage existence and the
     * tombstone log rather than expire.ctl: this backend is decision-agnostic
     * and only applies the verdict in place.
     *
     * Concurrency: expire is a separate writer process with its own r/w
     * connection, coordinating with innd via the WAL write-lock + busy_timeout
     * (the two-writer model described in the file header).
     * Nothing is wrapped in a transaction: each apply autocommits a single
     * UPDATE/DELETE so the lock is held one statement at a time.  Pass 1
     * streams the scan in hash-keyset pages: each page reads under its own
     * short read snapshot which is reset before the page is applied, so a long
     * expire never pins the WAL (which would block innd's checkpoints) and RAM
     * is bounded to one page rather than the whole change set.
     */

    memset(&last, 0, sizeof(last));
    acts = xmalloc(HISSQLITE_EXPIRE_BATCH * sizeof(*acts));

    while (ok && more) {
        sqlite3_stmt *scan = h->main.expire_scan;
        HASH maxhash;
        size_t n = 0; /* actions collected this page */
        int rows = 0; /* rows read this page */
        int status;

        memset(&maxhash, 0, sizeof(maxhash));
        /* Resume after the previous page's last hash (an empty blob sorts
           before every hash, so the first page starts at the beginning). */
        if (first)
            sqlite3_bind_blob(scan, 1, "", 0, SQLITE_STATIC);
        else
            sqlite3_bind_blob(scan, 1, &last, sizeof(HASH), SQLITE_TRANSIENT);
        sqlite3_bind_int(scan, 2, HISSQLITE_EXPIRE_BATCH);

        while ((status = sqlite3_step(scan)) == SQLITE_ROW) {
            TOKEN token, ltoken;
            time_t arrived, posted, expires;
            bool keep;

            /* maxhash tracks every row read (rows come in hash order), so
               after the page it is the resume point. */
            if (!copy_blob(h, scan, 0, &maxhash, sizeof(HASH),
                           "corrupt hash blob in expire scan")) {
                ok = false;
                break;
            }
            arrived = (time_t) sqlite3_column_int64(scan, 1);
            posted = (time_t) sqlite3_column_int64(scan, 2);
            expires = (time_t) sqlite3_column_int64(scan, 3);
            if (!copy_blob(h, scan, 4, &token, sizeof(TOKEN),
                           "corrupt token blob in expire scan")) {
                ok = false;
                break;
            }
            rows++;
            ltoken = token;

            keep = (*decide)(cookie, arrived, posted, expires, &ltoken);
            if (keep && memcmp(&ltoken, &token, sizeof(TOKEN)) == 0)
                continue; /* Unchanged: nothing to do. */

            acts[n].hash = maxhash;
            acts[n].transition = !keep;
            acts[n].token = ltoken;
            acts[n].expires = keep ? expires : 0;
            n++;
        }
        if (status != SQLITE_DONE && ok) {
            hissqlite_seterror(h, "expire scan");
            ok = false;
        }
        sqlite3_reset(scan); /* release the read snapshot before applying */

        /* Apply this page (only when writing; the expire(8) -t dry run passes
           writing=false, so it just scans and reports via the callback).  Each
           statement autocommits, holding the write lock one row at a time so
           innd's accepts interleave. */
        for (size_t i = 0; writing && ok && i < n; i++) {
            sqlite3_stmt *stmt;

            if (acts[i].transition) {
                stmt = h->main.transition_remember;
                sqlite3_bind_blob(stmt, 1, &acts[i].hash, sizeof(HASH),
                                  SQLITE_TRANSIENT);
            } else {
                stmt = h->main.update_token;
                sqlite3_bind_blob(stmt, 1, &acts[i].hash, sizeof(HASH),
                                  SQLITE_TRANSIENT);
                sqlite3_bind_blob(stmt, 2, &acts[i].token, sizeof(TOKEN),
                                  SQLITE_TRANSIENT);
                sqlite3_bind_int64(stmt, 3, (sqlite3_int64) acts[i].expires);
            }
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                hissqlite_seterror(h, "expire apply");
                ok = false;
            }
            sqlite3_reset(stmt);
        }

        if (rows < HISSQLITE_EXPIRE_BATCH)
            more = false; /* short page -> end of table */
        else
            last = maxhash; /* resume after the last row read */
        first = false;
    }
    free(acts);

    /* Pass 2: delete remembered entries past the /remember/ threshold, in
       bounded chunks (the statement deletes up to a LIMIT per step).  Each
       step autocommits, so the write lock is released between chunks and
       innd's accepts interleave; loop until a chunk deletes nothing. */
    if (writing && ok) {
        sqlite3_stmt *stmt = h->main.expire_remembered;

        do {
            sqlite3_bind_int64(stmt, 1, (sqlite3_int64) threshold);
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                hissqlite_seterror(h, "expire remembered");
                ok = false;
            }
            sqlite3_reset(stmt);
        } while (ok && sqlite3_changes(h->db) > 0);
    }
    return ok;
}

bool
hissqlite_ctl(void *history, int selector, void *val)
{
    struct hissqlite *h = history;

    switch (selector) {
    case HISCTLG_PATH:
        *(char **) val = h->path;
        return true;
    case HISCTLS_SYNCCOUNT:
        /* No-op: writes autocommit, so there is no batch size to set. */
        return true;
    case HISCTLS_STATINTERVAL:
        /* No-op: WAL readers always see a consistent snapshot; there is no
           swapped-inode poll as in hisv6. */
        return true;
    case HISCTLS_NPAIRS:
    case HISCTLS_IGNOREOLD:
        /* No-op: the B-tree grows itself; there is no fixed-size table to
           presize and no rebuild-to-grow. */
        return true;
    case HISCTLG_INPLACEEXPIRE:
        /* hissqlite expires in place (UPDATE/DELETE on the live DB), so
           expire(8) must open it read/write, not the hisv6 rebuild-and-swap
           way.  See hissqlite_expire(). */
        *(bool *) val = true;
        return true;
    case HISCTLS_PATH:
        /* Deferred-open path (makehistory): HISopen(NULL) left the database
           unopened; set the real path now and perform the open/create.  Refuse
           a second path set, as hisv6 does. */
        if (h->path != NULL) {
            his_seterror(h->history,
                         concat("hissqlite: path already set in handle",
                                (char *) NULL));
            return false;
        }
        h->path = concat((char *) val, ".sqlite", (char *) NULL);
        if (!hissqlite_doopen(h)) {
            free(h->path);
            h->path = NULL;
            return false;
        }
        return true;
    default:
        return false;
    }
}

#else /* ! HAVE_SQLITE3 */

void *
hissqlite_open(const char *path UNUSED, int flags UNUSED,
               struct history *history UNUSED)
{
    warn("hissqlite: SQLite support not enabled");
    return NULL;
}

bool
hissqlite_close(void *history UNUSED)
{
    return false;
}

bool
hissqlite_sync(void *history UNUSED)
{
    return false;
}

bool
hissqlite_lookup(void *history UNUSED, const char *key UNUSED,
                 time_t *arrived UNUSED, time_t *posted UNUSED,
                 time_t *expires UNUSED, struct token *token UNUSED)
{
    return false;
}

bool
hissqlite_check(void *history UNUSED, const char *key UNUSED)
{
    return false;
}

bool
hissqlite_write(void *history UNUSED, const char *key UNUSED,
                time_t arrived UNUSED, time_t posted UNUSED,
                time_t expires UNUSED, const struct token *token UNUSED)
{
    return false;
}

bool
hissqlite_replace(void *history UNUSED, const char *key UNUSED,
                  time_t arrived UNUSED, time_t posted UNUSED,
                  time_t expires UNUSED, const struct token *token UNUSED)
{
    return false;
}

bool
hissqlite_expire(void *history UNUSED, const char *path UNUSED,
                 const char *reason UNUSED, bool writing UNUSED,
                 void *cookie UNUSED, time_t threshold UNUSED,
                 bool (*decide)(void *, time_t, time_t, time_t, struct token *)
                     UNUSED)
{
    return false;
}

bool
hissqlite_walk(void *history UNUSED, const char *reason UNUSED,
               void *cookie UNUSED,
               bool (*callback)(void *, const HASH *, time_t, time_t, time_t,
                                const struct token *) UNUSED)
{
    return false;
}

bool
hissqlite_remember(void *history UNUSED, const char *key UNUSED,
                   time_t arrived UNUSED, time_t posted UNUSED)
{
    return false;
}

bool
hissqlite_ctl(void *history UNUSED, int selector UNUSED, void *val UNUSED)
{
    return false;
}

#endif /* ! HAVE_SQLITE3 */
