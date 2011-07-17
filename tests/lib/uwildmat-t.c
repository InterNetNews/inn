/*  $Id$
**
**  wildmat test suite.
**
**  As of March 11, 2001, this test suite achieves 100% coverage of the
**  wildmat source code at that time.
*/

#include "clibrary.h"
#include "inn/libinn.h"
#include "libtest.h"

static void
test_r(int n, const char *text, const char *pattern, bool matches)
{
    bool matched;

    matched = uwildmat(text, pattern);
    ok(n, matched == matches);
    if (matched != matches)
        diag("  %s\n  %s\n  expected %d\n", text, pattern, matches);
}

static void
test_p(int n, const char *text, const char *pattern, enum uwildmat matches)
{
    enum uwildmat matched;

    matched = uwildmat_poison(text, pattern);
    ok(n, matched == matches);
    if (matched != matches)
        diag("  %s\n  %s\n  expected %d got %d\n", text, pattern,
             (int) matches, (int) matched);
}

static void
test_s(int n, const char *text, const char *pattern, bool matches)
{
    bool matched;

    matched = uwildmat_simple(text, pattern);
    ok(n, matched == matches);
    if (matched != matches)
        diag("  %s\n  %s\n  expected %d\n", text, pattern, matches);
}

static void
test_v(int n, const char *text, bool matches)
{
    bool matched;

    matched = is_valid_utf8(text);
    ok(n, matched == matches);
    if (matched != matches)
        diag("  %s\n  expected %d\n", text, matches);
}

