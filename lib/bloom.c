/*
**  Bloom filter implementation.
**
**  A space-efficient probabilistic data structure for set membership testing.
**  Uses enhanced double hashing (Kirsch & Mitzenmacher 2006) to derive k
**  independent bit positions from the two 64-bit halves of a 16-byte MD5
**  HASH value.
**
**  Sizing uses the standard formulas:
**    m = -n * ln(p) / (ln(2))^2    (optimal number of bits)
**    k = (m / n) * ln(2)           (optimal number of hash functions)
**  where n = estimated entries, p = desired false positive rate.
**
**  The create function takes the false positive rate as 1/fp_inv (an integer
**  reciprocal) to avoid floating point in the library.  Common values:
**    fp_inv = 100     =>  1% FP,   ~10 bits/entry, k=7
**    fp_inv = 1000    =>  0.1% FP, ~15 bits/entry, k=10
**    fp_inv = 10000   =>  0.01% FP, ~20 bits/entry, k=14
**    fp_inv = 100000  =>  0.001% FP, ~24 bits/entry, k=17
*/

#include "portable/system.h"

#include <string.h>

#include "inn/bloom.h"
#include "inn/messages.h"
#include "inn/xmalloc.h"

struct bloom_filter {
    uint8_t *bits;      /* bit array */
    size_t nbits;       /* total bits (m) */
    unsigned int nhash; /* number of hash functions (k) */
    size_t count;       /* entries added */
};


/*
**  Compute k bit positions for a given HASH using enhanced double hashing.
**  h(i) = (h1 + i * h2) mod m, where h1 and h2 are the two 64-bit halves
**  of the MD5 hash.  Unsigned overflow on (h1 + i * h2) is well-defined
**  in C and acts as additional mixing before the final mod m.
*/
static void
bloom_positions(const struct bloom_filter *bf, const HASH *hash,
                size_t *positions)
{
    uint64_t h1, h2;
    unsigned int i;

    memcpy(&h1, hash->hash, 8);
    memcpy(&h2, hash->hash + 8, 8);
    for (i = 0; i < bf->nhash; i++)
        positions[i] = (size_t) ((h1 + (uint64_t) i * h2) % bf->nbits);
}


/*
**  Pre-computed bits-per-entry and optimal k for common false positive rates.
**  Values are ceil(-ln(1/fp_inv) / ln(2)^2) and ceil(bits_per_entry * ln(2)).
**  For fp_inv values not in the table, we interpolate conservatively.
*/
static const struct {
    unsigned long fp_inv;
    unsigned int bits_per_entry;
    unsigned int nhash;
} bloom_params[] = {
    {       10,   5,  4 },   /* 10% FP */
    {       20,   7,  5 },
    {       50,   9,  6 },
    {      100,  10,  7 },   /*  1% FP */
    {      200,  12,  8 },
    {      500,  13,  9 },
    {     1000,  15, 10 },   /*  0.1% FP */
    {     2000,  16, 11 },
    {     5000,  18, 13 },
    {    10000,  20, 14 },   /*  0.01% FP */
    {    20000,  21, 15 },
    {    50000,  23, 16 },
    {   100000,  24, 17 },   /*  0.001% FP */
    {  1000000,  29, 20 },   /*  0.0001% FP */
    { 10000000,  34, 24 },
};
#define BLOOM_NPARAMS   (sizeof(bloom_params) / sizeof(bloom_params[0]))
#define BLOOM_MAX_NHASH 24 /* must match max nhash in bloom_params table */

/* Maximum bloom filter size on 32-bit platforms where size_t overflow
 * is a real concern.  On 64-bit, there is no cap, xmalloc will die if
 * the system doesn't have enough memory, which is the correct behavior
 * for a batch job.  On 32-bit, cap at SIZE_MAX/16 so that the conversion
 * to bits (multiply by 8) stays within size_t. */
#if SIZE_MAX <= UINT32_MAX
#    define BLOOM_MAX_BITS ((SIZE_MAX / 16) * 8)
#endif


struct bloom_filter *
bloom_create(size_t estimated_entries, unsigned long fp_inv)
{
    struct bloom_filter *bf;
    unsigned int bits_per_entry;
    unsigned int nhash;
    size_t nbits;
    size_t nbytes;
    size_t i;

    /* Look up parameters from the table.  Use the entry with the smallest
     * fp_inv that is >= the requested fp_inv (i.e., the FP rate at least as
     * good as requested).  If fp_inv exceeds all table entries, use the
     * last (most conservative) entry. */
    bits_per_entry = bloom_params[BLOOM_NPARAMS - 1].bits_per_entry;
    nhash = bloom_params[BLOOM_NPARAMS - 1].nhash;
    for (i = 0; i < BLOOM_NPARAMS; i++) {
        if (bloom_params[i].fp_inv >= fp_inv) {
            bits_per_entry = bloom_params[i].bits_per_entry;
            nhash = bloom_params[i].nhash;
            break;
        }
    }

    bf = xmalloc(sizeof(*bf));

    if (estimated_entries == 0)
        estimated_entries = 1;

    /* Guard against size_t overflow on 32-bit platforms where
     * bits_per_entry * estimated_entries can exceed SIZE_MAX.
     * On 64-bit this check is effectively unreachable but harmless.
     * On 32-bit, the cap degrades the FP rate but does not affect
     * correctness. */
    if (estimated_entries > SIZE_MAX / bits_per_entry)
#if SIZE_MAX <= UINT32_MAX
        nbits = BLOOM_MAX_BITS;
#else
        die("bloom filter: entry count too large for size_t");
#endif
    else
        nbits = (size_t) bits_per_entry * estimated_entries;
#if SIZE_MAX <= UINT32_MAX
    if (nbits > BLOOM_MAX_BITS)
        nbits = BLOOM_MAX_BITS;
#endif
    if (nbits < 64)
        nbits = 64;

    bf->nbits = nbits;
    bf->nhash = nhash;
    bf->count = 0;

    nbytes = (nbits + 7) / 8;
    bf->bits = xcalloc(nbytes, 1);

    return bf;
}


void
bloom_add(struct bloom_filter *bf, const HASH *hash)
{
    size_t positions[BLOOM_MAX_NHASH];
    unsigned int i;

    bloom_positions(bf, hash, positions);
    for (i = 0; i < bf->nhash; i++)
        bf->bits[positions[i] / 8] |= (uint8_t) (1U << (positions[i] % 8));
    bf->count++;
}


bool
bloom_check(const struct bloom_filter *bf, const HASH *hash)
{
    size_t positions[BLOOM_MAX_NHASH];
    unsigned int i;

    bloom_positions(bf, hash, positions);
    for (i = 0; i < bf->nhash; i++) {
        if (!(bf->bits[positions[i] / 8] & (1U << (positions[i] % 8))))
            return false;
    }
    return true;
}


void
bloom_free(struct bloom_filter *bf)
{
    if (bf == NULL)
        return;
    free(bf->bits);
    free(bf);
}


size_t
bloom_count(const struct bloom_filter *bf)
{
    return bf->count;
}


unsigned int
bloom_nhash(const struct bloom_filter *bf)
{
    return bf->nhash;
}


size_t
bloom_bits(const struct bloom_filter *bf)
{
    return bf->nbits;
}
