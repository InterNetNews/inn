/*  $Id$
**
**  Generic hash table implementation.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  This work is hereby placed in the public domain by its author.
**
**  This is a generic hash table implementation with linear probing.  It
**  takes a comparison function and a hashing function and stores void *.
**
**  Included for the use of callers is the hash function LOOKUP2 by Bob
**  Jenkins, taken from <http://burtleburtle.net/bob/hash/>; see that web
**  page for analysis and performance comparisons.  The performance of this
**  hash is slightly worse than the standard sum and modulus hash function
**  seen in many places but it produces fewer collisions.
*/

#include "config.h"
#include "clibrary.h"
#include "inn/hashtab.h"
#include "libinn.h"
#include "macros.h"

/* Magic values for empty and deleted hash table slots. */
#define HASH_EMPTY      ((void *) 0)
#define HASH_DELETED    ((void *) 1)

struct hash {
    size_t size;                /* Allocated size. */
    size_t mask;                /* Used to resolve a hash to an index. */
    size_t nelements;           /* Total elements, including deleted. */
    size_t ndeleted;            /* Number of deleted elements. */

    unsigned long searches;     /* Count of lookups (for debugging). */
    unsigned long collisions;   /* Count of collisions (for debugging). */
    unsigned long expansions;   /* Count of hash resizes needed. */

    hash_func hash;             /* Return hash of a key. */
    hash_key_func key;          /* Given an element, returns its key. */
    hash_equal_func equal;      /* Whether a key matches an element. */
    hash_delete_func delete;    /* Called when a hash element is deleted. */

    void **table;               /* The actual elements. */
};


/*
**  Given a target table size, return the nearest power of two that's
**  greater than or equal to that size, with a minimum size of four.  The
**  minimum must be at least four to ensure that there is always at least
**  one empty slot in the table given hash_find_slot's resizing of the table
**  if it as least 75% full.  Otherwise, it would be possible for
**  hash_find_slot to go into an infinite loop.
*/
static size_t
hash_size(size_t target)
{
    int n;
    size_t size;

    size = target - 1;
    for (n = 0; size > 0; n++)
        size >>= 1;
    size = 1 << n;
    return (size < 4) ? 4 : size;
}


/*
**  Create a new hash table.  The given size is rounded up to the nearest
**  power of two for speed reasons (it greatly simplifies the use of the
**  hash function).
*/
struct hash *
hash_create(size_t size, hash_func hash_f, hash_key_func key_f,
            hash_equal_func equal_f, hash_delete_func delete_f)
{
    struct hash *hash;

    hash = xcalloc(1, sizeof(struct hash));
    hash->hash = hash_f;
    hash->key = key_f;
    hash->equal = equal_f;
    hash->delete = delete_f;
    hash->size = hash_size(size);
    hash->mask = hash->size - 1;
    hash->table = xcalloc(hash->size, sizeof(void *));
    return hash;
}


/*
**  Free a hash and all resources used by it, and call the delete function
**  on every element.
*/
void
hash_free(struct hash *hash)
{
    size_t i;
    void *entry;

    for (i = 0; i < hash->size; i++) {
        entry = hash->table[i];
        if (entry != HASH_EMPTY && entry != HASH_DELETED)
            (*hash->delete)(entry);
    }
    free(hash->table);
    free(hash);
}


/*
**  Internal search function used by hash_expand.  This is an optimized
**  version of hash_find_slot that returns a pointer to the first empty
**  slot, not trying to call the equality function on non-empty slots and
**  assuming there are no HASH_DELETED slots.
*/
static void **
hash_find_empty(struct hash *hash, const void *key)
{
    size_t slot;

    slot = (*hash->hash)(key) & hash->mask;
    while (1) {
        if (hash->table[slot] == HASH_EMPTY)
            return &hash->table[slot];

        slot++;
        if (slot >= hash->size)
            slot -= hash->size;
    }
}


/*
**  Expand the hash table to be approximately 50% empty based on the number
**  of elements in the hash.  This is done by allocating a new table and
**  then calling hash_find_empty for each element in the previous table,
**  recovering the key by calling hash->key on the element.
*/
static void
hash_expand(struct hash *hash)
{
    void **old, **slot;
    size_t i, size;

    old = hash->table;
    size = hash->size;
    hash->size = hash_size((hash->nelements - hash->ndeleted) * 2);
    hash->mask = hash->size - 1;
    hash->table = xcalloc(hash->size, sizeof(void *));

    hash->nelements = 0;
    hash->ndeleted = 0;
    for (i = 0; i < size; i++)
        if (old[i] != HASH_EMPTY && old[i] != HASH_DELETED) {
            slot = hash_find_empty(hash, (*hash->key)(old[i]));
            *slot = old[i];
            hash->nelements++;
        }

    hash->expansions++;
    free(old);
}


