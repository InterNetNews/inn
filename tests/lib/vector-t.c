/* $Id$ */
/* vector test suite. */

#include "config.h"
#include "clibrary.h"

#include "inn/vector.h"
#include "libinn.h"
#include "libtest.h"

int
main(void)
{
    struct vector *vector;
    struct cvector *cvector;
    const char cstring[] = "This is a\ttest.  ";
    const char tabs[] = "test\t\ting\t";
    static const char nulls1[] = "This\0is\0a\0test.";
    static const char nulls2[] = "This is a\t\0es\0.  ";
    char empty[] = "";
    char *string;
    char *p;

    test_init(87);

    vector = vector_new();
    ok(1, vector != NULL);
    vector_add(vector, cstring);
    ok_int(2, 1, vector->count);
    ok(3, vector->strings[0] != cstring);
    vector_resize(vector, 4);
    ok_int(4, 4, vector->allocated);
    vector_add(vector, cstring);
    vector_add(vector, cstring);
    vector_add(vector, cstring);
    ok_int(5, 4, vector->allocated);
    ok_int(6, 4, vector->count);
    ok(7, vector->strings[1] != vector->strings[2]);
    ok(8, vector->strings[2] != vector->strings[3]);
    ok(9, vector->strings[3] != vector->strings[0]);
    ok(10, vector->strings[0] != cstring);
    vector_clear(vector);
    ok_int(11, 0, vector->count);
    ok_int(12, 4, vector->allocated);
    string = xstrdup(cstring);
    vector_add(vector, cstring);
    vector_add(vector, string);
    ok_int(13, 2, vector->count);
    ok(14, vector->strings[1] != string);
    vector_resize(vector, 1);
    ok_int(15, 1, vector->count);
    ok(16, vector->strings[0] != cstring);
    vector_free(vector);
    free(string);

    cvector = cvector_new();
    ok(17, cvector != NULL);
    cvector_add(cvector, cstring);
    ok_int(18, 1, cvector->count);
    ok(19, cvector->strings[0] == cstring);
    cvector_resize(cvector, 4);
    ok_int(20, 4, cvector->allocated);
    cvector_add(cvector, cstring);
    cvector_add(cvector, cstring);
    cvector_add(cvector, cstring);
    ok_int(21, 4, cvector->allocated);
    ok_int(22, 4, cvector->count);
    ok(23, cvector->strings[1] == cvector->strings[2]);
    ok(24, cvector->strings[2] == cvector->strings[3]);
    ok(25, cvector->strings[3] == cvector->strings[0]);
    ok(26, cvector->strings[0] == cstring);
    cvector_clear(cvector);
    ok_int(27, 0, cvector->count);
    ok_int(28, 4, cvector->allocated);
    string = xstrdup(cstring);
    cvector_add(cvector, cstring);
    cvector_add(cvector, string);
    ok_int(29, 2, cvector->count);
    ok(30, cvector->strings[1] == string);
    cvector_resize(cvector, 1);
    ok_int(31, 1, cvector->count);
    ok(32, cvector->strings[0] == cstring);
    cvector_free(cvector);
    free(string);

    vector = vector_split_space("This is a\ttest.  ", NULL);
    ok_int(33, 4, vector->count);
    ok_int(34, 4, vector->allocated);
    ok_string(35, "This", vector->strings[0]);
    ok_string(36, "is", vector->strings[1]);
    ok_string(37, "a", vector->strings[2]);
    ok_string(38, "test.", vector->strings[3]);
    vector_add(vector, cstring);
    ok_string(39, cstring, vector->strings[4]);
    ok(40, vector->strings[4] != cstring);
    ok_int(41, 5, vector->allocated);
    vector = vector_split(cstring, 't', vector);
    ok_int(42, 3, vector->count);
    ok_int(43, 5, vector->allocated);
    ok_string(44, "This is a\t", vector->strings[0]);
    ok_string(45, "es", vector->strings[1]);
    ok_string(46, ".  ", vector->strings[2]);
    ok(47, vector->strings[0] != cstring);
    p = vector_join(vector, "fe");
    ok_string(48, "This is a\tfeesfe.  ", p);
    free(p);
    vector_free(vector);

    string = xstrdup(cstring);
    cvector = cvector_split_space(string, NULL);
    ok_int(49, 4, cvector->count);
    ok_int(50, 4, cvector->allocated);
    ok_string(51, "This", cvector->strings[0]);
    ok_string(52, "is", cvector->strings[1]);
    ok_string(53, "a", cvector->strings[2]);
    ok_string(54, "test.", cvector->strings[3]);
    ok(55, memcmp(string, nulls1, 16) == 0);
    cvector_add(cvector, cstring);
    ok(56, cvector->strings[4] == cstring);
    ok_int(57, 5, cvector->allocated);
    free(string);
    string = xstrdup(cstring);
    cvector = cvector_split(string, 't', cvector);
    ok_int(58, 3, cvector->count);
    ok_int(59, 5, cvector->allocated);
    ok_string(60, "This is a\t", cvector->strings[0]);
    ok_string(61, "es", cvector->strings[1]);
    ok_string(62, ".  ", cvector->strings[2]);
    ok(63, cvector->strings[0] == string);
    ok(64, memcmp(string, nulls2, 18) == 0);
    p = cvector_join(cvector, "oo");
    ok_string(65, "This is a\tooesoo.  ", p);
    free(p);
    cvector_free(cvector);
    free(string);

    vector = vector_split("", ' ', NULL);
    ok_int(66, 1, vector->count);
    ok_string(67, "", vector->strings[0]);
    vector_free(vector);
    cvector = cvector_split(empty, ' ', NULL);
    ok_int(68, 1, cvector->count);
    ok_string(69, "", vector->strings[0]);
    cvector_free(cvector);

    vector = vector_split_space("", NULL);
    ok_int(70, 0, vector->count);
    vector_free(vector);
    cvector = cvector_split_space(empty, NULL);
    ok_int(71, 0, cvector->count);
    cvector_free(cvector);

    vector = vector_split(tabs, '\t', NULL);
    ok_int(72, 4, vector->count);
    ok_string(73, "test", vector->strings[0]);
    ok_string(74, "", vector->strings[1]);
    ok_string(75, "ing", vector->strings[2]);
    ok_string(76, "", vector->strings[3]);
    p = vector_join(vector, "");
    ok_string(77, "testing", p);
    free(p);
    vector_free(vector);

    string = xstrdup(tabs);
    cvector = cvector_split(string, '\t', NULL);
    ok_int(78, 4, cvector->count);
    ok_string(79, "test", cvector->strings[0]);
    ok_string(80, "", cvector->strings[1]);
    ok_string(81, "ing", cvector->strings[2]);
    ok_string(82, "", cvector->strings[3]);
    p = cvector_join(cvector, "");
    ok_string(83, "testing", p);
    free(p);
    cvector_free(cvector);
    free(string);

    vector = vector_split_space("foo\nbar", NULL);
    ok_int(84, 1, vector->count);
    ok_string(85, "foo\nbar", vector->strings[0]);
    vector_free(vector);

    string = xstrdup("foo\nbar");
    cvector = cvector_split_space(string, NULL);
    ok_int(86, 1, cvector->count);
    ok_string(87, "foo\nbar", cvector->strings[0]);
    cvector_free(cvector);

    return 0;
}
