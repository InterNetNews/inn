/* $Id$ */
/* buffer test suite. */

#include "config.h"
#include "clibrary.h"
#include <fcntl.h>

#include "inn/buffer.h"
#include "inn/messages.h"
#include "libinn.h"
#include "libtest.h"

static const char test_string1[] = "This is a test";
static const char test_string2[] = " of the buffer system";
static const char test_string3[] = "This is a test\0 of the buffer system";

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

    puts("45");

    buffer_set(&one, test_string1, sizeof(test_string1));
    ok_int(1, 1024, one.size);
    ok_int(2, 0, one.used);
    ok_int(3, sizeof(test_string1), one.left);
    ok_string(4, test_string1, one.data);
    buffer_append(&one, test_string2, sizeof(test_string2));
    ok_int(5, 1024, one.size);
    ok_int(6, 0, one.used);
    ok_int(7, sizeof(test_string3), one.left);
    ok(8, memcmp(one.data, test_string3, sizeof(test_string3)) == 0);
    one.left -= sizeof(test_string1);
    one.used += sizeof(test_string1);
    buffer_append(&one, test_string1, sizeof(test_string1));
    ok_int(9, 1024, one.size);
    ok_int(10, sizeof(test_string1), one.used);
    ok_int(11, sizeof(test_string3), one.left);
    ok(12,
       memcmp(one.data + one.used, test_string2, sizeof(test_string2)) == 0);
    ok(13,
       memcmp(one.data + one.used + sizeof(test_string2), test_string1,
              sizeof(test_string1)) == 0);
    buffer_set(&one, test_string1, sizeof(test_string1));
    buffer_set(&two, test_string2, sizeof(test_string2));
    buffer_swap(&one, &two);
    ok_int(14, 1024, one.size);
    ok_int(15, 0, one.used);
    ok_int(16, sizeof(test_string2), one.left);
    ok_string(17, test_string2, one.data);
    ok_int(18, 1024, two.size);
    ok_int(19, 0, two.used);
    ok_int(20, sizeof(test_string1), two.left);
    ok_string(21, test_string1, two.data);
    free(one.data);
    free(two.data);
    one.data = NULL;
    two.data = NULL;
    one.size = 0;
    two.size = 0;

    three = buffer_new();
    ok(22, three != NULL);
    ok_int(23, 0, three->size);
    buffer_set(three, test_string1, sizeof(test_string1));
    ok_int(24, 1024, three->size);
    buffer_resize(three, 512);
    ok_int(25, 1024, three->size);
    buffer_resize(three, 1025);
    ok_int(26, 2048, three->size);
    buffer_free(three);

    fd = open("buffer-test", O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        sysdie("cannot create buffer-test");
    data = xmalloc(2048);
    memset(data, 'a', 1023);
    data[1023] = '\r';
    data[1024] = '\n';
    memset(data + 1025, 'b', 1023);
    if (xwrite(fd, data, 2048) < 2048)
        sysdie("cannot write to buffer-test");
    if (lseek(fd, 0, SEEK_SET) == (off_t) -1)
        sysdie("cannot rewind buffer-test");
    three = buffer_new();
    ok(27, three != NULL);
    ok_int(28, 0, three->size);
    buffer_resize(three, 1024);
    ok_int(29, 1024, three->size);
    count = buffer_read(three, fd);
    ok_int(30, 1024, count);
    offset = 0;
    ok(31, !buffer_find_string(three, "\r\n", 0, &offset));
    ok_int(32, 0, offset);
    ok(33, memcmp(three->data, data, three->size) == 0);
    buffer_resize(three, 2048);
    ok_int(34, 2048, three->size);
    count = buffer_read(three, fd);
    ok_int(35, 1024, count);
    ok(36, memcmp(three->data, data, 2048) == 0);
    ok(37, !buffer_find_string(three, "\r\n", 1024, &offset));
    ok_int(38, 0, offset);
    ok(39, buffer_find_string(three, "\r\n", 0, &offset));
    ok_int(40, 1023, offset);
    three->used += 400;
    three->left -= 400;
    buffer_compact(three);
    ok_int(41, 2048, three->size);
    ok_int(42, 0, three->used);
    ok_int(43, 1648, three->left);
    ok(44, memcmp(three->data, data + 400, 1648) == 0);
    count = buffer_read(three, fd);
    ok_int(45, 0, count);
    close(fd);
    unlink("buffer-test");
    free(data);
    buffer_free(three);

    return 0;
}
