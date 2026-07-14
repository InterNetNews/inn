/*
**  Runtime test for the hissqlite history backend.
**
**  Exercises the full HIS_METHOD vtable through the public HIS API against a
**  real SQLite database: open/create, write, remember, check, lookup, replace,
**  walk, the two-horizon expire (real->remembered transition +
**  remember-delete), sync, close, and persistence across reopen.
**
**  Written by Kevin Bowling in 2026.
*/

#include "portable/system.h"

#include "inn/history.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/storage.h"
#include "tap/basic.h"

#ifndef HAVE_SQLITE3

int
main(void)
{
    skip_all("not built with SQLite");
    return 0;
}

#else

#    include <sqlite3.h>
#    include <sys/stat.h>

#    define N_TOKEN    100 /* entries with a storage token */
#    define N_REMEMBER 20  /* remembered (token-less) entries */
#    define BASE       ((time_t) 1600000000)

/* msgid 0..N_TOKEN-1 are token entries; N_TOKEN..N_TOKEN+N_REMEMBER-1 are
   remembered.  arrived == posted == BASE + i for entry i. */
static char *
make_msgid(unsigned long n)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "<art-%lu@hissqlite.test>", n);
    return xstrdup(buf);
}

struct walkcount {
    unsigned long total;
    unsigned long with_token;
};

static bool
walk_cb(void *cookie, const HASH *hash UNUSED, time_t arrived UNUSED,
        time_t posted UNUSED, time_t expires UNUSED, const TOKEN *token)
{
    struct walkcount *w = cookie;
    w->total++;
    if (token != NULL)
        w->with_token++;
    return true;
}

/* Expire policy: keep token entries with arrived >= cutoff, drop (->remember)
   the older ones. */
static time_t expire_cutoff;
static bool
decide_drop_old(void *cookie UNUSED, time_t arrived, time_t posted UNUSED,
                time_t expires UNUSED, TOKEN *token UNUSED)
{
    return arrived >= expire_cutoff;
}

static bool
decide_keep_all(void *cookie UNUSED, time_t arrived UNUSED,
                time_t posted UNUSED, time_t expires UNUSED,
                TOKEN *token UNUSED)
{
    return true;
}

/* Does HISlookup find this msgid, and (optionally) does it carry a token?
   HISlookup only reports token-bearing entries (as hisv6 does), so found
   implies has_token; the flag is kept so assertions read explicitly. */
static bool
present_with_token(struct history *h, unsigned long n, bool *has_token,
                   TOKEN *out)
{
    char *msgid = make_msgid(n);
    TOKEN token;
    bool found;

    memset(&token, 0, sizeof(token));
    found = HISlookup(h, msgid, NULL, NULL, NULL, &token);
    if (has_token != NULL)
        *has_token = (found && token.type != TOKEN_EMPTY);
    if (out != NULL)
        *out = token;
    free(msgid);
    return found;
}

/* Is this msgid a remembered entry: known to HIScheck (so it is refused on
   re-offer) but not found by HISlookup (no article to fetch), matching the
   hisv6 contract for token-less history lines? */
static bool
remembered(struct history *h, unsigned long n)
{
    char *msgid = make_msgid(n);
    bool r;

    r = HIScheck(h, msgid) && !HISlookup(h, msgid, NULL, NULL, NULL, NULL);
    free(msgid);
    return r;
}

