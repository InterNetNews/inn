/*  $Id$
**
**  Vector handling (counted lists of char *'s).
**
**  Written by Russ Allbery <rra@stanford.edu>
**  This work is hereby placed in the public domain by its author.
**
**  A vector is a simple array of char *'s combined with a count.  It's a
**  convenient way of managing a list of strings, as well as a reasonable
**  output data structure for functions that split up a string.
**
**  Vectors can be "deep," in which case each char * points to allocated
**  memory that should be freed when the vector is freed, or "shallow," in
**  which case the char *'s are taken to be pointers into some other string
**  that shouldn't be freed.
*/

#ifndef INN_VECTOR_H
#define INN_VECTOR_H 1

#include <inn/defines.h>

struct vector {
    size_t count;
    size_t allocated;
    char **strings;
    bool shallow;
};

BEGIN_DECLS

/* Create a new, empty vector. */
struct vector *vector_new(bool shallow);

/* Add a string to a vector.  If vector->shallow is false, the string will be
   copied; otherwise, the pointer is just stashed.  Resizes the vector if
   necessary. */
void vector_add(struct vector *, char *string);

/* Resize the array of strings to hold size entries.  Saves reallocation work
   in vector_add if it's known in advance how many entries there will be. */
void vector_resize(struct vector *, size_t size);

/* Reset the number of elements to zero, freeing all of the strings if the
   vector isn't shallow, but not freeing the strings array (to cut down on
   memory allocations if the vector will be reused). */
void vector_clear(struct vector *);

/* Free the vector and all resources allocated for it. */
void vector_free(struct vector *);

/* Split functions build a vector from a string.  vector_split splits on a
   specified character, while vector_split_whitespace splits on any sequence
   of whitespace.  If copy is true, a deep vector will be constructed;
   otherwise, the provided string will be destructively  modified in-place to
   insert nul characters between the strings.  If the vector argument is NULL,
   a new vector is allocated; otherwise, the provided one is reused.

   Empty strings will yield zero-length vectors.  Adjacent delimiters are
   treated as a single delimiter (zero-length strings are not added to the
   vector). */
struct vector *vector_split(char *, char sep, bool copy, struct vector *);
struct vector *vector_split_whitespace(char *, bool copy, struct vector *);

/* Build a string from a vector by joining its components together with the
   specified string as separator.  Returns a newly allocated string; caller is
   responsible for freeing. */
char *vector_join(struct vector *, const char *seperator);

END_DECLS

#endif /* INN_VECTOR_H */
