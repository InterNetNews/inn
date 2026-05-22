/* Test suite for lib/bloom.c.
 *
 * Written by Kevin Bowling in 2026.
 */

#include "portable/system.h"

#include <string.h>

#include "inn/bloom.h"
#include "inn/libinn.h"
#include "tap/basic.h"


/*
**  Generate a deterministic HASH from an integer for testing.
*/
static HASH
make_hash(unsigned long n)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "<test-%lu@bloom.test>", n);
    return HashMessageID(buf);
}


int
main(void)
{
    struct bloom_filter *bf;
    HASH h1, h2, h3;
    unsigned long i;
    unsigned long false_positives;
    unsigned long n_check;

    test_init(20);

    /* Basic creation. */
    bf = bloom_create(1000, 10000);
    ok(1, bf != NULL);
    ok(2, bloom_bits(bf) >= 1000 * 20); /* 0.01% FP needs ~20 bits/entry */
    ok(3, bloom_nhash(bf) == 14);
    ok(4, bloom_count(bf) == 0);

    /* Add and check: true positives. */
    h1 = make_hash(1);
    h2 = make_hash(2);
    h3 = make_hash(3);
    bloom_add(bf, &h1);
    bloom_add(bf, &h2);
    ok(5, bloom_count(bf) == 2);
    ok(6, bloom_check(bf, &h1));
    ok(7, bloom_check(bf, &h2));

    /* True negative. */
    ok(8, !bloom_check(bf, &h3));

    bloom_free(bf);

    /* Larger test: verify false positive rate.
     * Add 10,000 items, then check 100,000 items that were NOT added.
     * At 0.01% target FP rate, we expect ~10 false positives out of
     * 100,000 checks.  Allow up to 50 (0.05%) to account for variance. */
    bf = bloom_create(10000, 10000);
    ok(9, bf != NULL);

    for (i = 0; i < 10000; i++) {
        HASH h = make_hash(i);
        bloom_add(bf, &h);
    }
    ok(10, bloom_count(bf) == 10000);

    /* Verify all added items are found (no false negatives). */
    for (i = 0; i < 10000; i++) {
        HASH h = make_hash(i);
        if (!bloom_check(bf, &h)) {
            ok(11, false);
            diag("false negative at i=%lu", i);
            goto fp_test;
        }
    }
    ok(11, true);

fp_test:
    /* Count false positives from items never added. */
    false_positives = 0;
    n_check = 100000;
    for (i = 10000; i < 10000 + n_check; i++) {
        HASH h = make_hash(i);
        if (bloom_check(bf, &h))
            false_positives++;
    }
    ok(12, false_positives <= 50);
    if (false_positives > 50)
        diag("false positives: %lu out of %lu (expected <= 50)",
             false_positives, n_check);
    else
        diag("false positives: %lu out of %lu (%.4f%%)", false_positives,
             n_check, 100.0 * (double) false_positives / (double) n_check);

    bloom_free(bf);

    /* Test with different FP rate parameters. */
    bf = bloom_create(1000, 100); /* 1% FP rate */
    ok(13, bf != NULL);
    ok(14, bloom_nhash(bf) == 7); /* k=7 for 1% FP */
    bloom_free(bf);

    bf = bloom_create(1000, 1000); /* 0.1% FP rate */
    ok(15, bf != NULL);
    ok(16, bloom_nhash(bf) == 10); /* k=10 for 0.1% FP */
    bloom_free(bf);

    /* Edge case: very small filter. */
    bf = bloom_create(1, 10000);
    ok(17, bf != NULL);
    ok(18, bloom_bits(bf) >= 64); /* minimum size */
    bloom_free(bf);

    /* Test with max nhash (fp_inv >= 10000000, nhash=24).
     * Exercises the full positions array to catch overflow. */
    bf = bloom_create(100, 10000000);
    ok(19, bf != NULL);
    h1 = make_hash(42);
    bloom_add(bf, &h1);
    ok(20, bloom_check(bf, &h1));
    bloom_free(bf);

    return 0;
}
