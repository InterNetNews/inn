/* $Id$ */
/* strlcat test suite. */

#include "config.h"
#include "clibrary.h"

#include "libtest.h"

size_t test_strlcat(char *, const char *, size_t);

int
main(void)
{
    char buffer[10] = "";

    puts("27");

    ok_int(1, 3, test_strlcat(buffer, "foo", sizeof(buffer)));
    ok_string(2, "foo", buffer);
    ok_int(3, 7, test_strlcat(buffer, " bar", sizeof(buffer)));
    ok_string(4, "foo bar", buffer);
    ok_int(5, 9, test_strlcat(buffer, "!!", sizeof(buffer)));
    ok_string(6, "foo bar!!", buffer);
    ok_int(7, 10, test_strlcat(buffer, "!", sizeof(buffer)));
    ok_string(8, "foo bar!!", buffer);
    ok(9, buffer[9] == '\0');
    buffer[0] = '\0';
    ok_int(10, 11, test_strlcat(buffer, "hello world", sizeof(buffer)));
    ok_string(11, "hello wor", buffer);
    ok(12, buffer[9] == '\0');
    buffer[0] = '\0';
    ok_int(13, 7, test_strlcat(buffer, "sausage", 5));
    ok_string(14, "saus", buffer);
    ok_int(15, 14, test_strlcat(buffer, "bacon eggs", sizeof(buffer)));
    ok_string(16, "sausbacon", buffer);

    /* Make sure that with a size of 0, the destination isn't changed. */
    ok_int(17, 11, test_strlcat(buffer, "!!", 0));
    ok_string(18, "sausbacon", buffer);

    /* Now play with empty strings. */
    ok_int(19, 9, test_strlcat(buffer, "", 0));
    ok_string(20, "sausbacon", buffer);
    buffer[0] = '\0';
    ok_int(21, 0, test_strlcat(buffer, "", sizeof(buffer)));
    ok_string(22, "", buffer);
    ok_int(23, 3, test_strlcat(buffer, "foo", 2));
    ok_string(24, "f", buffer);
    ok(25, buffer[1] == '\0');
    ok_int(26, 1, test_strlcat(buffer, "", sizeof(buffer)));
    ok(27, buffer[1] == '\0');

    return 0;
}
