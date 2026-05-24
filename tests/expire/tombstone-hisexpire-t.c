/* Integration test: HISexpire callback path consults the tombstone hashset.
 *
 * Builds a temporary history file with HISopen/HISwrite, populates a
 * tombstone hashset for some of the tokens, runs HISexpire with a
 * callback that mirrors EXPdoline's decision tree -- including the
 * SELFEXPIRE branch (gap G).  Verifies the resulting new history file
 * contains exactly the tokens that should survive.
 *
 * The callback distinguishes four token categories:
 *   N_KEPT             not tombstoned, not self-expiring -> keep
 *   N_TOMBSTONED       in tombstone (fast path)          -> drop
 *   N_SELFEXP_GONE     self-expiring, simulated NOENT    -> drop (slow path)
 *   N_SELFEXP_ALIVE    self-expiring, simulated alive    -> keep
 *
 * The slow path mirrors EXPdoline's behaviour for backends where
 * SMprobe(SELFEXPIRE) is true: even with a tombstone hashset present,
 * we still consult the simulated SMretrieve because articles can vanish
 * via wrap-around without going through SMcancel.
 *
 * Written by Kevin Bowling in 2026.
 */

#include "portable/system.h"

#include <errno.h>
#include <sys/stat.h>

#include "inn/hashtab.h"
#include "inn/history.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/storage.h"
#include "inn/tombstone.h"
#include "tap/basic.h"


#define N_KEPT          5
#define N_TOMBSTONED    4
#define N_SELFEXP_GONE  3
#define N_SELFEXP_ALIVE 2

#define N_TOTAL (N_KEPT + N_TOMBSTONED + N_SELFEXP_GONE + N_SELFEXP_ALIVE)


/* Cookie threading two hashsets through HISexpire's callback: the
 * tombstone (real EXPdoline checks this first) and a "gone" set used to
 * simulate SMretrieve(RETR_STAT) == NULL for self-expiring backends. */
struct expire_cookie {
    struct hash *tombstone;
    struct hash *gone;
};


/* Mirror of EXPdoline's decision tree for tokens that may be tombstoned
 * or self-expiring.  Returns true to keep, false to drop.
 *
 * Tokens are tagged via their type byte:
 *   type=1 - non-self-expiring backend
 *   type=2 - self-expiring backend (CNFS-like)
 */
static bool
test_expire_cb(void *cookie, time_t arrived UNUSED, time_t posted UNUSED,
               time_t expires UNUSED, TOKEN *token)
{
    struct expire_cookie *c = cookie;

    /* Fast path: tombstone hit -> drop, no further checks. */
    if (c->tombstone != NULL && tombstone_present(c->tombstone, token))
        return false;

    /* Self-expiring backend (gap G).  Even with a tombstone in hand, we
     * cannot trust "not in tombstone == alive" because wrap-around can
     * silently delete articles.  Simulate the SMretrieve(RETR_STAT)
     * check via the `gone` hashset. */
    if (token->type == 2) {
        if (tombstone_present(c->gone, token))
            return false;
        return true;
    }

    /* Non-self-expiring: trust the tombstone.  Not in tombstone means
     * still alive. */
    return true;
}


/* Build a synthetic token whose bytes encode n and whose type byte
 * tags whether it is from a self-expiring backend. */
static TOKEN
make_token(unsigned char type, unsigned long n)
{
    TOKEN t;

    memset(&t, 0, sizeof(t));
    t.type = type;
    t.class = 0;
    t.token[0] = (n >> 24) & 0xff;
    t.token[1] = (n >> 16) & 0xff;
    t.token[2] = (n >> 8) & 0xff;
    t.token[3] = n & 0xff;
    return t;
}


static char *
make_msgid(unsigned long n)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "<art-%lu@tombstone.test>", n);
    return xstrdup(buf);
}


/* Count the number of non-empty lines in the history file (one per
 * surviving entry). */
static unsigned long
count_history_lines(const char *path)
{
    FILE *f;
    char line[SMBUF];
    unsigned long n = 0;

    f = fopen(path, "r");
    if (f == NULL)
        return 0;
    while (fgets(line, sizeof(line), f) != NULL) {
        if (line[0] != '\n' && line[0] != '\0')
            n++;
    }
    fclose(f);
    return n;
}


/* Helper: insert a TOKEN copy into a hashset. */
static void
hash_add_token(struct hash *h, const TOKEN *token)
{
    TOKEN *p = xmalloc(sizeof(TOKEN));
    *p = *token;
    if (!hash_insert(h, p, p))
        free(p);
}


