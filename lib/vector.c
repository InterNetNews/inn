/*  $Id$
**
**  Vector handling (counted lists of char *'s).
**
**  Written by Russ Allbery <eagle@eyrie.org>
**  This work is hereby placed in the public domain by its author.
**
**  A vector is a table for handling a list of strings with less overhead than
**  linked list.  The intention is for vectors, once allocated, to be reused;
**  this saves on memory allocations once the array of char *'s reaches a
**  stable size.
**
**  There are two types of vectors.  Standard vectors copy strings when
**  they're inserted into the vector, whereas cvectors just accept pointers
**  to external strings to store.  There are therefore two entry points for
**  every vector function, one for vectors and one for cvectors.
**
**  There's a whole bunch of code duplication here.  This would be a lot
**  cleaner with C++ features (either inheritance or templates would
**  probably help).  One could probably in some places just cast a cvector
**  to a vector and perform the same operations, but I'm leery of doing that
**  as I'm not sure if it's a violation of the C type aliasing rules.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>

#include "inn/vector.h"
#include "inn/libinn.h"

/*
**  Allocate a new, empty vector.
*/
struct vector *
vector_new(void)
{
    struct vector *vector;

    vector = xmalloc(sizeof(struct vector));
    vector->count = 0;
    vector->allocated = 0;
    vector->strings = NULL;
    return vector;
}

struct cvector *
cvector_new(void)
{
    struct cvector *vector;

    vector = xmalloc(sizeof(struct cvector));
    vector->count = 0;
    vector->allocated = 0;
    vector->strings = NULL;
    return vector;
}


/*
**  Resize a vector (using realloc to resize the table).
*/
void
vector_resize(struct vector *vector, size_t size)
{
    size_t i;

    if (vector->count > size) {
        for (i = size; i < vector->count; i++)
            free(vector->strings[i]);
        vector->count = size;
    }
    if (size == 0) {
        free(vector->strings);
        vector->strings = NULL;
    } else {
        vector->strings = xrealloc(vector->strings, size * sizeof(char *));
    }
    vector->allocated = size;
}

void
cvector_resize(struct cvector *vector, size_t size)
{
    if (vector->count > size)
        vector->count = size;
    if (size == 0) {
        free(vector->strings);
        vector->strings = NULL;
    } else {
        vector->strings =
            xrealloc(vector->strings, size * sizeof(const char *));
    }
    vector->allocated = size;
}


/*
**  Add a new string to the vector, resizing the vector as necessary.  The
**  vector is resized an element at a time; if a lot of resizes are expected,
**  vector_resize should be called explicitly with a more suitable size.
*/
void
vector_add(struct vector *vector, const char *string)
{
    size_t next = vector->count;

    if (vector->count == vector->allocated)
        vector_resize(vector, vector->allocated + 1);
    vector->strings[next] = xstrdup(string);
    vector->count++;
}

void
cvector_add(struct cvector *vector, const char *string)
{
    size_t next = vector->count;

    if (vector->count == vector->allocated)
        cvector_resize(vector, vector->allocated + 1);
    vector->strings[next] = string;
    vector->count++;
}


/*
**  Empty a vector but keep the allocated memory for the pointer table.
*/
void
vector_clear(struct vector *vector)
{
    size_t i;

    for (i = 0; i < vector->count; i++)
        free(vector->strings[i]);
    vector->count = 0;
}

void
cvector_clear(struct cvector *vector)
{
    vector->count = 0;
}


/*
**  Free a vector completely.
*/
void
vector_free(struct vector *vector)
{
    vector_clear(vector);
    free(vector->strings);
    free(vector);
}

void
cvector_free(struct cvector *vector)
{
    cvector_clear(vector);
    free(vector->strings);
    free(vector);
}


/*
**  Given a vector that we may be reusing, clear it out.  If the first
**  argument is NULL, allocate a new vector.  Used by vector_split*.
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
**  Given a string and a separator character, count the number of strings
**  that it will split into.
*/
static size_t
split_count(const char *string, char separator)
{
    const char *p;
    size_t count;

    if (*string == '\0')
        return 1;
    for (count = 1, p = string; *p; p++)
        if (*p == separator)
            count++;
    return count;
}


/*
**  Given a string and a separator character, form a vector by splitting the
**  string at those separators.  Do a first pass to size the vector, and if
**  the third argument isn't NULL, reuse it.  Otherwise, allocate a new one.
*/
struct vector *
vector_split(const char *string, char separator, struct vector *vector)
{
    const char *p, *start;
    size_t i, count;

    vector = vector_reuse(vector);

    count = split_count(string, separator);
    if (vector->allocated < count)
        vector_resize(vector, count);

    for (start = string, p = string, i = 0; *p; p++)
        if (*p == separator) {
            vector->strings[i++] = xstrndup(start, p - start);
            start = p + 1;
        }
    vector->strings[i++] = xstrndup(start, p - start);
    vector->count = i;

    return vector;
}


