/* $Id$
 *
 * Vector handling (counted lists of char *'s).
 *
 * A vector is a table for handling a list of strings with less overhead than
 * linked list.  The intention is for vectors, once allocated, to be reused;
 * this saves on memory allocations once the array of char *'s reaches a
 * stable size.
 *
 * There are two types of vectors.  Standard vectors copy strings when they're
 * inserted into the vector, whereas cvectors just accept pointers to external
 * strings to store.  There are therefore two entry points for every vector
 * function, one for vectors and one for cvectors.
 *
 * Vectors require list of strings, not arbitrary binary data, and cannot
 * handle data elements containing nul characters.
 *
 * There's a whole bunch of code duplication here.  This would be a lot
 * cleaner with C++ features (either inheritance or templates would probably
 * help).  One could probably in some places just cast a cvector to a vector
 * and perform the same operations, but I'm leery of doing that as I'm not
 * sure if it's a violation of the C type aliasing rules.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 *
 * The authors hereby relinquish any claim to any copyright that they may have
 * in this work, whether granted under contract or by operation of law or
 * international treaty, and hereby commit to the public, at large, that they
 * shall not, at any time in the future, seek to enforce any copyright in this
 * work against any person or entity, or prevent any person or entity from
 * copying, publishing, distributing or creating derivative works of this
 * work.
 */

#include "config.h"
#include "clibrary.h"

#include <assert.h>

#include "inn/vector.h"
#include "inn/xmalloc.h"


/*
 * Allocate a new, empty vector.
 */
struct vector *
vector_new(void)
{
    struct vector *vector;

    vector = xcalloc(1, sizeof(struct vector));
    vector->allocated = 1;
    vector->strings = xcalloc(1, sizeof(char *));
    return vector;
}

struct cvector *
cvector_new(void)
{
    struct cvector *vector;

    vector = xcalloc(1, sizeof(struct cvector));
    vector->allocated = 1;
    vector->strings = xcalloc(1, sizeof(const char *));
    return vector;
}


/*
 * Resize a vector (using reallocarray to resize the table).  Maintain a
 * minimum allocated size of 1 so that the strings data element is never NULL.
 * This simplifies other code.
 */
void
vector_resize(struct vector *vector, size_t size)
{
    size_t i;

    assert(vector != NULL);
    if (vector->count > size) {
        for (i = size; i < vector->count; i++)
            free(vector->strings[i]);
        vector->count = size;
    }
    if (size == 0)
        size = 1;
    vector->strings = xreallocarray(vector->strings, size, sizeof(char *));
    vector->allocated = size;
}

void
cvector_resize(struct cvector *vector, size_t size)
{
    assert(vector != NULL);
    if (vector->count > size)
        vector->count = size;
    if (size == 0)
        size = 1;
    vector->strings
        = xreallocarray(vector->strings, size, sizeof(const char *));
    vector->allocated = size;
}


/*
 * Add a new string to the vector, resizing the vector as necessary.  The
 * vector is resized an element at a time; if a lot of resizes are expected,
 * vector_resize should be called explicitly with a more suitable size.
 */
void
vector_add(struct vector *vector, const char *string)
{
    size_t next = vector->count;

    assert(vector != NULL);
    if (vector->count == vector->allocated)
        vector_resize(vector, vector->allocated + 1);
    vector->strings[next] = xstrdup(string);
    vector->count++;
}

void
cvector_add(struct cvector *vector, const char *string)
{
    size_t next = vector->count;

    assert(vector != NULL);
    if (vector->count == vector->allocated)
        cvector_resize(vector, vector->allocated + 1);
    vector->strings[next] = string;
    vector->count++;
}


/*
 * Add a new string to the vector, copying at most length characters of the
 * string, resizing the vector as necessary the same as with vector_add.  This
 * function is only available for vectors, not cvectors, since it requires the
 * duplication of the input string to be sure it's nul-terminated.
 */
void
vector_addn(struct vector *vector, const char *string, size_t length)
{
    size_t next = vector->count;

    assert(vector != NULL);
    if (vector->count == vector->allocated)
        vector_resize(vector, vector->allocated + 1);
    vector->strings[next] = xstrndup(string, length);
    vector->count++;
}


/*
 * Empty a vector but keep the allocated memory for the pointer table.
 */
void
vector_clear(struct vector *vector)
{
    size_t i;

    assert(vector != NULL);
    for (i = 0; i < vector->count; i++)
        free(vector->strings[i]);
    vector->count = 0;
}

void
cvector_clear(struct cvector *vector)
{
    assert(vector != NULL);
    vector->count = 0;
}


/*
 * Free a vector completely.
 */
void
vector_free(struct vector *vector)
{
    if (vector == NULL)
        return;
    vector_clear(vector);
    free(vector->strings);
    free(vector);
}

void
cvector_free(struct cvector *vector)
{
    if (vector == NULL)
        return;
    cvector_clear(vector);
    free(vector->strings);
    free(vector);
}


