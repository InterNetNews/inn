/*
 * strlcat test suite.
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

#include "libtest.h"

size_t test_strlcat(char *, const char *, size_t);


int
main(void)
{
    char buffer[10] = "";

    plan(27);

    is_int(3, test_strlcat(buffer, "foo", sizeof(buffer)),
           "strlcat into empty buffer");
    is_string("foo", buffer, "...with right output");
    is_int(7, test_strlcat(buffer, " bar", sizeof(buffer)),
           "...and append more");
    is_string("foo bar", buffer, "...and output is still correct");
    is_int(9, test_strlcat(buffer, "!!", sizeof(buffer)),
           "...and append to buffer limit");
    is_string("foo bar!!", buffer, "...output is still correct");
    is_int(10, test_strlcat(buffer, "!", sizeof(buffer)),
           "...append one more character");
    is_string("foo bar!!", buffer, "...and output didn't change");
    ok(buffer[9] == '\0', "...buffer still nul-terminated");
    buffer[0] = '\0';
    is_int(11, test_strlcat(buffer, "hello world", sizeof(buffer)),
           "append single long string");
    is_string("hello wor", buffer, "...string truncates properly");
    ok(buffer[9] == '\0', "...buffer still nul-terminated");
    buffer[0] = '\0';
    is_int(7, test_strlcat(buffer, "sausage", 5), "lie about buffer length");
    is_string("saus", buffer, "...contents are correct");
    is_int(14, test_strlcat(buffer, "bacon eggs", sizeof(buffer)),
           "...add more up to real size");
    is_string("sausbacon", buffer, "...and result is correct");

    /* Make sure that with a size of 0, the destination isn't changed. */
    is_int(11, test_strlcat(buffer, "!!", 0), "no change with size of 0");
    is_string("sausbacon", buffer, "...and content is the same");

    /* Now play with empty strings. */
    is_int(9, test_strlcat(buffer, "", 0),
           "correct count when appending empty string");
    is_string("sausbacon", buffer, "...and contents are unchanged");
    buffer[0] = '\0';
    is_int(0, test_strlcat(buffer, "", sizeof(buffer)),
           "correct count when appending empty string to empty buffer");
    is_string("", buffer, "...and buffer content is correct");
    is_int(3, test_strlcat(buffer, "foo", 2), "append to length 2 buffer");
    is_string("f", buffer, "...and got only a single character");
    ok(buffer[1] == '\0', "...and buffer is still nul-terminated");
    is_int(1, test_strlcat(buffer, "", sizeof(buffer)),
           "append an empty string");
    ok(buffer[1] == '\0', "...and buffer is still nul-terminated");

    return 0;
}