/*
**  Given a modifiable string and a separator character, form a cvector by
**  modifying the string in-place to add nuls at the separators and then
**  building a vector of pointers into the string.  Do a first pass to size
**  the vector, and if the third argument isn't NULL, reuse it.  Otherwise,
**  allocate a new one.
*/
struct cvector *
cvector_split(char *string, char separator, struct cvector *vector)
{
    char *p, *start;
    size_t i, count;

    vector = cvector_reuse(vector);

    count = split_count(string, separator);
    if (vector->allocated < count)
        cvector_resize(vector, count);

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
**  Given a string, count the number of strings that it will split into when
**  splitting on whitespace.
*/
static size_t
split_space_count(const char *string)
{
    const char *p;
    size_t count;

    if (*string == '\0')
        return 0;
    for (count = 1, p = string + 1; *p != '\0'; p++)
        if ((*p == ' ' || *p == '\t') && !(p[-1] == ' ' || p[-1] == '\t'))
            count++;

    /* If the string ends in whitespace, we've overestimated the number of
       strings by one. */
    if (p[-1] == ' ' || p[-1] == '\t')
        count--;
    return count;
}


/*
**  Given a string, split it at whitespace to form a vector, copying each
**  string segment.  If the second argument isn't NULL, reuse that vector;
**  otherwise, allocate a new one.  Any number of consecutive whitespace
**  characters is considered a single separator.
*/
struct vector *
vector_split_space(const char *string, struct vector *vector)
{
    const char *p, *start;
    size_t i, count;

    vector = vector_reuse(vector);

    count = split_space_count(string);
    if (vector->allocated < count)
        vector_resize(vector, count);

    for (start = string, p = string, i = 0; *p; p++)
        if (*p == ' ' || *p == '\t') {
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
**  Given a string, split it at whitespace to form a vector, destructively
**  modifying the string to nul-terminate each segment.  If the second
**  argument isn't NULL, reuse that vector; otherwise, allocate a new one.
**  Any number of consecutive whitespace characters is considered a single
**  separator.
*/
struct cvector *
cvector_split_space(char *string, struct cvector *vector)
{
    char *p, *start;
    size_t i, count;

    vector = cvector_reuse(vector);

    count = split_space_count(string);
    if (vector->allocated < count)
        cvector_resize(vector, count);

    for (start = string, p = string, i = 0; *p; p++)
        if (*p == ' ' || *p == '\t') {
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
**  Given a vector and a separator string, allocate and build a new string
**  composed of all the strings in the vector separated from each other by the
**  seperator string.  Caller is responsible for freeing.
*/
char *
vector_join(const struct vector *vector, const char *seperator)
{
    char *string;
    size_t i, size, seplen;

    seplen = strlen(seperator);
    for (size = 0, i = 0; i < vector->count; i++)
        size += strlen(vector->strings[i]);
    size += (vector->count - 1) * seplen + 1;

    string = xmalloc(size);
    strlcpy(string, vector->strings[0], size);
    for (i = 1; i < vector->count; i++) {
        strlcat(string, seperator, size);
        strlcat(string, vector->strings[i], size);
    }

    return string;
}

char *
cvector_join(const struct cvector *vector, const char *seperator)
{
    char *string;
    size_t i, size, seplen;

    seplen = strlen(seperator);
    for (size = 0, i = 0; i < vector->count; i++)
        size += strlen(vector->strings[i]);
    size += (vector->count - 1) * seplen + 1;

    string = xmalloc(size);
    strlcpy(string, vector->strings[0], size);
    for (i = 1; i < vector->count; i++) {
        strlcat(string, seperator, size);
        strlcat(string, vector->strings[i], size);
    }

    return string;
}


/*
**  Given a vector and a path to a program, exec that program with the vector
**  as its arguments.  This requires adding a NULL terminator to the vector
**  and casting it appropriately.
*/
int
vector_exec(const char *path, struct vector *vector)
{
    if (vector->allocated == vector->count)
        vector_resize(vector, vector->count + 1);
    vector->strings[vector->count] = NULL;
    return execv(path, (char * const *) vector->strings);
}

int
cvector_exec(const char *path, struct cvector *vector)
{
    if (vector->allocated == vector->count)
        cvector_resize(vector, vector->count + 1);
    vector->strings[vector->count] = NULL;
    return execv(path, (char * const *) vector->strings);
}
