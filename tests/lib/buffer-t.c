/* $Id$ */
/* buffer test suite. */

#include "config.h"
#include "clibrary.h"

#include "inn/buffer.h"
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

    puts("26");

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

    three = buffer_new();
    ok(22, three != NULL);
    ok_int(23, 0, three->size);
    buffer_set(three, test_string1, sizeof(test_string1));
    ok_int(24, 1024, three->size);
    buffer_resize(three, 512);
    ok_int(25, 1024, three->size);
    buffer_resize(three, 1025);
    ok_int(26, 2048, three->size);
    free(three->data);
    free(three);

    return 0;
}