/*
**  Find a slot in the hash for a given key.  This is used both for
**  inserting and deleting elements from the hash, as well as looking up
**  entries.  Returns a pointer to the slot.  If insert is true, return the
**  first empty or deleted slot.  If insert is false, return NULL if the
**  element could not be found.
**
**  This function assumes that there is at least one empty slot in the
**  hash; otherwise, it can loop infinitely.  It attempts to ensure this by
**  always expanding the hash if it is at least 75% full; this will ensure
**  that property for any hash size of 4 or higher.
*/
static void **
hash_find_slot(struct hash *hash, const void *key, bool insert)
{
    void **deleted_slot = NULL;
    void *entry;
    size_t slot;

    if (insert && hash->nelements * 4 >= hash->size * 3)
        hash_expand(hash);

    hash->searches++;

    slot = (*hash->hash)(key) & hash->mask;
    while (1) {
        entry = hash->table[slot];
        if (entry == HASH_EMPTY) {
            if (!insert)
                return NULL;

            if (deleted_slot != NULL) {
                *deleted_slot = HASH_EMPTY;
                hash->ndeleted--;
                return deleted_slot;
            }
            hash->nelements++;
            return &hash->table[slot];
        } else if (entry == HASH_DELETED) {
            if (insert)
                deleted_slot = &hash->table[slot];
        } else if ((*hash->equal)(key, entry)) {
            return &hash->table[slot];
        }

        hash->collisions++;
        slot++;
        if (slot >= hash->size)
            slot -= hash->size;
    }
}


/*
**  Given a key, return the entry corresponding to that key or NULL if that
**  key isn't present in the hash table.
*/
void *
hash_lookup(struct hash *hash, const void *key)
{
    void **slot;

    slot = hash_find_slot(hash, key, false);
    return (slot == NULL) ? NULL : *slot;
}


/*
**  Insert a new key/value pair into the hash, returning true if the
**  insertion was successful and false if there is already a value in the
**  hash with that key.
*/
bool
hash_insert(struct hash *hash, const void *key, void *datum)
{
    void **slot;

    slot = hash_find_slot(hash, key, true);
    if (*slot != HASH_EMPTY)
        return false;
    *slot = datum;
    return true;
}


/*
**  Replace an existing hash value with a new data value, calling the delete
**  function for the old data.  Returns true if the replacement was
**  successful or false (without changing the hash) if the key whose value
**  should be replaced was not found in the hash.
*/
bool
hash_replace(struct hash *hash, const void *key, void *datum)
{
    void **slot;

    slot = hash_find_slot(hash, key, false);
    if (slot == NULL)
        return false;
    (*hash->delete)(*slot);
    *slot = datum;
    return true;
}


/*
**  Delete a key out of the hash.  Returns true if the deletion was
**  successful, false if the key could not be found in the hash.
*/
bool
hash_delete(struct hash *hash, const void *key)
{
    bool result;

    result = hash_replace(hash, key, HASH_DELETED);
    if (result)
        hash->ndeleted++;
    return result;
}


/*
**  For each element in the hash table, call the provided function, passing
**  it the element and the opaque token that's passed to this function.
*/
void
hash_traverse(struct hash *hash, hash_traverse_func callback, void *data)
{
    size_t i;
    void *entry;

    for (i = 0; i < hash->size; i++) {
        entry = hash->table[i];
        if (entry != HASH_EMPTY && entry != HASH_DELETED)
            (*callback)(entry, data);
    }
}


/*
**  Returns a count of undeleted elements in the hash.
*/
unsigned long
hash_count(struct hash *hash)
{
    return hash->nelements - hash->ndeleted;
}


/*
**  Accessor functions for the debugging statistics.
*/
unsigned long
hash_searches(struct hash *hash)
{
    return hash->searches;
}

unsigned long
hash_collisions(struct hash *hash)
{
    return hash->collisions;
}

unsigned long
hash_expansions(struct hash *hash)
{
    return hash->expansions;
}


