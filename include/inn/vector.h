/*  $Id$
**
**  Vector handling (counted lists of char *'s).
**
**  Written by Russ Allbery <rra@stanford.edu>
**  This work is hereby placed in the public domain by its author.
**
**  A vector is a simple array of char *'s combined with a count.  It's a
**  convenient way of managing a list of strings, as well as a reasonable
**  output data structure for functions that split up a string.  There are
**  two basic types of vectors, regular vectors (in which case strings are
**  copied when put into a vector and freed when the vector is freed) and
**  cvectors or const vectors (where each pointer is a const char * to some
**  external string that isn't freed when the vector is freed).
**
**  There are two interfaces here, one for vectors and one for cvectors,
**  with the basic operations being the same between the two.
*/

#ifndef INN_VECTOR_H
#define INN_VECTOR_H 1

#include <inn/defines.h>

struct vector {
    size_t count;
    size_t allocated;
    char **strings;
};

struct cvector {
    size_t count;
    size_t allocated;
    const char **strings;
};

BEGIN_DECLS

/* Create a new, empty vector. */
struct vector *vector_new(void);
struct cvector *cvector_new(void);

/* Add a string to a vector.  Resizes the vector if necessary. */
void vector_add(struct vector *, const char *string);
void cvector_add(struct cvector *, const char *string);

/* Resize the array of strings to hold size entries.  Saves reallocation work
   in vector_add if it's known in advance how many entries there will be. */
void vector_resize(struct vector *, size_t size);
void cvector_resize(struct cvector *, size_t size);

/* Reset the number of elements to zero, freeing all of the strings for a
   regular vector, but not freeing the strings array (to cut down on memory
   allocations if the vector will be reused). */
void vector_clear(struct vector *);
void cvector_clear(struct cvector *);

/* Free the vector and all resources allocated for it. */
void vector_free(struct vector *);
void cvector_free(struct cvector *);

/* Split functions build a vector from a string.  vector_split splits on a
   specified character, while vector_split_space splits on any sequence of
   spaces or tabs (not any sequence of whitespace, as just spaces or tabs is
   more useful for INN).  The cvector versions destructively modify the
   provided string in-place to insert nul characters between the strings.  If
   the vector argument is NULL, a new vector is allocated; otherwise, the
   provided one is reused.

   Empty strings will yield zero-length vectors.  Adjacent delimiters are
   treated as a single delimiter by *_split_space, but *not* by *_split, so
   callers of *_split should be prepared for zero-length strings in the
   vector. */
struct vector *vector_split(const char *string, char sep, struct vector *);
struct vector *vector_split_space(const char *string, struct vector *);
struct cvector *cvector_split(char *string, char sep, struct cvector *);
struct cvector *cvector_split_space(char *string, struct cvector *);

/* Build a string from a vector by joining its components together with the
   specified string as separator.  Returns a newly allocated string; caller is
   responsible for freeing. */
char *vector_join(const struct vector *, const char *seperator);
char *cvector_join(const struct cvector *, const char *separator);

END_DECLS

#endif /* INN_VECTOR_H */
