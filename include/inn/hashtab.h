/*  $Id$
**
**  Generic hash table interface.
**
**  Written by Russ Allbery <eagle@eyrie.org>
**  This work is hereby placed in the public domain by its author.
**
**  A hash table takes a hash function that acts on keys, a function to
**  extract the key from a data item stored in a hash, a function that takes
**  a key and a data item and returns true if the key matches, and a
**  function to be called on any data item being deleted from the hash.
**
**  hash_create creates a hash and hash_free frees all the space allocated
**  by one.  hash_insert, hash_replace, and hash_delete modify it, and
**  hash_lookup extracts values.  hash_traverse can be used to walk the
**  hash, and hash_count returns the number of elements currently stored in
**  the hash.  hash_searches, hash_collisions, and hash_expansions extract
**  performance and debugging statistics.
*/

#ifndef INN_HASHTAB_H
#define INN_HASHTAB_H 1

#include <inn/defines.h>

BEGIN_DECLS

/* The layout of this struct is entirely internal to the implementation. */
struct hash;

/* Data types for function pointers used by the hash table interface. */
typedef unsigned long (*hash_func)(const void *);
typedef const void * (*hash_key_func)(const void *);
typedef bool (*hash_equal_func)(const void *, const void *);
typedef void (*hash_delete_func)(void *);
typedef void (*hash_traverse_func)(void *, void *);

/* Generic hash table interface. */
struct hash *   hash_create(size_t, hash_func, hash_key_func,
                            hash_equal_func, hash_delete_func);
void            hash_free(struct hash *);
void *          hash_lookup(struct hash *, const void *key);
bool            hash_insert(struct hash *, const void *key, void *datum);
bool            hash_replace(struct hash *, const void *key, void *datum);
bool            hash_delete(struct hash *, const void *key);
void            hash_traverse(struct hash *, hash_traverse_func, void *);
unsigned long   hash_count(struct hash *);
unsigned long   hash_searches(struct hash *);
unsigned long   hash_collisions(struct hash *);
unsigned long   hash_expansions(struct hash *);

/* Hash functions available for callers. */
unsigned long   hash_string(const void *);

/* Functions useful for constructing new hashes. */
unsigned long   hash_lookup2(const char *, size_t, unsigned long partial);

END_DECLS

#endif /* INN_HASHTAB_H */
