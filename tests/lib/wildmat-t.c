/*  $Id$
**
**  wildmat test suite.
**
**  As of March 11, 2001, this test suite achieves 100% coverage of the
**  wildmat source code at that time.
*/

#include "clibrary.h"
#include "libinn.h"

static void
ok_reg(int n, const char *text, const char *pattern, bool matches)
{
    bool matched;

    matched = wildmat(text, pattern);
    printf("%sok %d\n", matched == matches ? "" : "not ", n);
    if (matched != matches)
        printf("  %s\n  %s\n  expected %d\n", text, pattern, matches);
}

static void
ok_poi(int n, const char *text, const char *pattern, enum wildmat matches)
{
    enum wildmat matched;

    matched = wildmat_poison(text, pattern);
    printf("%sok %d\n", matched == matches ? "" : "not ", n);
    if (matched != matches)
        printf("  %s\n  %s\n  expected %d got %d\n", text, pattern,
               (int) matches, (int) matched);
}

static void
ok_sim(int n, const char *text, const char *pattern, bool matches)
{
    bool matched;

    matched = wildmat_simple(text, pattern);
    printf("%sok %d\n", matched == matches ? "" : "not ", n);
    if (matched != matches)
        printf("  %s\n  %s\n  expected %d\n", text, pattern, matches);
}