/*
 * Given a vector that we may be reusing, clear it out.  If the argument is
 * NULL, allocate a new vector.  Helper function for vector_split*.
 */
static struct vector *
vector_reuse(struct vector *vector)
{
    if (vector == NULL)
        return vector_new();
    else {
        vector_clear(vector);
        return vector;
    }
}

static struct cvector *
cvector_reuse(struct cvector *vector)
{
    if (vector == NULL)
        return cvector_new();
    else {
        cvector_clear(vector);
        return vector;
    }
}


/*
 * Given a string and a separator character, count the number of strings that
 * it will split into.
 */
static size_t
split_count(const char *string, char separator)
{
    const char *p;
    size_t count;

    if (*string == '\0')
        return 1;
    for (count = 1, p = string; *p != '\0'; p++)
        if (*p == separator)
            count++;
    return count;
}


/*
 * Given a string and a separator character, form a vector by splitting the
 * string at occurrences of that separator.  Consecutive occurrences of the
 * character will result in empty strings added to the vector.  Reuse the
 * provided vector if non-NULL.
 */
struct vector *
vector_split(const char *string, char separator, struct vector *vector)
{
    const char *p, *start;
    size_t i, count;

    /* If the vector argument isn't NULL, reuse it. */
    vector = vector_reuse(vector);

    /* Do a first pass to size the vector. */
    count = split_count(string, separator);
    if (vector->allocated < count)
        vector_resize(vector, count);

    /* Walk the string and create the new strings with xstrndup. */
    for (start = string, p = string, i = 0; *p != '\0'; p++)
        if (*p == separator) {
            vector->strings[i++] = xstrndup(start, p - start);
            start = p + 1;
        }
    vector->strings[i++] = xstrndup(start, p - start);
    vector->count = i;
    return vector;
}


/*
 * Given a modifiable string and a separator character, form a cvector by
 * modifying the string in-place to add nuls at the separators and then
 * building a vector of pointers into the string.  Reuse the provided vector
 * if non-NULL.
 */
struct cvector *
cvector_split(char *string, char separator, struct cvector *vector)
{
    char *p, *start;
    size_t i, count;

    /* If the vector argument isn't NULL, reuse it. */
    vector = cvector_reuse(vector);

    /* Do a first pass to size the vector. */
    count = split_count(string, separator);
    if (vector->allocated < count)
        cvector_resize(vector, count);

    /*
     * Walk the string and replace separators with nul, and store the pointers
     * to the start of each string segment.
     */
    for (start = string, p = string, i = 0; *p; p++)
        if (*p == separator) {
            *p = '\0';
            vector->strings[i++] = start;
            start = p + 1;
        }
    vector->strings[i++] = start;
    vector->count = i;
    return vector;
}


/*
 * Given a string and a set of separators expressed as a string, count the
 * number of strings that it will split into when splitting on those
 * separators.  Unlike with split_count, multiple consecutive separator
 * characters will be treated the same as a single separator.
 */
static size_t
split_multi_count(const char *string, const char *seps)
{
    const char *p;
    size_t count;

    /* The empty string produces no substrings. */
    if (*string == '\0')
        return 0;

    /*
     * Walk the string looking for the first separator not preceeded by
     * another separator (and ignore a separator at the start of the string).
     */
    for (count = 1, p = string + 1; *p != '\0'; p++)
        if (strchr(seps, *p) != NULL && strchr(seps, p[-1]) == NULL)
            count++;

    /*
     * If the string ends in separators, we've overestimated the number of
     * strings by one.
     */
    if (strchr(seps, p[-1]) != NULL)
        count--;
    return count;
}


/*
 * Given a string, split it at any of the provided separators to form a
 * vector, copying each string segment.  Any number of consecutive separators
 * are considered a single separator.  Reuse the provided vector if non-NULL.
 */
struct vector *
vector_split_multi(const char *string, const char *seps,
                   struct vector *vector)
{
    const char *p, *start;
    size_t i, count;

    /* If the vector argument isn't NULL, reuse it. */
    vector = vector_reuse(vector);

    /* Count the number of strings we'll create and size the vector. */
    count = split_multi_count(string, seps);
    if (vector->allocated < count)
        vector_resize(vector, count);

    /*
     * Walk the string and look for separators.  start tracks the
     * non-separator that starts a new string, so as long as start == p, we're
     * tracking a sequence of separators.
     */
    for (start = string, p = string, i = 0; *p != '\0'; p++)
        if (strchr(seps, *p) != NULL) {
            if (start != p)
                vector->strings[i++] = xstrndup(start, p - start);
            start = p + 1;
        }
    if (start != p)
        vector->strings[i++] = xstrndup(start, p - start);
    vector->count = i;
    return vector;
}


