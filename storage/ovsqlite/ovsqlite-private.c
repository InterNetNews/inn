#include "ovsqlite-private.h"

#ifdef HAVE_SQLITE3

#include <string.h>
#include "inn/xmalloc.h"

bool unpack_now(
    buffer_t *src,
    void *bytes,
    size_t count)
{
    if (count>src->left)
        return false;
    if (bytes && count>0)
        memcpy(bytes, src->data+src->used, count);
    src->used += count;
    src->left -= count;
    return true;
}

void *unpack_later(
    buffer_t *src,
    size_t count)
{
    void *result;

    if (count>src->left)
        return NULL;
    result = src->data+src->used;
    src->used += count;
    src->left -= count;
    return result;
}

size_t pack_now(
    buffer_t *dst,
    void const *bytes,
    size_t count)
{
    if (bytes) {
        buffer_append(dst, bytes, count);
    } else {
        buffer_resize(dst, dst->used+dst->left+count);
        dst->left += count;
    }
    return dst->left;
}

size_t pack_later(
    buffer_t *dst,
    size_t count)
{
    size_t result;

    result = dst->left;
    buffer_resize(dst, dst->used+result+count);
    dst->left = result+count;
    return result;
}

#endif /* HAVE_SQLITE3 */
