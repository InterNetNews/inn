/* Unit tests for the lib/tombstone helpers used by expire to consume
 * the deletion logs written by expireover/expirerm and innd/sm. */

#include "portable/system.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "inn/hashtab.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/storage.h"
#include "inn/tombstone.h"
#include "tap/basic.h"


static TOKEN
make_token(unsigned char type, unsigned char class, unsigned long n)
{
    TOKEN t;

    memset(&t, 0, sizeof(t));
    t.type = type;
    t.class = class;
    t.token[0] = (n >> 24) & 0xff;
    t.token[1] = (n >> 16) & 0xff;
    t.token[2] = (n >> 8) & 0xff;
    t.token[3] = n & 0xff;
    return t;
}


/* Write a tombstone-format file containing the given tokens and any
 * extra raw lines.  Returns the path; caller must unlink and free. */
static char *
write_tombstone_file(const char *prefix, const TOKEN *tokens, size_t ntokens,
                     const char *const *extra_lines, size_t nextra)
{
    char tmpl[64];
    int fd;
    FILE *f;
    size_t i;

    snprintf(tmpl, sizeof(tmpl), "%s-XXXXXX", prefix);
    fd = mkstemp(tmpl);
    if (fd < 0)
        sysbail("can't create %s", tmpl);
    f = fdopen(fd, "w");
    if (f == NULL)
        sysbail("can't fdopen %s", tmpl);
    for (i = 0; i < ntokens; i++)
        fprintf(f, "%s\n", TokenToText(tokens[i]));
    for (i = 0; i < nextra; i++)
        fprintf(f, "%s", extra_lines[i]);
    fclose(f);
    return xstrdup(tmpl);
}