/*
 * Given a string, split it at any of the provided separators to form a
 * vector, destructively modifying the string to nul-terminate each segment.
 * Any number of consecutive separators are considered a single separator.
 * Reuse the provided vector if non-NULL.
 */
struct cvector *
cvector_split_multi(char *string, const char *seps, struct cvector *vector)
{
    char *p, *start;
    size_t i, count;

    /* If the vector argument isn't NULL, reuse it. */
    vector = cvector_reuse(vector);

    /* Count the number of strings we'll create and size the vector. */
    count = split_multi_count(string, seps);
    if (vector->allocated < count)
        cvector_resize(vector, count);

    /*
     * Walk the string and look for separators, replacing the ones that
     * terminate a substring with a nul.  start tracks the non-separator that
     * starts a new string, so as long as start == p, we're tracking a
     * sequence of separators.
     */
    for (start = string, p = string, i = 0; *p != '\0'; p++)
        if (strchr(seps, *p) != NULL) {
            if (start != p) {
                *p = '\0';
                vector->strings[i++] = start;
            }
            start = p + 1;
        }
    if (start != p)
        vector->strings[i++] = start;
    vector->count = i;
    return vector;
}


/*
 * Given a string, split it at whitespace to form a vector, copying each
 * string segment.  Any number of consecutive whitespace characters are
 * considered a single separator.  Reuse the provided vector if non-NULL.
 * This is just a special case of vector_split_multi.
 */
struct vector *
vector_split_space(const char *string, struct vector *vector)
{
    return vector_split_multi(string, " \t", vector);
}


/*
 * Given a string, split it at whitespace to form a vector, destructively
 * modifying the string to nul-terminate each segment.  Any number of
 * consecutive whitespace characters are considered a single separator.  Reuse
 * the provided vector if non-NULL.  This is just a special case of
 * cvector_split_multi.
 */
struct cvector *
cvector_split_space(char *string, struct cvector *vector)
{
    return cvector_split_multi(string, " \t", vector);
}


/*
 * Given a vector and a separator string, allocate and build a new string
 * composed of all the strings in the vector separated from each other by the
 * separator string.  Caller is responsible for freeing.
 */
char *
vector_join(const struct vector *vector, const char *separator)
{
    char *string;
    size_t i, size, seplen;

    /* If the vector is empty, this is trivial. */
    assert(vector != NULL);
    if (vector->count == 0)
        return xstrdup("");

    /*
     * Determine the total size of the resulting string.  Be careful of
     * integer overflow while doing so.
     */
    seplen = strlen(separator);
    for (size = 0, i = 0; i < vector->count; i++) {
        assert(SIZE_MAX - size >= strlen(vector->strings[i]) + seplen + 1);
        size += strlen(vector->strings[i]);
    }
    assert(SIZE_MAX - size >= (vector->count - 1) * seplen + 1);
    size += (vector->count - 1) * seplen + 1;

    /* Allocate the memory and build up the string using strlcat. */
    string = xmalloc(size);
    strlcpy(string, vector->strings[0], size);
    for (i = 1; i < vector->count; i++) {
        strlcat(string, separator, size);
        strlcat(string, vector->strings[i], size);
    }
    return string;
}

char *
cvector_join(const struct cvector *vector, const char *separator)
{
    char *string;
    size_t i, size, seplen;

    /* If the vector is empty, this is trivial. */
    assert(vector != NULL);
    if (vector->count == 0)
        return xstrdup("");

    /*
     * Determine the total size of the resulting string.  Be careful of
     * integer overflow while doing so.
     */
    seplen = strlen(separator);
    for (size = 0, i = 0; i < vector->count; i++) {
        assert(SIZE_MAX - size >= strlen(vector->strings[i]));
        size += strlen(vector->strings[i]);
    }
    assert(SIZE_MAX - size >= (vector->count - 1) * seplen + 1);
    size += (vector->count - 1) * seplen + 1;

    /* Allocate the memory and build up the string using strlcat. */
    string = xmalloc(size);
    strlcpy(string, vector->strings[0], size);
    for (i = 1; i < vector->count; i++) {
        strlcat(string, separator, size);
        strlcat(string, vector->strings[i], size);
    }
    return string;
}


/*
 * Given a vector and a path to a program, exec that program with the vector
 * as its arguments.  This requires adding a NULL terminator to the vector
 * (which we do not add to count, so it will be invisible to other users of
 * the vector) and casting it appropriately.
 */
int
vector_exec(const char *path, struct vector *vector)
{
    assert(vector != NULL);
    if (vector->allocated == vector->count)
        vector_resize(vector, vector->count + 1);
    vector->strings[vector->count] = NULL;
    return execv(path, (char * const *) vector->strings);
}

int
cvector_exec(const char *path, struct cvector *vector)
{
    assert(vector != NULL);
    if (vector->allocated == vector->count)
        cvector_resize(vector, vector->count + 1);
    vector->strings[vector->count] = NULL;
    return execv(path, (char * const *) vector->strings);
}
