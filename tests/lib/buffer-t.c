/*
 * buffer test suite.
 *
 * $Id$
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 *
 * The authors hereby relinquish any claim to any copyright that they may have
 * in this work, whether granted under contract or by operation of law or
 * international treaty, and hereby commit to the public, at large, that they
 * shall not, at any time in the future, seek to enforce any copyright in this
 * work against any person or entity, or prevent any person or entity from
 * copying, publishing, distributing or creating derivative works of this
 * work.
 */

#define LIBTEST_NEW_FORMAT 1

#include "config.h"
#include "clibrary.h"
#include <fcntl.h>

#include "inn/buffer.h"
#include "inn/messages.h"
#include "inn/libinn.h"
#include "libtest.h"

static const char test_string1[] = "This is a test";
static const char test_string2[] = " of the buffer system";
static const char test_string3[] = "This is a test\0 of the buffer system";

/*
 * Test buffer_vsprintf.  Wrapper needed to generate the va_list.
 */
static void
test_vsprintf(struct buffer *buffer, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    buffer_vsprintf(buffer, format, args);
    va_end(args);
}


/*
 * Likewise for buffer_append_vsprintf.
 */
static void
test_append_vsprintf(struct buffer *buffer, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    buffer_append_vsprintf(buffer, format, args);
    va_end(args);
}


