/* $Id$ */
/* vector test suite. */

#include "config.h"
#include "clibrary.h"

#include "inn/vector.h"
#include "libinn.h"

static void
ok(int n, int success)
{
    printf("%sok %d\n", success ? "" : "not ", n);
}

int
main(void)
{
    struct vector *vector;
    char cstring[] = "This is a test.";
    char empty[] = "";
    char tabs[] = "test\t\ting\t";
    char *string = xstrdup(cstring);

    puts("38");

    vector = vector_new(true);
    ok(1, vector && vector->shallow);
    vector_add(vector, cstring);
    ok(2, vector->count == 1);
    ok(3, vector->strings[0] == cstring);
    vector_resize(vector, 4);
    ok(4, vector->allocated == 4);
    vector_add(vector, cstring);
    vector_add(vector, cstring);
    vector_add(vector, cstring);
    ok(5, vector->allocated == 4);
    ok(6, vector->count == 4);
    ok(7, vector->strings[1] == vector->strings[2]);
    ok(8, vector->strings[2] == vector->strings[3]);
    ok(9, vector->strings[0] == vector->strings[3]);
    ok(10, vector->strings[0] == cstring);
    vector_clear(vector);
    ok(11, vector->count == 0);
    ok(12, vector->allocated == 4);
    vector_add(vector, cstring);
    vector_add(vector, string);
    ok(13, vector->count == 2);
    ok(14, vector->strings[1] == string);
    vector_resize(vector, 1);
    ok(15, vector->count == 1);
    ok(16, vector->strings[0] == cstring);
    vector_free(vector);

    vector = vector_split_whitespace(cstring, true, NULL);
    ok(17, vector->count == 4);
    ok(18, vector->allocated = 4);
    ok(19, strcmp(vector->strings[0], "This") == 0);
    ok(20, strcmp(vector->strings[1], "is") == 0);
    ok(21, strcmp(vector->strings[2], "a") == 0);
    ok(22, strcmp(vector->strings[3], "test.") == 0);
    vector_add(vector, cstring);
    ok(23, strcmp(vector->strings[4], cstring) == 0);
    ok(24, vector->strings[4] != cstring);
    ok(25, vector->allocated == 5);
    vector = vector_split(string, 't', false, vector);
    ok(26, vector->count == 3);
    ok(27, vector->allocated == 5);
    ok(28, strcmp(vector->strings[0], "This is a ") == 0);
    ok(29, strcmp(vector->strings[1], "es") == 0);
    ok(30, strcmp(vector->strings[2], ".") == 0);
    ok(31, vector->strings[0] == string);
    ok(32, strcmp(vector_join(vector, "fe"), "This is a feesfe.") == 0);
    vector_free(vector);

    vector = vector_split(empty, ' ', true, NULL);
    ok(33, vector->count == 0);
    vector_free(vector);

    vector = vector_split_whitespace(empty, true, NULL);
    ok(34, vector->count == 0);
    vector_free(vector);

    vector = vector_split(tabs, '\t', true, NULL);
    ok(35, vector->count == 2);
    ok(36, strcmp(vector->strings[0], "test") == 0);
    ok(37, strcmp(vector->strings[1], "ing") == 0);
    ok(38, strcmp(vector_join(vector, ""), "testing") == 0);
    vector_free(vector);

    return 0;
}