int
main(void)
{
    struct history *h;
    struct expire_cookie cookie;
    char tmpdir[64];
    char histpath[128];
    char newhistpath[140];
    TOKEN tokens[N_TOTAL];
    unsigned long i;
    unsigned long base;
    bool expire_ok;
    struct stat sb;

    test_init(9);

    strlcpy(tmpdir, "tombstone-hisexp-XXXXXX", sizeof(tmpdir));
    if (mkdtemp(tmpdir) == NULL)
        sysbail("can't create temp directory");
    snprintf(histpath, sizeof(histpath), "%s/history", tmpdir);

    /* Layout of the tokens array:
       [0 .. N_KEPT)                                     non-selfexpire keep
       [N_KEPT .. N_KEPT+N_TOMBSTONED)                   non-selfexpire tomb
       [N_KEPT+N_TOMBSTONED .. + N_SELFEXP_GONE)         selfexpire gone
       [..rest]                                          selfexpire alive
    */
    base = 0;
    for (i = 0; i < N_KEPT; i++)
        tokens[base + i] = make_token(1, base + i);
    base += N_KEPT;
    for (i = 0; i < N_TOMBSTONED; i++)
        tokens[base + i] = make_token(1, base + i);
    base += N_TOMBSTONED;
    for (i = 0; i < N_SELFEXP_GONE; i++)
        tokens[base + i] = make_token(2, base + i);
    base += N_SELFEXP_GONE;
    for (i = 0; i < N_SELFEXP_ALIVE; i++)
        tokens[base + i] = make_token(2, base + i);

    /* Populate a fresh history database with all N_TOTAL entries. */
    h = HISopen(histpath, "hisv6", HIS_CREAT | HIS_RDWR);
    if (h == NULL)
        bail("can't create history at %s", histpath);
    for (i = 0; i < N_TOTAL; i++) {
        char *msgid = make_msgid(i);
        if (!HISwrite(h, msgid, (time_t) 1000000 + i, (time_t) 1000000 + i,
                      (time_t) 0, &tokens[i]))
            bail("can't write history entry %lu: %s", i, HISerror(h));
        free(msgid);
    }
    HISsync(h);
    HISclose(h);
    ok(1, stat(histpath, &sb) == 0 && sb.st_size > 0);

    /* Build the tombstone hashset (only the N_TOMBSTONED slice). */
    cookie.tombstone = tombstone_hash_create(8);
    for (i = N_KEPT; i < N_KEPT + N_TOMBSTONED; i++)
        hash_add_token(cookie.tombstone, &tokens[i]);
    ok(2, hash_count(cookie.tombstone) == N_TOMBSTONED);

    /* Build the "gone" hashset for self-expiring backends (only the
     * N_SELFEXP_GONE slice; selfexpire-alive tokens are NOT added). */
    cookie.gone = tombstone_hash_create(8);
    base = N_KEPT + N_TOMBSTONED;
    for (i = 0; i < N_SELFEXP_GONE; i++)
        hash_add_token(cookie.gone, &tokens[base + i]);
    ok(3, hash_count(cookie.gone) == N_SELFEXP_GONE);

    /* Reopen read-only and run HISexpire with our SELFEXPIRE-aware
     * callback. */
    h = HISopen(histpath, "hisv6", HIS_RDONLY);
    if (h == NULL)
        bail("can't reopen history at %s", histpath);
    snprintf(newhistpath, sizeof(newhistpath), "%s/history.new", tmpdir);
    /* High threshold so dropped entries do not survive as remember-only
     * records; we want a clean count of survivors. */
    expire_ok = HISexpire(h, newhistpath, NULL, true, &cookie,
                          (time_t) 2000000000, test_expire_cb);
    ok(4, expire_ok);
    HISclose(h);

    /* The new history at <newhistpath>.n should contain exactly the
     * survivors: N_KEPT + N_SELFEXP_ALIVE.  Tombstoned and selfexpire-
     * gone entries should be absent. */
    {
        char actual[160];
        unsigned long expected = N_KEPT + N_SELFEXP_ALIVE;
        snprintf(actual, sizeof(actual), "%s.n", newhistpath);
        ok(5, count_history_lines(actual) == expected);
    }

    /* Per-category lookup checks: kept and selfexpire-alive present;
     * tombstoned and selfexpire-gone absent. */
    {
        char actual[160];
        struct history *hnew;
        unsigned long kept_present = 0;
        unsigned long tomb_present = 0;
        unsigned long alive_present = 0;
        unsigned long gone_present = 0;
        TOKEN found;

        snprintf(actual, sizeof(actual), "%s.n", newhistpath);
        hnew = HISopen(actual, "hisv6", HIS_RDONLY);
        if (hnew == NULL)
            bail("can't open new history %s", actual);

        for (i = 0; i < N_KEPT; i++) {
            char *msgid = make_msgid(i);
            if (HISlookup(hnew, msgid, NULL, NULL, NULL, &found))
                kept_present++;
            free(msgid);
        }
        for (i = N_KEPT; i < N_KEPT + N_TOMBSTONED; i++) {
            char *msgid = make_msgid(i);
            if (HISlookup(hnew, msgid, NULL, NULL, NULL, &found))
                tomb_present++;
            free(msgid);
        }
        base = N_KEPT + N_TOMBSTONED;
        for (i = 0; i < N_SELFEXP_GONE; i++) {
            char *msgid = make_msgid(base + i);
            if (HISlookup(hnew, msgid, NULL, NULL, NULL, &found))
                gone_present++;
            free(msgid);
        }
        base += N_SELFEXP_GONE;
        for (i = 0; i < N_SELFEXP_ALIVE; i++) {
            char *msgid = make_msgid(base + i);
            if (HISlookup(hnew, msgid, NULL, NULL, NULL, &found))
                alive_present++;
            free(msgid);
        }

        ok(6, kept_present == N_KEPT);
        ok(7, tomb_present == 0);
        ok(8, gone_present == 0); /* SELFEXPIRE branch correctly drops */
        ok(9, alive_present == N_SELFEXP_ALIVE);
        HISclose(hnew);
    }

    hash_free(cookie.tombstone);
    hash_free(cookie.gone);

    /* Cleanup. */
    {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "/bin/rm -rf %s", tmpdir);
        if (system(cmd) < 0)
            sysdiag("can't clean up %s", tmpdir);
    }

    return 0;
}