int
main(void)
{
    struct buffer one = { 0, 0, 0, NULL };
    struct buffer two = { 0, 0, 0, NULL };
    struct buffer *three;
    int fd;
    char *data;
    ssize_t count;
    size_t offset;

    plan(89);

    /* buffer_set, buffer_append, buffer_swap */
    buffer_set(&one, test_string1, sizeof(test_string1));
    is_int(1024, one.size, "minimum size is 1024");
    is_int(0, one.used, "used starts at 0");
    is_int(sizeof(test_string1), one.left, "left is correct");
    is_string(test_string1, one.data, "data is corect");
    buffer_append(&one, test_string2, sizeof(test_string2));
    is_int(1024, one.size, "appended data doesn't change size");
    is_int(0, one.used, "or used");
    is_int(sizeof(test_string3), one.left, "but left is the right size");
    ok(memcmp(one.data, test_string3, sizeof(test_string3)) == 0,
       "and the resulting data is correct");
    one.left -= sizeof(test_string1);
    one.used += sizeof(test_string1);
    buffer_append(&one, test_string1, sizeof(test_string1));
    is_int(1024, one.size, "size still isn't larger after adding data");
    is_int(sizeof(test_string1), one.used, "and used is preserved on append");
    is_int(sizeof(test_string3), one.left, "and left is updated properly");
    ok(memcmp(one.data + one.used, test_string2, sizeof(test_string2)) == 0,
       "and the middle data is unchanged");
    ok(memcmp(one.data + one.used + sizeof(test_string2), test_string1,
              sizeof(test_string1)) == 0, "and the final data is correct");
    buffer_set(&one, test_string1, sizeof(test_string1));
    buffer_set(&two, test_string2, sizeof(test_string2));
    buffer_swap(&one, &two);
    is_int(1024, one.size, "swap #1 size is correct");
    is_int(0, one.used, "swap #1 used is correct");
    is_int(sizeof(test_string2), one.left, "swap #1 left is correct");
    is_string(test_string2, one.data, "swap #1 data is correct");
    is_int(1024, two.size, "swap #2 size is correct");
    is_int(0, two.used, "swap #2 used is correct");
    is_int(sizeof(test_string1), two.left, "swap #2 left is correct");
    is_string(test_string1, two.data, "swap #2 data is correct");
    free(one.data);
    free(two.data);
    one.data = NULL;
    two.data = NULL;
    one.size = 0;
    two.size = 0;

    /* buffer_resize */
    three = buffer_new();
    ok(three != NULL, "buffer_new works");
    is_int(0, three->size, "initial size is 0");
    buffer_set(three, test_string1, sizeof(test_string1));
    is_int(1024, three->size, "size becomes 1024 when adding data");
    buffer_resize(three, 512);
    is_int(1024, three->size, "resizing to something smaller doesn't change");
    buffer_resize(three, 1025);
    is_int(2048, three->size, "resizing to something larger goes to 2048");
    buffer_free(three);

    /* buffer_read, buffer_find_string, buffer_compact */
    fd = open("buffer-test", O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        sysbail("cannot create buffer-test");
    data = xmalloc(2048);
    memset(data, 'a', 1023);
    data[1023] = '\r';
    data[1024] = '\n';
    memset(data + 1025, 'b', 1023);
    if (xwrite(fd, data, 2048) < 2048)
        sysbail("cannot write to buffer-test");
    if (lseek(fd, 0, SEEK_SET) == (off_t) -1)
        sysbail("cannot rewind buffer-test");
    three = buffer_new();
    ok(three != NULL, "buffer_new works");
    is_int(0, three->size, "and initial size is 0");
    buffer_resize(three, 1024);
    is_int(1024, three->size, "resize to 1024 works");
    count = buffer_read(three, fd);
    is_int(1024, count, "reading into a buffer of size 1024 reads 1024");
    offset = 0;
    ok(!buffer_find_string(three, "\r\n", 0, &offset),
       "buffer_find_string with truncated string fails");
    is_int(0, offset, "and offset is unchanged");
    ok(memcmp(three->data, data, three->size) == 0, "buffer data is correct");
    buffer_resize(three, 2048);
    is_int(2048, three->size, "resizing the buffer to 2048 works");
    count = buffer_read(three, fd);
    is_int(1024, count, "and now we can read the rest of the data");
    ok(memcmp(three->data, data, 2048) == 0, "and it's all there");
    ok(!buffer_find_string(three, "\r\n", 1024, &offset),
       "buffer_find_string with a string starting before offset fails");
    is_int(0, offset, "and offset is unchanged");
    ok(buffer_find_string(three, "\r\n", 0, &offset),
       "finding the string on the whole buffer works");
    is_int(1023, offset, "and returns the correct location");
    three->used += 400;
    three->left -= 400;
    buffer_compact(three);
    is_int(2048, three->size, "compacting buffer doesn't change the size");
    is_int(0, three->used, "but used is now zero");
    is_int(1648, three->left, "and left is decreased appropriately");
    ok(memcmp(three->data, data + 400, 1648) == 0, "and the data is correct");
    count = buffer_read(three, fd);
    is_int(0, count, "reading at EOF returns 0");
    close(fd);
    unlink("buffer-test");
    free(data);
    buffer_free(three);

    /* buffer_sprintf and buffer_append_sprintf */
    three = buffer_new();
    buffer_append_sprintf(three, "testing %d testing", 6);
    is_int(0, three->used, "buffer_append_sprintf doesn't change used");
    is_int(17, three->left, "but sets left correctly");
    buffer_append(three, "", 1);
    is_int(18, three->left, "appending a nul works");
    is_string("testing 6 testing", three->data, "and the data is correct");
    three->left--;
    three->used += 5;
    three->left -= 5;
    buffer_append_sprintf(three, " %d", 7);
    is_int(14, three->left, "appending a digit works");
    buffer_append(three, "", 1);
    is_string("testing 6 testing 7", three->data, "and the data is correct");
    buffer_sprintf(three, "%d testing", 8);
    is_int(9, three->left, "replacing the buffer works");
    is_string("8 testing", three->data, "and the results are correct");
    data = xmalloc(1050);
    memset(data, 'a', 1049);
    data[1049] = '\0';
    is_int(1024, three->size, "size before large sprintf is 1024");
    buffer_sprintf(three, "%s", data);
    is_int(2048, three->size, "size after large sprintf is 2048");
    is_int(1049, three->left, "and left is correct");
    buffer_append(three, "", 1);
    is_string(data, three->data, "and data is correct");
    free(data);
    buffer_free(three);

    /* buffer_read_all */
    fd = open("buffer-test", O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        sysbail("cannot create buffer-test");
    data = xmalloc(2049);
    memset(data, 'a', 2049);
    if (xwrite(fd, data, 2049) < 2049)
        sysbail("cannot write to buffer-test");
    if (lseek(fd, 0, SEEK_SET) == (off_t) -1)
        sysbail("cannot rewind buffer-test");
    three = buffer_new();
    ok(buffer_read_all(three, fd), "buffer_read_all succeeds");
    is_int(0, three->used, "and unused is zero");
    is_int(2049, three->left, "and left is correct");
    is_int(4096, three->size, "and size is correct");
    ok(memcmp(data, three->data, 2049) == 0, "and data is correct");
    if (lseek(fd, 0, SEEK_SET) == (off_t) -1)
        sysbail("cannot rewind buffer-test");
    ok(buffer_read_all(three, fd), "reading again succeeds");
    is_int(0, three->used, "and used is correct");
    is_int(4098, three->left, "and left is now larger");
    is_int(8192, three->size, "and size doubled");
    ok(memcmp(data, three->data + 2049, 2049) == 0, "and data is correct");

    /* buffer_read_file */
    if (lseek(fd, 0, SEEK_SET) == (off_t) -1)
        sysbail("cannot rewind buffer-test");
    buffer_free(three);
    three = buffer_new();
    ok(buffer_read_file(three, fd), "buffer_read_file succeeds");
    is_int(0, three->used, "and leaves unused at 0");
    is_int(2049, three->left, "and left is correct");
    is_int(3072, three->size, "and size is a multiple of 1024");
    ok(memcmp(data, three->data, 2049) == 0, "and the data is correct");

    /* buffer_read_all and buffer_read_file errors */
    close(fd);
    ok(!buffer_read_all(three, fd), "buffer_read_all on closed fd fails");
    is_int(3072, three->size, "and size is unchanged");
    ok(!buffer_read_file(three, fd), "buffer_read_file on closed fd fails");
    is_int(3072, three->size, "and size is unchanged");
    is_int(2049, three->left, "and left is unchanged");
    unlink("buffer-test");
    free(data);
    buffer_free(three);

    /* buffer_vsprintf and buffer_append_vsprintf */
    three = buffer_new();
    test_append_vsprintf(three, "testing %d testing", 6);
    is_int(0, three->used, "buffer_append_vsprintf leaves used as 0");
    is_int(17, three->left, "and left is correct");
    buffer_append(three, "", 1);
    is_int(18, three->left, "and left is correct after appending a nul");
    is_string("testing 6 testing", three->data, "and data is correct");
    three->left--;
    three->used += 5;
    three->left -= 5;
    test_append_vsprintf(three, " %d", 7);
    is_int(14, three->left, "and appending results in the correct left");
    buffer_append(three, "", 1);
    is_string("testing 6 testing 7", three->data, "and the right data");
    test_vsprintf(three, "%d testing", 8);
    is_int(9, three->left, "replacing the buffer results in the correct size");
    is_string("8 testing", three->data, "and the correct data");
    data = xmalloc(1050);
    memset(data, 'a', 1049);
    data[1049] = '\0';
    is_int(1024, three->size, "size is 1024 before large vsprintf");
    test_vsprintf(three, "%s", data);
    is_int(2048, three->size, "and 2048 afterwards");
    is_int(1049, three->left, "and left is correct");
    buffer_append(three, "", 1);
    is_string(data, three->data, "and data is correct");
    free(data);
    buffer_free(three);

    return 0;
}
