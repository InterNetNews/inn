/* Test suite for the innfeed Buffer class. */

#include "portable/system.h"

#include <stdarg.h>

#include "inn/libinn.h"
#include "tap/basic.h"

#include "../../innfeed/buffer.h"

/* Stubs for debugging helpers used by buffer.c. */
void
d_printf(unsigned int level UNUSED, const char *format UNUSED, ...)
{
}

const char *
boolToString(bool value)
{
    return value ? "true" : "false";
}

static void
set_data(Buffer buffer, const char *data)
{
    size_t length = strlen(data);

    memcpy(bufferBase(buffer), data, length);
    bufferSetDataSize(buffer, length);
}

int
main(void)
{
    static const char input[] = "first\n.second";
    static const char expected[] = "first\r\n..second\r\n.\r\n";
    Buffer dest, empty;
    Buffer *array, *source_array;

    test_init(13);

    /* An empty buffer is a valid NNTP payload and becomes just ".\r\n". */
    empty = newBuffer(0);
    ok(1, nntpPrepareBuffer(empty));
    ok_int(2, 3, bufferDataSize(empty));
    ok_int(3, 0, memcmp(bufferBase(empty), ".\r\n", 3));
    delBuffer(empty);

    /* Normal preparation adds CRs, dot-stuffs, and terminates the article. */
    dest = newBuffer(strlen(input));
    set_data(dest, input);
    ok(4, nntpPrepareBuffer(dest));
    ok_int(5, strlen(expected), bufferDataSize(dest));
    ok_string(6, expected, bufferBase(dest));
    delBuffer(dest);

    dest = newBuffer(3);
    set_data(dest, "abc");
    bufferDecrDataSize(dest, 2);
    ok_int(7, 1, bufferDataSize(dest));
    delBuffer(dest);

    array = dupBufferArray(NULL);
    ok(8, array != NULL);
    ok(9, array[0] == NULL);
    free(array);

    dest = newBuffer(4);
    set_data(dest, "ab");
    source_array = makeBufferArray(dest, NULL);
    array = dupBufferArray(source_array);
    free(source_array);
    ok(10, array != NULL);
    ok(11, array[0] == dest);
    ok_int(12, 2, bufferRefCount(dest));
    freeBufferArray(array);
    delBuffer(dest);

    dest = newBuffer(1);
    ok(13, !expandBuffer(dest, SIZE_MAX));
    delBuffer(dest);

    return 0;
}