/*
**  Mix three 32-bit values reversibly.  This is the internal mixing
**  function for the hash function.
**
**  For every delta with one or two bit set, and the deltas of all three
**  high bits or all three low bits, whether the original value of a,b,c
**  is almost all zero or is uniformly distributed,
**
**   * If mix() is run forward or backward, at least 32 bits in a,b,c
**     have at least 1/4 probability of changing.
**
**   * If mix() is run forward, every bit of c will change between 1/3 and
**     2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
**
**  mix() takes 36 machine instructions, but only 18 cycles on a superscalar
**  machine (like a Pentium or a Sparc).  No faster mixer seems to work,
**  that's the result of my brute-force search.  There were about 2^68
**  hashes to choose from.  I (Bob Jenkins) only tested about a billion of
**  those.
*/
#define MIX(a, b, c)                                    \
    {                                                   \
        (a) -= (b); (a) -= (c); (a) ^= ((c) >> 13);     \
        (b) -= (c); (b) -= (a); (b) ^= ((a) << 8);      \
        (c) -= (a); (c) -= (b); (c) ^= ((b) >> 13);     \
        (a) -= (b); (a) -= (c); (a) ^= ((c) >> 12);     \
        (b) -= (c); (b) -= (a); (b) ^= ((a) << 16);     \
        (c) -= (a); (c) -= (b); (c) ^= ((b) >> 5);      \
        (a) -= (b); (a) -= (c); (a) ^= ((c) >> 3);      \
        (b) -= (c); (b) -= (a); (b) ^= ((a) << 10);     \
        (c) -= (a); (c) -= (b); (c) ^= ((b) >> 15);     \
    }


/*
**  Hash a variable-length key into a 32-bit value.
**
**  Takes byte sequence to hash and returns a 32-bit value.  A partial
**  result can be passed as the third parameter so that large amounts of
**  data can be hashed by subsequent calls, passing in the result of the
**  previous call each time.  Every bit of the key affects every bit of the
**  return value.  Every 1-bit and 2-bit delta achieves avalanche.  About
**  (36 + 6n) instructions.
**
**  The best hash table sizes are powers of 2.  There is no need to mod with
**  a prime (mod is sooo slow!).  If you need less than 32 bits, use a
**  bitmask.  For example, if you need only 10 bits, do:
**
**      h = h & ((1 << 10) - 1);
**
**  In which case, the hash table should have 2^10 elements.
**
**  Based on code by Bob Jenkins <bob_jenkins@burtleburtle.net>, originally
**  written in 1996.  The original license was:
**
**      By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use
**      this code any way you wish, private, educational, or commercial.
**      It's free.
**
**  See <http://burlteburtle.net/bob/hash/evahash.html> for discussion of
**  this hash function.  Use for hash table lookup, or anything where one
**  collision in 2^32 is acceptable.  Do NOT use for cryptographic purposes.
*/
unsigned long
hash_lookup2(const char *key, size_t length, unsigned long partial)
{
    uint32_t a, b, c, len;

    /* Set up the internal state.  a and b are initialized to a golden
       ratio, an arbitrary value intended to avoid mapping all zeroes to all
       zeroes. */
    len = length;
    a = b = 0x9e3779b9;
    c = partial;

#define S0(c)   ((uint32_t)(c))
#define S1(c)   ((uint32_t)(c) << 8)
#define S2(c)   ((uint32_t)(c) << 16)
#define S3(c)   ((uint32_t)(c) << 24)

    /* Handle most of the key. */
    while (len >= 12) {
        a += S0(key[0]) + S1(key[1]) + S2(key[2])  + S3(key[3]);
        b += S0(key[4]) + S1(key[5]) + S2(key[6])  + S3(key[7]);
        c += S0(key[8]) + S1(key[9]) + S2(key[10]) + S3(key[11]);
        MIX(a, b, c);
        key += 12;
        len -= 12;
   }

    /* Handle the last 11 bytes.  All of the cases fall through. */
    c += length;
    switch (len) {
    case 11: c += S3(key[10]);
    case 10: c += S2(key[9]);
    case  9: c += S1(key[8]);
        /* The first byte of c is reserved for the length. */
    case  8: b += S3(key[7]);
    case  7: b += S2(key[6]);
    case  6: b += S1(key[5]);
    case  5: b += S0(key[4]);
    case  4: a += S3(key[3]);
    case  3: a += S2(key[2]);
    case  2: a += S1(key[1]);
    case  1: a += S0(key[0]);
        /* case 0: nothing left to add. */
    }
    MIX(a, b, c);
    return c;
}


/*
**  A hash function for nul-terminated strings using hash_lookup2, suitable
**  for passing to hash_create.
*/
unsigned long
hash_string(const void *key)
{
    return hash_lookup2(key, strlen(key), 0);
}
