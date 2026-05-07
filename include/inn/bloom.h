/*
**  Bloom filter for fast set membership testing.
**
**  A space-efficient probabilistic data structure that can test whether
**  an element is a member of a set.  False positive matches are possible,
**  but false negatives are not: a query returns either "possibly in set"
**  or "definitely not in set."
**
**  Uses enhanced double hashing (Kirsch & Mitzenmacher 2006) to derive
**  multiple hash positions from a single HASH value.
*/

#ifndef INN_BLOOM_H
#define INN_BLOOM_H

#include "inn/libinn.h"
#include "inn/portable-macros.h"
#include "inn/portable-stdbool.h"

#include <stddef.h>

BEGIN_DECLS

/* The layout of this struct is entirely internal to the implementation. */
struct bloom_filter;

/*
**  Create a new bloom filter sized for the given number of estimated entries
**  and false positive rate expressed as a reciprocal (e.g., 10000 means
**  1-in-10,000 or 0.01% false positive rate).  Uses xmalloc internally,
**  so dies on allocation failure.
*/
struct bloom_filter *bloom_create(size_t estimated_entries, unsigned long fp_inv);

/*
**  Add a HASH to the bloom filter.
*/
void bloom_add(struct bloom_filter *bf, const HASH *hash);

/*
**  Check whether a HASH is possibly in the bloom filter.  Returns true if
**  the element is probably in the set (with false positive rate as configured),
**  or false if the element is definitely not in the set.
*/
bool bloom_check(const struct bloom_filter *bf, const HASH *hash);

/*
**  Free a bloom filter and all associated memory.  Safe to call with NULL.
*/
void bloom_free(struct bloom_filter *bf);

/*
**  Return the number of entries that have been added to the bloom filter.
*/
size_t bloom_count(const struct bloom_filter *bf);

/*
**  Return the number of hash functions (k) used by the bloom filter.
*/
unsigned int bloom_nhash(const struct bloom_filter *bf);

/*
**  Return the total number of bits (m) in the bloom filter.
*/
size_t bloom_bits(const struct bloom_filter *bf);

END_DECLS

#endif /* INN_BLOOM_H */