int
main(void)
{
    test_init(187);

    /* Basic wildmat features. */
    test_r(  1, "foo",            "foo",               true);
    test_r(  2, "foo",            "bar",               false);
    test_r(  3, "",               "",                  true);
    test_r(  4, "foo",            "???",               true);
    test_r(  5, "foo",            "??",                false);
    test_r(  6, "foo",            "*",                 true);
    test_r(  7, "foo",            "f*",                true);
    test_r(  8, "foo",            "*f",                false);
    test_r(  9, "foo",            "*foo*",             true);
    test_r( 10, "foobar",         "*ob*a*r*",          true);
    test_r( 11, "aaaaaaabababab", "*ab",               true);
    test_r( 12, "foo*",           "foo\\*",            true);
    test_r( 13, "foobar",         "foo\\*bar",         false);
    test_r( 14, "\\",             "\\\\",              true);
    test_r( 15, "ball",           "*[al]?",            true);
    test_r( 16, "ten",            "[ten]",             false);
    test_r( 17, "ten",            "**[^te]",           true);
    test_r( 18, "ten",            "**[^ten]",          false);
    test_r( 19, "ten",            "t[a-g]n",           true);
    test_r( 20, "ten",            "t[^a-g]n",          false);
    test_r( 21, "ton",            "t[^a-g]n",          true);
    test_r( 22, "]",              "]",                 true);
    test_r( 23, "a]b",            "a[]]b",             true);
    test_r( 24, "a-b",            "a[]-]b",            true);
    test_r( 25, "a]b",            "a[]-]b",            true);
    test_r( 26, "aab",            "a[]-]b",            false);
    test_r( 27, "aab",            "a[]a-]b",           true);

    /* Multiple and negation. */
    test_r( 28, "foo",            "!foo",              false);
    test_r( 29, "foo",            "!bar",              false);
    test_r( 30, "foo",            "*,!foo",            false);
    test_r( 31, "foo",            "*,!bar",            true);
    test_r( 32, "foo",            "foo,bar",           true);
    test_r( 33, "bar",            "foo,bar",           true);
    test_r( 34, "baz",            "foo,bar",           false);
    test_r( 35, "baz",            "foo,ba?",           true);
    test_r( 36, "",               "!",                 false);
    test_r( 37, "foo",            "!",                 false);
    test_r( 38, "a",              "a,!b,c",            true);
    test_r( 39, "b",              "a,!b,c",            false);
    test_r( 40, "c",              "a,!b,c",            true);
    test_r( 41, "ab",             "a*,!ab",            false);
    test_r( 42, "abc",            "a*,!ab",            true);
    test_r( 43, "dabc",           "a*,!ab",            false);
    test_r( 44, "abc",            "a*,!ab*,abc",       true);
    test_r( 45, "",               ",",                 true);
    test_r( 46, "a",              ",a",                true);
    test_r( 47, "a",              "a,,,",              true);
    test_r( 48, "b",              ",a",                false);
    test_r( 49, "b",              "a,,,",              false);
    test_r( 50, "a,b",            "a\\,b",             true);
    test_r( 51, "a,b",            "a\\\\,b",           false);
    test_r( 52, "a\\",            "a\\\\,b",           true);
    test_r( 53, "a\\,b",          "a\\\\,b",           false);
    test_r( 54, "a\\,b",          "a\\\\\\,b",         true);
    test_r( 55, ",",              "\\,",               true);
    test_r( 56, ",\\",            "\\,",               false);
    test_r( 57, ",\\",            "\\,\\\\,",          true);
    test_r( 58, "",               "\\,\\\\,",          true);
    test_r( 59, "",               "\\,,!",             false);
    test_r( 60, "",               "\\,!,",             true);

    /* Various additional tests. */
    test_r( 61, "acrt",           "a[c-c]st",          false);
    test_r( 62, "]",              "[^]-]",             false);
    test_r( 63, "a",              "[^]-]",             true);
    test_r( 64, "",               "\\",                false);
    test_r( 65, "\\",             "\\",                false);
    test_r( 66, "foo",            "*,@foo",            true);
    test_r( 67, "@foo",           "@foo",              true);
    test_r( 68, "foo",            "@foo",              false);
    test_r( 69, "[ab]",           "\\[ab]",            true);
    test_r( 70, "?a?b",           "\\??\\?b",          true);
    test_r( 71, "abc",            "\\a\\b\\c",         true);

    /* Poison negation. */
    test_p( 72, "abc",            "*",                 UWILDMAT_MATCH);
    test_p( 73, "abc",            "def",               UWILDMAT_FAIL);
    test_p( 74, "abc",            "*,!abc",            UWILDMAT_FAIL);
    test_p( 75, "a",              "*,@a",              UWILDMAT_POISON);
    test_p( 76, "ab",             "*,@a*,ab",          UWILDMAT_MATCH);
    test_p( 77, "ab",             "*,@a**,!ab",        UWILDMAT_FAIL);
    test_p( 78, "@ab",            "\\@ab",             UWILDMAT_MATCH);
    test_p( 79, "@ab",            "@\\@ab",            UWILDMAT_POISON);

    /* UTF-8 characters. */
    test_r( 80, "S\303\256ne",    "S\303\256ne",       true);
    test_r( 81, "S\303\256ne",    "S\303\257ne",       false);
    test_r( 82, "S\303\256ne",    "S?ne",              true);
    test_r( 83, "S\303\256ne",    "S*e",               true);
    test_r( 84, "S\303\256ne",    "S[a-\330\200]ne",   true);
    test_r( 85, "S\303\256ne",    "S[a-\300\256]ne",   false);
    test_r( 86, "S\303\256ne",    "S[^\1-\177]ne",     true);
    test_r( 87, "S\303\256ne",    "S[0\303\256$]ne",   true);
    test_r( 88, "\2",             "[\1-\3]",           true);
    test_r( 89, "\330\277",     "[\330\276-\331\200]", true);
    test_r( 90, "\337\277", "[\337\276-\350\200\200]", true);
    test_r( 91, "\357\277\277", "[\357\277\276-\364\200\200\200]", true);
    test_r( 92, "\357\276\277", "[\357\277\276-\364\200\200\200]", false);
    test_r( 93, "\367\277\277\277",
                    "[\310\231-\372\200\200\200\200]", true);
    test_r( 94, "\373\277\277\277\277",
                      "[\1-\375\200\200\200\200\200]", true);
    test_r( 95, "\375\200\200\200\200\200",
                      "[\5-\375\200\200\200\200\200]", true);
    test_r( 96, "\375\277\277\277\277\276",
           "[\375\277\277\277\277\275-\375\277\277\277\277\277]", true);
    test_r( 97, "b\357\277\277a", "b?a",               true);
    test_r( 98, "b\367\277\277\277a", "b?a",           true);
    test_r( 99, "b\373\277\277\277\277a", "b?a",       true);
    test_r(100, "b\375\277\277\277\277\276a", "b?a",   true);
    test_r(101, "\357\240\275S\313\212\375\206\203\245\260\211",
                                               "????", true);
    test_r(102, "S\303\256ne",    "S\\\303\256ne",     true);
    test_r(103, "s", "[^\330\277-\375\277\277\277\277\277]", true);
    test_r(104, "\367\277\277\277",
               "[^\330\277-\375\277\277\277\277\277]", false);

    /* Malformed UTF-8. */
    test_r(105, "S\303\256ne",    "S?\256ne",          false);
    test_r(106, "\303\303",       "?",                 false);
    test_r(107, "\303\303",       "??",                true);
    test_r(108, "\200",           "[\177-\201]",       true);
    test_r(109, "abc\206d",       "*\206d",            true);
    test_r(110, "\303\206",       "*\206",             false);
    test_r(111, "\40",            "\240",              false);
    test_r(112, "\323",           "[a-\377]",          true);
    test_r(113, "\376\277\277\277\277\277", "?",       false);
    test_r(114, "\376\277\277\277\277\277", "??????",  true);
    test_r(115, "\377\277\277\277\277\277", "?",       false);
    test_r(116, "\377\277\277\277\277\277", "??????",  true);
    test_r(117, "\303\323\206",   "??",                true);
    test_r(118, "\206",           "[\341\206f]",       true);
    test_r(119, "f",              "[\341\206f]",       true);
    test_r(120, "\207",           "[\341\206-\277]",   true);
    test_r(121, "\207",           "[\341\206\206-\277]", false);
    test_r(122, "\300",           "[\277-\341\206]",   true);
    test_r(123, "\206",           "[\277-\341\206]",   true);
    test_r(124, "\341\206",       "[\341\206-\277]?",  true);

    /* Additional tests, including some malformed wildmats. */
    test_r(125, "ab",             "a[]b",              false);
    test_r(126, "a[]b",           "a[]b",              false);
    test_r(127, "ab[",            "ab[",               false);
    test_r(128, "ab",             "[^",                false);
    test_r(129, "ab",             "[-",                false);
    test_r(130, "-",              "[-]",               true);
    test_r(131, "-",              "[a-",               false);
    test_r(132, "-",              "[^a-",              false);
    test_r(133, "-",              "[--A]",             true);
    test_r(134, "5",              "[--A]",             true);
    test_r(135, "\303\206",       "[--A]",             false);
    test_r(136, " ",              "[ --]",             true);
    test_r(137, "$",              "[ --]",             true);
    test_r(138, "-",              "[ --]",             true);
    test_r(139, "0",              "[ --]",             false);
    test_r(140, "-",              "[---]",             true);
    test_r(141, "-",              "[------]",          true);
    test_r(142, "j",              "[a-e-n]",           false);
    test_r(143, "a",              "[^------]",         true);
    test_r(144, "[",              "[]-a]",             false);
    test_r(145, "^",              "[]-a]",             true);
    test_r(146, "^",              "[^]-a]",            false);
    test_r(147, "[",              "[^]-a]",            true);
    test_r(148, "^",              "[a^bc]",            true);
    test_r(149, "-b]",            "[a-]b]",            true);
    test_r(150, "\\]",            "[\\]]",             true);
    test_r(151, "]",              "[\\-^]",            true);
    test_r(152, "[",              "[\\-^]",            false);
    test_r(153, "G",              "[A-\\]",            true);
    test_r(154, "aaabbb",         "b*a",               false);
    test_r(155, "aabcaa",         "*ba*",              false);
    test_r(156, ",",              "[,]",               true);
    test_r(157, ",",              "[\\,]",             true);
    test_r(158, "\\",             "[\\,]",             true);
    test_r(159, "-",              "[,-.]",             true);
    test_r(160, "+",              "[,-.]",             false);
    test_r(161, "-.]",            "[,-.]",             false);

    /* Tests for the wildmat_simple interface. */
    test_s(162, "ab,cd",          "ab,cd",             true);
    test_s(163, "ab",             "ab,cd",             false);
    test_s(164, "!aaabbb",        "!a*b*",             true);
    test_s(165, "ccc",            "*,!a*",             false);
    test_s(166, "foo",            "*",                 true);

    /* Combinations of * and (possibly invalid) UTF-8 characters. */
    test_r(167, "\303\206",       "*[^\303\206]",      false);
    test_r(168, "\303\206",       "\303*",             true);
    test_r(169, "\303\206",       "*\206",             false);
    test_r(170, "\357\277\277",   "*\277",             false);
    test_r(171, "\357\277\277",   "\357*",             true);
    test_r(172, "\303\206",       "*[\206]",           false);
    test_r(173, "\303\206\303\206",
                                  "*[^\303\206]",      false);
    test_r(174, "\303\206\357\277\277",
                                  "*[^\303\206]",      true);

    /* Tests for the is_valid_utf8 interface. */
    test_v(175, "a",                                   true);
    test_v(176, "aaabbb",                              true);
    test_v(177, "test\303\251\302\240!",               true);
    test_v(178, "\200",                                false);
    test_v(179, "\277",                                false);
    test_v(180, "\300 ",                               false);
    test_v(181, "\340\277",                            false);
    test_v(182, "\374\277\277\277\277",                false);
    test_v(183, "\374\277\277\277\277\277",            true);
    test_v(184, "a\303\251b\303\251c\374\277\277\277\277\277",
                                                       true);
    test_v(185, "a\303\251b\303c\374\277\277\277\277\277",
                                                       false);
    test_v(186, "",                                    true);
    test_v(187, "a\303\251b\303\0c",                   false);
    
    return 0;
}