int
main(void)
{
    struct history *h;
    char tmpdir[64], histpath[128];
    TOKEN token, t2;
    unsigned long i;
    struct walkcount wc;
    bool has_token;

    test_init(32);

    strlcpy(tmpdir, "hissqlite-XXXXXX", sizeof(tmpdir));
    if (mkdtemp(tmpdir) == NULL)
        sysbail("can't create temp directory");
    snprintf(histpath, sizeof(histpath), "%s/history", tmpdir);

    /* create + populate */
    h = HISopen(histpath, "hissqlite", HIS_CREAT | HIS_RDWR);
    ok(1, h != NULL);
    if (h == NULL)
        bail("can't create hissqlite history at %s", histpath);

    memset(&token, 0, sizeof(token));
    token.type = 1;
    memcpy(token.token, "0123456789abcdef", sizeof(token.token));

    for (i = 0; i < N_TOKEN; i++) {
        char *msgid = make_msgid(i);
        if (!HISwrite(h, msgid, BASE + i, BASE + i, (time_t) 0, &token))
            bail("HISwrite %lu failed: %s", i, HISerror(h));
        free(msgid);
    }
    for (i = N_TOKEN; i < N_TOKEN + N_REMEMBER; i++) {
        char *msgid = make_msgid(i);
        if (!HISremember(h, msgid, BASE + i, BASE + i))
            bail("HISremember %lu failed: %s", i, HISerror(h));
        free(msgid);
    }
    ok(2, HISsync(h));
    ok(3, HISclose(h));

    /* reopen read/write and verify */
    h = HISopen(histpath, "hissqlite", HIS_RDWR);
    ok(4, h != NULL);
    if (h == NULL)
        bail("can't reopen hissqlite history");

    /* check: real present, remembered present, absent absent */
    {
        char *m_real = make_msgid(5);
        char *m_rem = make_msgid(N_TOKEN + 1);
        char *m_abs = make_msgid(99999);
        ok(5, HIScheck(h, m_real));
        ok(6, HIScheck(h, m_rem));
        ok(7, !HIScheck(h, m_abs));
        free(m_real);
        free(m_rem);
        free(m_abs);
    }

    /* lookup: real -> token round-trips; remembered -> not found (hisv6
       parity: only HIScheck reports remembered entries) */
    {
        char *m = make_msgid(5);
        time_t arrived = 0, posted = 0, expires = -1;
        memset(&t2, 0xff, sizeof(t2));
        ok(8, HISlookup(h, m, &arrived, &posted, &expires, &t2)
                  && arrived == BASE + 5 && posted == BASE + 5 && t2.type == 1
                  && memcmp(t2.token, token.token, sizeof(token.token)) == 0);
        free(m);
    }
    ok(9, remembered(h, N_TOKEN + 2));
    ok(10, !present_with_token(h, 99999, NULL, NULL) && !remembered(h, 99999));

    /* walk (before mutations): total and token counts */
    wc.total = wc.with_token = 0;
    HISwalk(h, NULL, &wc, walk_cb);
    ok(11, wc.total == N_TOKEN + N_REMEMBER);
    ok(12, wc.with_token == N_TOKEN);

    /* replace: change a token; downgrade real->remembered */
    memset(&t2, 0, sizeof(t2));
    t2.type = 2;
    memcpy(t2.token, "fedcba9876543210", sizeof(t2.token));
    {
        char *m = make_msgid(0);
        TOKEN got;
        HISreplace(h, m, BASE, BASE, 0, &t2);
        present_with_token(h, 0, &has_token, &got);
        ok(13, has_token && got.type == 2
                   && memcmp(got.token, t2.token, sizeof(t2.token)) == 0);
        free(m);
    }
    {
        char *m = make_msgid(1); /* real -> remembered (NULL token) */
        HISreplace(h, m, BASE + 1, BASE + 1, 0, NULL);
        ok(14, remembered(h, 1));
        free(m);
    }

    /* expire pass 1: drop old token entries -> remembered */
    expire_cutoff = BASE + N_TOKEN / 2; /* keep i>=50, drop i<50 */
    ok(15, HISexpire(h, NULL, NULL, true, NULL, (time_t) (BASE - 1),
                     decide_drop_old));
    /* #10 (arrived BASE+10 < cutoff) -> now remembered; #90 still a token */
    {
        bool r10 = remembered(h, 10);
        bool t90 = present_with_token(h, 90, &has_token, NULL) && has_token;
        ok(16, r10 && t90);
    }

    /* expire pass 2: remember-delete past the threshold */
    ok(17,
       HISexpire(h, NULL, NULL, true, NULL, BASE + 1000000, decide_keep_all));
    /* original remembered (#N_TOKEN+5) gone; surviving token (#90) stays */
    {
        char *m = make_msgid(N_TOKEN + 5);
        bool rem_gone = !HIScheck(h, m);
        bool tok_stays =
            present_with_token(h, 90, &has_token, NULL) && has_token;
        ok(18, rem_gone && tok_stays);
        free(m);
    }

    /* HISCTLG_INPLACEEXPIRE capability contract (expire.c relies on it) */
    {
        bool inplace = false;
        ok(19, HISctl(h, HISCTLG_INPLACEEXPIRE, &inplace) && inplace);
    }
    HISclose(h);

    /* A backend that does not implement the selector must leave the flag
       untouched (hisv6 -> stays false, i.e. rebuild-and-swap). */
    {
        struct history *hv;
        bool inplace = false;
        char hv6path[160];

        snprintf(hv6path, sizeof(hv6path), "%s/hv6", tmpdir);
        hv = HISopen(hv6path, "hisv6", HIS_CREAT | HIS_RDWR);
        if (hv != NULL)
            HISctl(hv, HISCTLG_INPLACEEXPIRE, &inplace);
        ok(20, hv != NULL && !inplace);
        if (hv != NULL)
            HISclose(hv);
    }

    /* corrupt token -> reported, and schema-version mismatch -> refused.
       Both need DB-level tampering there is no HIS API for, so reach in with
       SQLite directly (this whole test is already HAVE_SQLITE3-only). */
    {
        char vpath[160], vdb[176];
        struct history *vh;
        sqlite3 *raw;
        TOKEN tk;

        snprintf(vpath, sizeof(vpath), "%s/v", tmpdir);
        snprintf(vdb, sizeof(vdb), "%s.sqlite", vpath);
        vh = HISopen(vpath, "hissqlite", HIS_CREAT | HIS_RDWR);
        memset(&tk, 0, sizeof(tk));
        tk.type = 1;
        HISwrite(vh, "<corrupt@test>", BASE, BASE, 0, &tk);
        HISsync(vh);
        HISclose(vh);

        /* Truncate the stored token to a wrong length: lookup must report the
           entry as not-found rather than mask the corruption (MI2). */
        if (sqlite3_open(vdb, &raw) == SQLITE_OK) {
            sqlite3_exec(raw, "update hist set token = x'0011'", NULL, NULL,
                         NULL);
            sqlite3_close(raw);
        }
        vh = HISopen(vpath, "hissqlite", HIS_RDONLY);
        if (vh != NULL) {
            TOKEN got;
            memset(&got, 0, sizeof(got));
            ok(21, !HISlookup(vh, "<corrupt@test>", NULL, NULL, NULL, &got));
            HISclose(vh);
        } else {
            ok(21, false);
        }

        /* Bump the stored schema version: reopen must refuse cleanly (MA2). */
        if (sqlite3_open(vdb, &raw) == SQLITE_OK) {
            sqlite3_exec(raw,
                         "update misc set value = 99 where key = 'version'",
                         NULL, NULL, NULL);
            sqlite3_close(raw);
        }
        vh = HISopen(vpath, "hissqlite", HIS_RDONLY);
        ok(22, vh == NULL);
        if (vh != NULL)
            HISclose(vh);
    }

    /* deferred-open path: HISopen(NULL) then HISCTLS_PATH, as makehistory
       drives it.  The handle must defer the open until the path is set, then
       create the real database at that path (not an anonymous temp DB), and a
       write must land in that file. */
    {
        char dpath[160], ddb[176];
        struct history *dh;
        struct stat sb;
        TOKEN tk;

        snprintf(dpath, sizeof(dpath), "%s/deferred", tmpdir);
        snprintf(ddb, sizeof(ddb), "%s.sqlite", dpath);
        dh = HISopen(NULL, "hissqlite", HIS_CREAT | HIS_RDWR);
        ok(23, dh != NULL && HISctl(dh, HISCTLS_PATH, dpath)
                   && stat(ddb, &sb) == 0);
        if (dh != NULL) {
            memset(&tk, 0, sizeof(tk));
            tk.type = 1;
            HISwrite(dh, "<deferred@test>", BASE, BASE, 0, &tk);
            HISsync(dh);
            HISclose(dh);
        }
        /* Reopen the real file and confirm the entry persisted there, proving
           the write did not go to a throwaway temporary database. */
        dh = HISopen(dpath, "hissqlite", HIS_RDWR);
        ok(24, dh != NULL && HIScheck(dh, "<deferred@test>"));
        if (dh != NULL)
            HISclose(dh);

        /* Closing a deferred handle whose path was never set (so no database
           was ever opened) must be a clean no-op, not a NULL-connection crash.
         */
        dh = HISopen(NULL, "hissqlite", HIS_CREAT | HIS_RDWR);
        ok(25, dh != NULL && HISclose(dh));
    }

    /* duplicate HISwrite is tolerated (makehistory over a spool with a
       duplicate Message-ID must warn and continue, not abort): both calls
       succeed and the entry exists. */
    {
        char wpath[160];
        struct history *wh;
        TOKEN w1, w2;
        bool first, second;

        snprintf(wpath, sizeof(wpath), "%s/dup", tmpdir);
        wh = HISopen(wpath, "hissqlite", HIS_CREAT | HIS_RDWR);
        memset(&w1, 0, sizeof(w1));
        w1.type = 1;
        memset(&w2, 0, sizeof(w2));
        w2.type = 2;
        first = wh != NULL && HISwrite(wh, "<dup@test>", BASE, BASE, 0, &w1);
        second = wh != NULL && HISwrite(wh, "<dup@test>", BASE, BASE, 0, &w2);
        ok(26, first && second && wh != NULL && HIScheck(wh, "<dup@test>"));
        if (wh != NULL)
            HISclose(wh);
    }

    /* a dry-run expire (writing == false, as expire(8) -t now passes for an
       in-place backend) scans but must change nothing: an entry the policy
       would drop stays a real token. */
    {
        char epath[160];
        struct history *eh;
        TOKEN et, got;
        bool kept;

        snprintf(epath, sizeof(epath), "%s/dry", tmpdir);
        eh = HISopen(epath, "hissqlite", HIS_CREAT | HIS_RDWR);
        memset(&et, 0, sizeof(et));
        et.type = 1;
        if (eh != NULL)
            HISwrite(eh, "<dry@test>", BASE, BASE, 0, &et);
        expire_cutoff =
            BASE + 100; /* decide_drop_old would drop arrived=BASE */
        if (eh != NULL)
            HISexpire(eh, NULL, NULL, false, NULL, BASE + 1000000,
                      decide_drop_old);
        memset(&got, 0, sizeof(got));
        kept =
            eh != NULL && HISlookup(eh, "<dry@test>", NULL, NULL, NULL, &got)
            && got.type == 1; /* still real, not transitioned to remembered */
        ok(27, kept);
        if (eh != NULL)
            HISclose(eh);
    }

    /* bulk write batching: HIS_INCORE (the rebuild hint makehistory passes)
       makes writes commit in transactions of BULK_BATCH rows.  Writes inside
       an open batch are deferred (invisible to a WAL direct reader) until a
       commit: on batch full, on HISsync, and on close. */
#    define BULK_BATCH 50000 /* must match HISSQLITE_BULK_BATCH */
    {
        char bpath[160];
        struct history *bw, *br;
        TOKEN bt;
        unsigned long n;
        char msgid[64];

        snprintf(bpath, sizeof(bpath), "%s/batch", tmpdir);
        bw = HISopen(bpath, "hissqlite", HIS_CREAT | HIS_RDWR | HIS_INCORE);
        if (bw == NULL)
            bail("can't create bulk hissqlite history");
        memset(&bt, 0, sizeof(bt));
        bt.type = 1;

        /* 5 writes: the batch is far from full, so it is still open and a
           WAL direct reader must not see them yet. */
        for (n = 0; n < 5; n++) {
            snprintf(msgid, sizeof(msgid), "<batch-%lu@test>", n);
            if (!HISwrite(bw, msgid, BASE, BASE, 0, &bt))
                bail("batched HISwrite failed: %s", HISerror(bw));
        }
        br = HISopen(bpath, "hissqlite", HIS_RDONLY);
        ok(28, br != NULL && !HIScheck(br, "<batch-0@test>"));

        /* HISsync commits the open batch; the same reader now sees them. */
        ok(29, HISsync(bw) && br != NULL && HIScheck(br, "<batch-0@test>")
                   && HIScheck(br, "<batch-4@test>"));

        /* BULK_BATCH + 3 more writes: one batch fills and commits on its own
           (its last row, BULK_BATCH+4, becomes visible), three stay pending
           in the next batch (BULK_BATCH+5..+7, invisible). */
        for (n = 5; n < 5 + BULK_BATCH + 3; n++) {
            snprintf(msgid, sizeof(msgid), "<batch-%lu@test>", n);
            if (!HISwrite(bw, msgid, BASE, BASE, 0, &bt))
                bail("batched HISwrite failed: %s", HISerror(bw));
        }
        {
            bool committed, pending_hidden;

            snprintf(msgid, sizeof(msgid), "<batch-%d@test>", BULK_BATCH + 4);
            committed = br != NULL && HIScheck(br, msgid);
            snprintf(msgid, sizeof(msgid), "<batch-%d@test>", BULK_BATCH + 7);
            pending_hidden = br != NULL && !HIScheck(br, msgid);
            ok(30, committed && pending_hidden);
        }
        if (br != NULL)
            HISclose(br);

        /* Close flushes the pending tail; everything written is present. */
        if (!HISclose(bw))
            bail("batched HISclose failed");
        br = HISopen(bpath, "hissqlite", HIS_RDONLY);
        snprintf(msgid, sizeof(msgid), "<batch-%d@test>", BULK_BATCH + 7);
        ok(31, br != NULL && HIScheck(br, "<batch-0@test>")
                   && HIScheck(br, msgid));
        if (br != NULL)
            HISclose(br);
    }

    /* Without HIS_INCORE, writes autocommit: immediately visible to a direct
       reader with no sync -- the steady-state two-writer invariant. */
    {
        char apath[160];
        struct history *aw, *ar;
        TOKEN at;

        snprintf(apath, sizeof(apath), "%s/autocommit", tmpdir);
        aw = HISopen(apath, "hissqlite", HIS_CREAT | HIS_RDWR);
        memset(&at, 0, sizeof(at));
        at.type = 1;
        if (aw != NULL)
            HISwrite(aw, "<auto@test>", BASE, BASE, 0, &at);
        ar = HISopen(apath, "hissqlite", HIS_RDONLY);
        ok(32, aw != NULL && ar != NULL && HIScheck(ar, "<auto@test>"));
        if (ar != NULL)
            HISclose(ar);
        if (aw != NULL)
            HISclose(aw);
    }

    {
        char cmd[160];
        snprintf(cmd, sizeof(cmd), "/bin/rm -rf %s", tmpdir);
        if (system(cmd) < 0)
            sysdiag("can't clean up %s", tmpdir);
    }
    return 0;
}

#endif /* HAVE_SQLITE3 */
