/* $Id$ */
/* strlcpy test suite. */

#include "config.h"
#include "clibrary.h"

#include "libtest.h"

size_t test_strlcpy(char *, const char *, size_t);

int
main(void)
{
    char buffer[10];

    puts("23");

    ok_int(1, 3, test_strlcpy(buffer, "foo", sizeof(buffer)));
    ok_string(2, "foo", buffer);
    ok_int(3, 9, test_strlcpy(buffer, "hello wor", sizeof(buffer)));
    ok_string(4, "hello wor", buffer);
    ok_int(5, 10, test_strlcpy(buffer, "world hell", sizeof(buffer)));
    ok_string(6, "world hel", buffer);
    ok(7, buffer[9] == '\0');
    ok_int(8, 11, test_strlcpy(buffer, "hello world", sizeof(buffer)));
    ok_string(9, "hello wor", buffer);
    ok(10, buffer[9] == '\0');

    /* Make sure that with a size of 0, the destination isn't changed. */
    ok_int(11, 3, test_strlcpy(buffer, "foo", 0));
    ok_string(12, "hello wor", buffer);

    /* Now play with empty strings. */
    ok_int(13, 0, test_strlcpy(buffer, "", 0));
    ok_string(14, "hello wor", buffer);
    ok_int(15, 0, test_strlcpy(buffer, "", sizeof(buffer)));
    ok_string(16, "", buffer);
    ok_int(17, 3, test_strlcpy(buffer, "foo", 2));
    ok_string(18, "f", buffer);
    ok(19, buffer[1] == '\0');
    ok_int(20, 0, test_strlcpy(buffer, "", 1));
    ok(21, buffer[0] == '\0');

    /* Finally, check using strlcpy as strlen. */
    ok_int(22, 3, test_strlcpy(NULL, "foo", 0));
    ok_int(23, 11, test_strlcpy(NULL, "hello world", 0));

    return 0;
}
