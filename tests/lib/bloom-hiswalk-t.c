/* Integration test: HISwalk + bloom filter for expireover token cache.
 *
 * Creates a temporary history file with a mix of entries (some with tokens,
 * some remembered-only), builds a bloom filter via HISwalk, and verifies
 * that the bloom filter correctly identifies articles with tokens vs.
 * remembered entries. */

#include "portable/system.h"

#include <errno.h>
#include <sys/stat.h>

#include "inn/bloom.h"
#include "inn/history.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/storage.h"
#include "tap/basic.h"

#define N_WITH_TOKEN  500
#define N_REMEMBERED  100
#define N_NOT_IN_HIST 200


/*
**  Generate a deterministic message-ID from an integer.
*/
static char *
make_msgid(unsigned long n)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "<art-%lu@hiswalk.test>", n);
    return xstrdup(buf);
}


/*
**  HISwalk callback: add entries with tokens to the bloom filter.
**  Same logic as build_bloom_cb in expireover.c.
*/
static bool
build_bloom_cb(void *cookie, const HASH *hash,
               time_t arrived UNUSED, time_t posted UNUSED,
               time_t expires UNUSED, const TOKEN *token)
{
    if (token != NULL)
        bloom_add(cookie, hash);
    return true;
}


int
main(void)
{
    struct history *h;
    struct bloom_filter *bloom;
    char tmpdir[64];
    char histpath[128];
    TOKEN token;
    unsigned long i;
    unsigned long bloom_misses;
    unsigned long false_negatives;
    bool walk_ok;

    test_init(8);

    /* Create temporary directory for the history database. */
    strlcpy(tmpdir, "bloom-hiswalk-XXXXXX", sizeof(tmpdir));
    if (mkdtemp(tmpdir) == NULL)
        sysbail("can't create temp directory");
    snprintf(histpath, sizeof(histpath), "%s/history", tmpdir);

    /* Create and populate the history database. */
    h = HISopen(histpath, "hisv6", HIS_CREAT | HIS_RDWR);
    if (h == NULL)
        bail("can't create history at %s", histpath);

    memset(&token, 0, sizeof(token));
    token.type = 1;

    /* Write entries with storage tokens. */
    for (i = 0; i < N_WITH_TOKEN; i++) {
        char *msgid = make_msgid(i);
        if (!HISwrite(h, msgid, (time_t) 1000000 + i, (time_t) 1000000 + i,
                      (time_t) 0, &token))
            bail("can't write history entry %lu: %s", i, HISerror(h));
        free(msgid);
    }

    /* Write remembered entries (no token). */
    for (i = N_WITH_TOKEN; i < N_WITH_TOKEN + N_REMEMBERED; i++) {
        char *msgid = make_msgid(i);
        if (!HISremember(h, msgid, (time_t) 1000000 + i,
                         (time_t) 1000000 + i))
            bail("can't remember history entry %lu: %s", i, HISerror(h));
        free(msgid);
    }

    HISsync(h);
    HISclose(h);
    ok(1, true); /* history created and populated */

    /* Reopen read-only (as expireover does). */
    h = HISopen(histpath, "hisv6", HIS_RDONLY);
    if (h == NULL)
        bail("can't reopen history at %s", histpath);
    ok(2, true); /* history reopened */

    /* Build the bloom filter via HISwalk. */
    bloom = bloom_create(N_WITH_TOKEN + N_REMEMBERED, 10000);
    walk_ok = HISwalk(h, NULL, bloom, build_bloom_cb);
    ok(3, walk_ok); /* HISwalk succeeded */
    ok(4, bloom_count(bloom) == N_WITH_TOKEN); /* only token entries added */

    /* Verify: all token entries should be bloom hits. */
    false_negatives = 0;
    for (i = 0; i < N_WITH_TOKEN; i++) {
        char *msgid = make_msgid(i);
        HASH hash = HashMessageID(msgid);
        if (!bloom_check(bloom, &hash))
            false_negatives++;
        free(msgid);
    }
    ok(5, false_negatives == 0); /* no false negatives for token entries */
    if (false_negatives > 0)
        diag("false negatives: %lu out of %d", false_negatives, N_WITH_TOKEN);

    /* Verify: remembered entries should NOT be in the bloom filter.
     * (Some may be false positives, but most should miss.) */
    bloom_misses = 0;
    for (i = N_WITH_TOKEN; i < N_WITH_TOKEN + N_REMEMBERED; i++) {
        char *msgid = make_msgid(i);
        HASH hash = HashMessageID(msgid);
        if (!bloom_check(bloom, &hash))
            bloom_misses++;
        free(msgid);
    }
    ok(6, bloom_misses > N_REMEMBERED * 9 / 10); /* >90% should miss */
    diag("remembered entries: %lu/%d were bloom misses (expected most)",
         bloom_misses, N_REMEMBERED);

    /* Verify: entries not in history at all should mostly miss. */
    bloom_misses = 0;
    for (i = N_WITH_TOKEN + N_REMEMBERED;
         i < N_WITH_TOKEN + N_REMEMBERED + N_NOT_IN_HIST; i++) {
        char *msgid = make_msgid(i);
        HASH hash = HashMessageID(msgid);
        if (!bloom_check(bloom, &hash))
            bloom_misses++;
        free(msgid);
    }
    ok(7, bloom_misses > N_NOT_IN_HIST * 9 / 10); /* >90% should miss */
    diag("not-in-history entries: %lu/%d were bloom misses (expected most)",
         bloom_misses, N_NOT_IN_HIST);

    bloom_free(bloom);
    HISclose(h);

    /* Cleanup temp directory. */
    {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "/bin/rm -rf %s", tmpdir);
        if (system(cmd) < 0)
            sysdiag("can't clean up %s", tmpdir);
    }
    ok(8, true); /* cleanup */

    return 0;
}