int
main(void)
{
    TOKEN t1, t2, t3, t4, t_other;
    char *text;
    char *path;
    char *path2;
    struct hash *h;
    unsigned long count;
    TOKEN parsed;
    char *roundtrip;

    test_init(25);

    /* TokenToText / TextToToken / IsToken round trip is the foundation
     * of the file format. */
    t1 = make_token(3, 0, 0xdeadbeef);
    text = xstrdup(TokenToText(t1));
    parsed = TextToToken(text);
    ok(1, IsToken(text));
    ok(2, memcmp(&parsed, &t1, sizeof(TOKEN)) == 0);
    roundtrip = xstrdup(TokenToText(parsed));
    ok(3, strcmp(text, roundtrip) == 0);
    free(text);
    free(roundtrip);

    /* tombstone_hash_create gives us a hash that handles TOKEN keys. */
    t2 = make_token(3, 0, 1);
    t3 = make_token(3, 0, 2);
    t_other = make_token(3, 0, 99);
    h = tombstone_hash_create(4);
    {
        TOKEN *p = xmalloc(sizeof(TOKEN));
        *p = t2;
        ok(4, hash_insert(h, p, p));
    }
    {
        TOKEN *p = xmalloc(sizeof(TOKEN));
        *p = t3;
        ok(5, hash_insert(h, p, p));
    }
    ok(6, tombstone_present(h, &t2));
    ok(7, tombstone_present(h, &t3));
    ok(8, !tombstone_present(h, &t_other));
    ok(9, hash_count(h) == 2);
    hash_free(h);

    /* tombstone_read parses a single file with mixed valid/invalid
     * lines, skips malformed entries, and silently ignores comment
     * lines (used for the format version header and for future
     * metadata).  This locks in the writer/reader contract for
     * "# inn-tombstone v1\n" written by expireover. */
    {
        TOKEN tokens[] = {t1, t2, t3, t1}; /* duplicate t1 */
        const char *extras[] = {
            "\n",                       /* blank */
            "# inn-tombstone v1\n",     /* version header */
            "not-a-token\n",            /* malformed */
            "# arbitrary metadata\n",   /* future-format placeholder */
            "\r\n",                     /* CRLF blank */
        };
        path = write_tombstone_file("tombstone-read", tokens, 4, extras, 5);
        h = tombstone_hash_create(4);
        count = tombstone_read(h, path, NULL);
        /* Three unique tokens (t1 dedup'd) plus one valid duplicate
         * line counted but not inserted -> count returned is 4
         * (lines processed) but hash has 3 unique.  Comment and
         * blank lines are skipped silently and not counted. */
        ok(10, h != NULL);
        ok(11, count == 4);
        ok(12, hash_count(h) == 3);
        ok(13, tombstone_present(h, &t1));
        ok(14, tombstone_present(h, &t2));
        ok(15, tombstone_present(h, &t3));
        ok(16, !tombstone_present(h, &t_other));
        hash_free(h);
        unlink(path);
        free(path);
    }

    /* Two-file merge with overlap -- T-C4-B.  expireover.tombstone has
     * t1, t2; cancels.tombstone has t2, t3.  Loaded into one hashset,
     * we expect 3 unique entries with t2 merged. */
    t4 = make_token(3, 0, 4);
    {
        TOKEN expireover_tokens[] = {t1, t2};
        TOKEN cancels_tokens[] = {t2, t3, t4};
        path = write_tombstone_file("tombstone-expireover",
                                    expireover_tokens, 2, NULL, 0);
        path2 = write_tombstone_file("tombstone-cancels",
                                     cancels_tokens, 3, NULL, 0);
        h = tombstone_hash_create(4);
        tombstone_read(h, path, NULL);
        tombstone_read(h, path2, NULL);
        ok(17, hash_count(h) == 4); /* t1, t2, t3, t4 */
        ok(18, tombstone_present(h, &t1) && tombstone_present(h, &t2)
                   && tombstone_present(h, &t3)
                   && tombstone_present(h, &t4));
        hash_free(h);
        unlink(path);
        unlink(path2);
        free(path);
        free(path2);
    }

    /* tombstone_rename_for_processing -- T-C4-C.  Atomically renames
     * the source file to "<path>.processing"; subsequent open of the
     * original path finds nothing. */
    {
        TOKEN tokens[] = {t1, t2};
        char *snapshot;
        struct stat sb;

        path = write_tombstone_file("tombstone-rename", tokens, 2, NULL, 0);
        snapshot = tombstone_rename_for_processing(path);
        ok(19, snapshot != NULL);
        if (snapshot != NULL) {
            /* Original path no longer exists; snapshot does. */
            ok(20, stat(path, &sb) < 0 && errno == ENOENT
                       && stat(snapshot, &sb) == 0);
            unlink(snapshot);
            free(snapshot);
        } else {
            ok(20, false);
        }
        unlink(path); /* in case rename failed */
        free(path);
    }

    /* tombstone_ensure_header on an absent file: creates a header-only
     * file under exclusive lock.  Read back as a hashset that holds
     * no tokens (the header is a comment line, skipped by the
     * reader). */
    {
        char tmpl[] = "tombstone-ensure-XXXXXX";
        int tmp_fd;
        struct stat sb;

        tmp_fd = mkstemp(tmpl);
        if (tmp_fd < 0)
            sysbail("can't create temp file");
        close(tmp_fd);
        unlink(tmpl); /* mkstemp creates it; we want absent */
        path = xstrdup(tmpl);
        tombstone_ensure_header(path);
        h = tombstone_hash_create(4);
        count = tombstone_read(h, path, NULL);
        ok(21, stat(path, &sb) == 0 && sb.st_size > 0 && count == 0
                   && hash_count(h) == 0);
        hash_free(h);
        unlink(path);
        free(path);
    }

    /* tombstone_ensure_header on a non-empty file (appender raced a
     * cancel in between consumer's unlink and our recreate): the
     * existing tokens are preserved verbatim below the new header. */
    {
        TOKEN raced[] = {t1, t2};

        path = write_tombstone_file("tombstone-prepend", raced, 2, NULL, 0);
        tombstone_ensure_header(path);
        h = tombstone_hash_create(4);
        count = tombstone_read(h, path, NULL);
        ok(22, count == 2 && hash_count(h) == 2
                   && tombstone_present(h, &t1) && tombstone_present(h, &t2));
        hash_free(h);
        unlink(path);
        free(path);
    }

    /* tombstone_ensure_header is idempotent: called twice in a row,
     * the second call is a no-op because the file already starts
     * with the header.  Without this, every expire cycle would
     * prepend an extra header line and grow the file 19 bytes per
     * cycle on a site with zero cancels per cycle. */
    {
        char tmpl[] = "tombstone-idem-XXXXXX";
        int tmp_fd;
        struct stat sb1, sb2;

        tmp_fd = mkstemp(tmpl);
        if (tmp_fd < 0)
            sysbail("can't create temp file");
        close(tmp_fd);
        unlink(tmpl);
        path = xstrdup(tmpl);
        tombstone_ensure_header(path);
        stat(path, &sb1);
        tombstone_ensure_header(path);
        stat(path, &sb2);
        ok(23, sb1.st_size == sb2.st_size && sb1.st_size > 0);
        unlink(path);
        free(path);
    }

    /* Multi-chunk shift: build a file larger than the internal
     * chunk size (64 KiB), call tombstone_ensure_header, and verify
     * the shift loop's chunk-boundary arithmetic is correct.  At
     * ~38 bytes per token line, 3000 tokens is ~108 KiB, which
     * forces the loop to run at least twice (one full CHUNK + one
     * partial trailing chunk). */
    {
        size_t nbig = 3000;
        TOKEN *big = xmalloc(nbig * sizeof(TOKEN));
        size_t i;
        bool all_present;

        for (i = 0; i < nbig; i++)
            big[i] = make_token(3, 0, 0x10000 + i);
        path = write_tombstone_file("tombstone-multichunk", big, nbig,
                                    NULL, 0);
        tombstone_ensure_header(path);
        h = tombstone_hash_create(nbig * 2);
        count = tombstone_read(h, path, NULL);
        all_present = (count == nbig && hash_count(h) == nbig);
        for (i = 0; i < nbig && all_present; i++)
            all_present = tombstone_present(h, &big[i]);
        ok(24, all_present);
        hash_free(h);
        unlink(path);
        free(path);
        free(big);
    }

    /* Crash-recovery tolerance: simulate a file in a post-crash
     * state (no header, some garbled lines interleaved with valid
     * tokens).  ensure_header should shift everything right by
     * header_len without crashing; the subsequent tombstone_read
     * should warn-and-skip garbled lines and parse the valid
     * tokens.  This pins down the recovery argument as actual
     * behavior. */
    {
        TOKEN valid[] = {t1, t2, t3};
        const char *garbled[] = {
            "this-is-not-a-token\n",
            "neither-is-this\n",
        };

        path = write_tombstone_file("tombstone-corrupt", valid, 3,
                                    garbled, 2);
        tombstone_ensure_header(path);
        h = tombstone_hash_create(4);
        count = tombstone_read(h, path, NULL);
        ok(25, hash_count(h) == 3 && tombstone_present(h, &t1)
                   && tombstone_present(h, &t2)
                   && tombstone_present(h, &t3));
        hash_free(h);
        unlink(path);
        free(path);
    }

    return 0;
}