int
main(void)
{
    puts("166");

    /* Basic wildmat features. */
    ok_reg(  1, "foo",            "foo",               true);
    ok_reg(  2, "foo",            "bar",               false);
    ok_reg(  3, "",               "",                  true);
    ok_reg(  4, "foo",            "???",               true);
    ok_reg(  5, "foo",            "??",                false);
    ok_reg(  6, "foo",            "*",                 true);
    ok_reg(  7, "foo",            "f*",                true);
    ok_reg(  8, "foo",            "*f",                false);
    ok_reg(  9, "foo",            "*foo*",             true);
    ok_reg( 10, "foobar",         "*ob*a*r*",          true);
    ok_reg( 11, "aaaaaaabababab", "*ab",               true);
    ok_reg( 12, "foo*",           "foo\\*",            true);
    ok_reg( 13, "foobar",         "foo\\*bar",         false);
    ok_reg( 14, "\\",             "\\\\",              true);
    ok_reg( 15, "ball",           "*[al]?",            true);
    ok_reg( 16, "ten",            "[ten]",             false);
    ok_reg( 17, "ten",            "**[^te]",           true);
    ok_reg( 18, "ten",            "**[^ten]",          false);
    ok_reg( 19, "ten",            "t[a-g]n",           true);
    ok_reg( 20, "ten",            "t[^a-g]n",          false);
    ok_reg( 21, "ton",            "t[^a-g]n",          true);
    ok_reg( 22, "]",              "]",                 true);
    ok_reg( 23, "a]b",            "a[]]b",             true);
    ok_reg( 24, "a-b",            "a[]-]b",            true);
    ok_reg( 25, "a]b",            "a[]-]b",            true);
    ok_reg( 26, "aab",            "a[]-]b",            false);
    ok_reg( 27, "aab",            "a[]a-]b",           true);

    /* Multiple and negation. */
    ok_reg( 28, "foo",            "!foo",              false);
    ok_reg( 29, "foo",            "!bar",              false);
    ok_reg( 30, "foo",            "*,!foo",            false);
    ok_reg( 31, "foo",            "*,!bar",            true);
    ok_reg( 32, "foo",            "foo,bar",           true);
    ok_reg( 33, "bar",            "foo,bar",           true);
    ok_reg( 34, "baz",            "foo,bar",           false);
    ok_reg( 35, "baz",            "foo,ba?",           true);
    ok_reg( 36, "",               "!",                 false);
    ok_reg( 37, "foo",            "!",                 false);
    ok_reg( 38, "a",              "a,!b,c",            true);
    ok_reg( 39, "b",              "a,!b,c",            false);
    ok_reg( 40, "c",              "a,!b,c",            true);
    ok_reg( 41, "ab",             "a*,!ab",            false);
    ok_reg( 42, "abc",            "a*,!ab",            true);
    ok_reg( 43, "dabc",           "a*,!ab",            false);
    ok_reg( 44, "abc",            "a*,!ab*,abc",       true);
    ok_reg( 45, "",               ",",                 true);
    ok_reg( 46, "a",              ",a",                true);
    ok_reg( 47, "a",              "a,,,",              true);
    ok_reg( 48, "b",              ",a",                false);
    ok_reg( 49, "b",              "a,,,",              false);
    ok_reg( 50, "a,b",            "a\\,b",             true);
    ok_reg( 51, "a,b",            "a\\\\,b",           false);
    ok_reg( 52, "a\\",            "a\\\\,b",           true);
    ok_reg( 53, "a\\,b",          "a\\\\,b",           false);
    ok_reg( 54, "a\\,b",          "a\\\\\\,b",         true);
    ok_reg( 55, ",",              "\\,",               true);
    ok_reg( 56, ",\\",            "\\,",               false);
    ok_reg( 57, ",\\",            "\\,\\\\,",          true);
    ok_reg( 58, "",               "\\,\\\\,",          true);
    ok_reg( 59, "",               "\\,,!",             false);
    ok_reg( 60, "",               "\\,!,",             true);

    /* Various additional tests. */
    ok_reg( 61, "acrt",           "a[c-c]st",          false);
    ok_reg( 62, "]",              "[^]-]",             false);
    ok_reg( 63, "a",              "[^]-]",             true);
    ok_reg( 64, "",               "\\",                false);
    ok_reg( 65, "\\",             "\\",                false);
    ok_reg( 66, "foo",            "*,@foo",            true);
    ok_reg( 67, "@foo",           "@foo",              true);
    ok_reg( 68, "foo",            "@foo",              false);
    ok_reg( 69, "[ab]",           "\\[ab]",            true);
    ok_reg( 70, "?a?b",           "\\??\\?b",          true);
    ok_reg( 71, "abc",            "\\a\\b\\c",         true);

    /* Poison negation. */
    ok_poi( 72, "abc",            "*",                 WILDMAT_MATCH);
    ok_poi( 73, "abc",            "def",               WILDMAT_FAIL);
    ok_poi( 74, "abc",            "*,!abc",            WILDMAT_FAIL);
    ok_poi( 75, "a",              "*,@a",              WILDMAT_POISON);
    ok_poi( 76, "ab",             "*,@a*,ab",          WILDMAT_MATCH);
    ok_poi( 77, "ab",             "*,@a**,!ab",        WILDMAT_FAIL);
    ok_poi( 78, "@ab",            "\\@ab",             WILDMAT_MATCH);
    ok_poi( 79, "@ab",            "@\\@ab",            WILDMAT_POISON);

    /* UTF-8 characters. */
    ok_reg( 80, "S\303\256ne",    "S\303\256ne",       true);
    ok_reg( 81, "S\303\256ne",    "S\303\257ne",       false);
    ok_reg( 82, "S\303\256ne",    "S?ne",              true);
    ok_reg( 83, "S\303\256ne",    "S*e",               true);
    ok_reg( 84, "S\303\256ne",    "S[a-\330\200]ne",   true);
    ok_reg( 85, "S\303\256ne",    "S[a-\300\256]ne",   false);
    ok_reg( 86, "S\303\256ne",    "S[^\1-\177]ne",     true);
    ok_reg( 87, "S\303\256ne",    "S[0\303\256$]ne",   true);
    ok_reg( 88, "\2",             "[\1-\3]",           true);
    ok_reg( 89, "\330\277",     "[\330\276-\331\200]", true);
    ok_reg( 90, "\337\277", "[\337\276-\350\200\200]", true);
    ok_reg( 91, "\357\277\277", "[\357\277\276-\364\200\200\200]", true);
    ok_reg( 92, "\357\276\277", "[\357\277\276-\364\200\200\200]", false);
    ok_reg( 93, "\367\277\277\277",
                    "[\310\231-\372\200\200\200\200]", true);
    ok_reg( 94, "\373\277\277\277\277",
                      "[\1-\375\200\200\200\200\200]", true);
    ok_reg( 95, "\375\200\200\200\200\200",
                      "[\5-\375\200\200\200\200\200]", true);
    ok_reg( 96, "\375\277\277\277\277\276",
           "[\375\277\277\277\277\275-\375\277\277\277\277\277]", true);
    ok_reg( 97, "b\357\277\277a", "b?a",               true);
    ok_reg( 98, "b\367\277\277\277a", "b?a",           true);
    ok_reg( 99, "b\373\277\277\277\277a", "b?a",       true);
    ok_reg(100, "b\375\277\277\277\277\276a", "b?a",   true);
    ok_reg(101, "\357\240\275S\313\212\375\206\203\245\260\211",
                                               "????", true);
    ok_reg(102, "S\303\256ne",    "S\\\303\256ne",     true);
    ok_reg(103, "s", "[^\330\277-\375\277\277\277\277\277]", true);
    ok_reg(104, "\367\277\277\277",
               "[^\330\277-\375\277\277\277\277\277]", false);

    /* Malformed UTF-8. */
    ok_reg(105, "S\303\256ne",    "S?\256ne",          false);
    ok_reg(106, "\303\303",       "?",                 false);
    ok_reg(107, "\303\303",       "??",                true);
    ok_reg(108, "\200",           "[\177-\201]",       true);
    ok_reg(109, "abc\206d",       "*\206d",            true);
    ok_reg(110, "\303\206",       "*\206",             true);
    ok_reg(111, "\40",            "\240",              false);
    ok_reg(112, "\323",           "[a-\377]",          true);
    ok_reg(113, "\376\277\277\277\277\277", "?",       false);
    ok_reg(114, "\376\277\277\277\277\277", "??????",  true);
    ok_reg(115, "\377\277\277\277\277\277", "?",       false);
    ok_reg(116, "\377\277\277\277\277\277", "??????",  true);
    ok_reg(117, "\303\323\206",   "??",                true);
    ok_reg(118, "\206",           "[\341\206f]",       true);
    ok_reg(119, "f",              "[\341\206f]",       true);
    ok_reg(120, "\207",           "[\341\206-\277]",   true);
    ok_reg(121, "\207",           "[\341\206\206-\277]", false);
    ok_reg(122, "\300",           "[\277-\341\206]",   true);
    ok_reg(123, "\206",           "[\277-\341\206]",   true);
    ok_reg(124, "\341\206",       "[\341\206-\277]?",  true);

    /* Additional tests, including some malformed wildmats. */
    ok_reg(125, "ab",             "a[]b",              false);
    ok_reg(126, "a[]b",           "a[]b",              false);
    ok_reg(127, "ab[",            "ab[",               false);
    ok_reg(128, "ab",             "[^",                false);
    ok_reg(129, "ab",             "[-",                false);
    ok_reg(130, "-",              "[-]",               true);
    ok_reg(131, "-",              "[a-",               false);
    ok_reg(132, "-",              "[^a-",              false);
    ok_reg(133, "-",              "[--A]",             true);
    ok_reg(134, "5",              "[--A]",             true);
    ok_reg(135, "\303\206",       "[--A]",             false);
    ok_reg(136, " ",              "[ --]",             true);
    ok_reg(137, "$",              "[ --]",             true);
    ok_reg(138, "-",              "[ --]",             true);
    ok_reg(139, "0",              "[ --]",             false);
    ok_reg(140, "-",              "[---]",             true);
    ok_reg(141, "-",              "[------]",          true);
    ok_reg(142, "j",              "[a-e-n]",           false);
    ok_reg(143, "a",              "[^------]",         true);
    ok_reg(144, "[",              "[]-a]",             false);
    ok_reg(145, "^",              "[]-a]",             true);
    ok_reg(146, "^",              "[^]-a]",            false);
    ok_reg(147, "[",              "[^]-a]",            true);
    ok_reg(148, "^",              "[a^bc]",            true);
    ok_reg(149, "-b]",            "[a-]b]",            true);
    ok_reg(150, "\\]",            "[\\]]",             true);
    ok_reg(151, "]",              "[\\-^]",            true);
    ok_reg(152, "[",              "[\\-^]",            false);
    ok_reg(153, "G",              "[A-\\]",            true);
    ok_reg(154, "aaabbb",         "b*a",               false);
    ok_reg(155, "aabcaa",         "*ba*",              false);
    ok_reg(156, ",",              "[,]",               true);
    ok_reg(157, ",",              "[\\,]",             true);
    ok_reg(158, "\\",             "[\\,]",             true);
    ok_reg(159, "-",              "[,-.]",             true);
    ok_reg(160, "+",              "[,-.]",             false);
    ok_reg(161, "-.]",            "[,-.]",             false);

    /* Tests for the wildmat_simple interface. */
    ok_sim(162, "ab,cd",          "ab,cd",             true);
    ok_sim(163, "ab",             "ab,cd",             false);
    ok_sim(164, "!aaabbb",        "!a*b*",             true);
    ok_sim(165, "ccc",            "*,!a*",             false);
    ok_sim(166, "foo",            "*",                 true);

    return 0;
}
