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
    struct cvector *cvector;
    const char cstring[] = "This is a\ttest.  ";
    const char tabs[] = "test\t\ting\t";
    char empty[] = "";
    char *string;
    char *p;

    puts("81");

    vector = vector_new();
    ok(1, vector != NULL);
    vector_add(vector, cstring);
    ok(2, vector->count == 1);
    ok(3, vector->strings[0] != cstring);
    vector_resize(vector, 4);
    ok(4, vector->allocated == 4);
    vector_add(vector, cstring);
    vector_add(vector, cstring);
    vector_add(vector, cstring);
    ok(5, vector->allocated == 4);
    ok(6, vector->count == 4);
    ok(7, vector->strings[1] != vector->strings[2]);
    ok(8, vector->strings[2] != vector->strings[3]);
    ok(9, vector->strings[3] != vector->strings[0]);
    ok(10, vector->strings[0] != cstring);
    vector_clear(vector);
    ok(11, vector->count == 0);
    ok(12, vector->allocated == 4);
    string = xstrdup(cstring);
    vector_add(vector, cstring);
    vector_add(vector, string);
    ok(13, vector->count == 2);
    ok(14, vector->strings[1] != string);
    vector_resize(vector, 1);
    ok(15, vector->count == 1);
    ok(16, vector->strings[0] != cstring);
    vector_free(vector);
    free(string);

    cvector = cvector_new();
    ok(17, cvector != NULL);
    cvector_add(cvector, cstring);
    ok(18, cvector->count == 1);
    ok(19, cvector->strings[0] == cstring);
    cvector_resize(cvector, 4);
    ok(20, cvector->allocated == 4);
    cvector_add(cvector, cstring);
    cvector_add(cvector, cstring);
    cvector_add(cvector, cstring);
    ok(21, cvector->allocated == 4);
    ok(22, cvector->count == 4);
    ok(23, cvector->strings[1] == cvector->strings[2]);
    ok(24, cvector->strings[2] == cvector->strings[3]);
    ok(25, cvector->strings[3] == cvector->strings[0]);
    ok(26, cvector->strings[0] == cstring);
    cvector_clear(cvector);
    ok(27, cvector->count == 0);
    ok(28, cvector->allocated == 4);
    string = xstrdup(cstring);
    cvector_add(cvector, cstring);
    cvector_add(cvector, string);
    ok(29, cvector->count == 2);
    ok(30, cvector->strings[1] == string);
    cvector_resize(cvector, 1);
    ok(31, cvector->count == 1);
    ok(32, cvector->strings[0] == cstring);
    cvector_free(cvector);
    free(string);

    vector = vector_split_space("This is a\ttest.  ", NULL);
    ok(33, vector->count == 4);
    ok(34, vector->allocated = 4);
    ok(35, strcmp(vector->strings[0], "This") == 0);
    ok(36, strcmp(vector->strings[1], "is") == 0);
    ok(37, strcmp(vector->strings[2], "a") == 0);
    ok(38, strcmp(vector->strings[3], "test.") == 0);
    vector_add(vector, cstring);
    ok(39, strcmp(vector->strings[4], cstring) == 0);
    ok(40, vector->strings[4] != cstring);
    ok(41, vector->allocated == 5);
    vector = vector_split(cstring, 't', vector);
    ok(42, vector->count == 3);
    ok(43, vector->allocated == 5);
    ok(44, strcmp(vector->strings[0], "This is a\t") == 0);
    ok(45, strcmp(vector->strings[1], "es") == 0);
    ok(46, strcmp(vector->strings[2], ".  ") == 0);
    ok(47, vector->strings[0] != string);
    p = vector_join(vector, "fe");
    ok(48, strcmp(p, "This is a\tfeesfe.  ") == 0);
    free(p);
    vector_free(vector);

    string = xstrdup(cstring);
    cvector = cvector_split_space(string, NULL);
    ok(49, cvector->count == 4);
    ok(50, cvector->allocated = 4);
    ok(51, strcmp(cvector->strings[0], "This") == 0);
    ok(52, strcmp(cvector->strings[1], "is") == 0);
    ok(53, strcmp(cvector->strings[2], "a") == 0);
    ok(54, strcmp(cvector->strings[3], "test.") == 0);
    ok(55, memcmp(string, "This\0is\0a\0test.", 16) == 0);
    cvector_add(cvector, cstring);
    ok(56, cvector->strings[4] == cstring);
    ok(57, cvector->allocated == 5);
    free(string);
    string = xstrdup(cstring);
    cvector = cvector_split(string, 't', cvector);
    ok(58, cvector->count == 3);
    ok(59, cvector->allocated == 5);
    ok(60, strcmp(cvector->strings[0], "This is a\t") == 0);
    ok(61, strcmp(cvector->strings[1], "es") == 0);
    ok(62, strcmp(cvector->strings[2], ".  ") == 0);
    ok(63, cvector->strings[0] == string);
    ok(64, memcmp(string, "This is a\t\0es\0.  ", 18) == 0);
    p = cvector_join(cvector, "oo");
    ok(65, strcmp(p, "This is a\tooesoo.  ") == 0);
    free(p);
    cvector_free(cvector);
    free(string);

    vector = vector_split("", ' ', NULL);
    ok(66, vector->count == 0);
    vector_free(vector);
    cvector = cvector_split(empty, ' ', NULL);
    ok(67, cvector->count == 0);
    cvector_free(cvector);

    vector = vector_split_space("", NULL);
    ok(68, vector->count == 0);
    vector_free(vector);
    cvector = cvector_split_space(empty, NULL);
    ok(69, cvector->count == 0);
    cvector_free(cvector);

    vector = vector_split(tabs, '\t', NULL);
    ok(70, vector->count == 2);
    ok(71, strcmp(vector->strings[0], "test") == 0);
    ok(72, strcmp(vector->strings[1], "ing") == 0);
    p = vector_join(vector, "");
    ok(73, strcmp(p, "testing") == 0);
    free(p);
    vector_free(vector);

    string = xstrdup(tabs);
    cvector = cvector_split(string, '\t', NULL);
    ok(74, cvector->count == 2);
    ok(75, strcmp(cvector->strings[0], "test") == 0);
    ok(76, strcmp(cvector->strings[1], "ing") == 0);
    p = cvector_join(cvector, "");
    ok(77, strcmp(p, "testing") == 0);
    free(p);
    cvector_free(cvector);
    free(string);

    vector = vector_split_space("foo\nbar", NULL);
    ok(78, vector->count == 1);
    ok(79, strcmp(vector->strings[0], "foo\nbar") == 0);
    vector_free(vector);

    string = xstrdup("foo\nbar");
    cvector = cvector_split_space(string, NULL);
    ok(80, cvector->count == 1);
    ok(81, strcmp(cvector->strings[0], "foo\nbar") == 0);
    cvector_free(cvector);

    return 0;
}
