/*  $Id$
**
**  Vector handling (counted lists of char *'s).
**
**  Written by Russ Allbery <rra@stanford.edu>
**  This work is hereby placed in the public domain by its author.
**
**  A vector is a table for handling a list of strings with less overhead than
**  linked list.  The intention is for vectors, once allocated, to be reused;
**  this saves on memory allocations once the array of char *'s reaches a
**  stable size.
**
**  Vectors may be shallow, in which case the char *'s are pointers into
**  another string, or deep, in which case each char * points to separately
**  allocated memory.  The depth of a vector is set at creation, and affects
**  operations later performed on it.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>

#include "inn/vector.h"
#include "libinn.h"

/*
**  Allocate a new, empty vector.
*/
struct vector *
vector_new(bool shallow)
{
    struct vector *vector;

    vector = xmalloc(sizeof(struct vector));
    vector->count = 0;
    vector->allocated = 0;
    vector->strings = NULL;
    vector->shallow = shallow;
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
        if (!vector->shallow)
            for (i = vector->count - 1; i < size; i++)
                free(vector->strings[i]);
        vector->count = size;
    }
    if (size == 0)
        vector->strings = NULL;
    else
        vector->strings = xrealloc(vector->strings, size * sizeof(char *));
    vector->allocated = size;
}


/*
**  Add a new string to the vector, resizing the vector as necessary.  The
**  vector is resized an element at a time; if a lot of resizes are expected,
**  vector_resize should be called explicitly with a more suitable size.
*/
void
vector_add(struct vector *vector, char *string)
{
    size_t next = vector->count;

    if (vector->count == vector->allocated)
        vector_resize(vector, vector->allocated + 1);
    vector->strings[next] = vector->shallow ? string : xstrdup(string);
    vector->count++;
}


/*
**  Empty a vector but keep the allocated memory for the char * table.
*/
void
vector_clear(struct vector *vector)
{
    size_t i;

    if (!vector->shallow)
        for (i = 0; i < vector->count; i++)
            free(vector->strings[i]);
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


/*
**  Given a vector that we may be reusing and whether or not it should be
**  shallow, clear it out and initialize the shallow flag.  If the first
**  argument is NULL, allocate a new vector.  Used by vector_split*.
*/
static struct vector *
vector_reuse(struct vector *vector, bool shallow)
{
    if (vector == NULL)
        return vector_new(shallow);
    else {
        vector_clear(vector);
        vector->shallow = shallow;
        return vector;
    }
}


/*
**  Given a string (specified by a starting address and a length), either copy
**  it into the vector if the vector is deep or otherwise add the pointer to
**  the vector.  If the vector is shallow, destructively modify the string by
**  nul-terminated it at the specified length.  Takes the vector, the index
**  location, the starting address of the string, and the string length.  Used
**  by vector_split*.
*/
static void
vector_insert(struct vector *vector, size_t i, char *string, ptrdiff_t length)
{
    if (vector->shallow) {
        *(string + length) = '\0';
        vector->strings[i] = string;
    } else {
        vector->strings[i] = xmalloc(length + 1);
        strncpy(vector->strings[i], string, length);
        vector->strings[i][length] = '\0';
    }
}


/*
**  Given a string and a separator character, form a vector, deep if the third
**  argument is true and shallow otherwise.  Do a first pass to size the
**  vector, and if the fourth argument isn't NULL, reuse it.  Otherwise,
**  allocate a new one.
*/
struct vector *
vector_split(char *string, char separator, bool copy, struct vector *vector)
{
    char *p, *start;
    size_t i, count;

    vector = vector_reuse(vector, !copy);

    if (*string == '\0')
        return vector;

    for (count = 1, p = string + 1; *p; p++)
        if (*p == separator && *(p - 1) != separator)
            count++;
    if (vector->allocated < count)
        vector_resize(vector, count);

    for (start = string, p = string, i = 0; *p; p++)
        if (*p == separator) {
            if (p == start) {
                start++;
            } else {
                vector_insert(vector, i, start, p - start);
                i++;
                start = p + 1;
            }
        }
    if (start != p) {
        vector_insert(vector, i, start, p - start);
        i++;
    }
    vector->count = i;

    return vector;
}


/*
**  Given a string, split it at whitespace to form a vector, deep if the third
**  argument is true and shallow otherwise.  If the fourth argument isn't
**  NULL, reuse that vector; otherwise, allocate a new one.  Any number of
**  consecutive whitespace characters is considered a single separator.
*/
struct vector *
vector_split_whitespace(char *string, bool copy, struct vector *vector)
{
    char *p, *start;
    size_t i, count;

    vector = vector_reuse(vector, !copy);

    if (*string == '\0')
        return vector;

    for (count = 1, p = string + 1; *p; p++)
        if (CTYPE(isspace, *p) && !CTYPE(isspace, *(p - 1)))
            count++;
    if (vector->allocated < count)
        vector_resize(vector, count);

    for (start = string, p = string, i = 0; *p; p++)
        if (CTYPE(isspace, *p)) {
            if (p == start) {
                start++;
            } else {
                vector_insert(vector, i, start, p - start);
                i++;
                for (++p; *p && CTYPE(isspace, *p); p++)
                    ;
                start = p;
            }
        }
    if (start != p) {
        vector_insert(vector, i, start, p - start);
        i++;
    }
    vector->count = i;

    return vector;
}


/*
**  Given a vector and a separator string, allocate and build a new string
**  composed of all the strings in the vector separated from each other by the
**  seperator string.  Caller is responsible for freeing.
*/
char *
vector_join(struct vector *vector, const char *seperator)
{
    char *string;
    size_t i, size, seplen;

    seplen = strlen(seperator);
    for (size = 0, i = 0; i < vector->count; i++)
        size += strlen(vector->strings[i]);
    size += (vector->count - 1) * seplen;

    string = xmalloc(size + 1);
    strcpy(string, vector->strings[0]);
    for (i = 1; i < vector->count; i++) {
        strcat(string, seperator);
        strcat(string, vector->strings[i]);
    }

    return string;
}
