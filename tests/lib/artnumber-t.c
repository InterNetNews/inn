/*
** IsValidArticleNumber & IsValidRange tests
*/

#define LIBTEST_NEW_FORMAT 1

#include "portable/system.h"

#include "inn/libinn.h"
#include "tap/basic.h"
#include <assert.h>

static void
testIsValidArticleNumber(void)
{
    is_bool(false, IsValidArticleNumber(""), "empty article number");
    is_bool(false, IsValidArticleNumber(" "), "only whitespace");
    is_bool(false, IsValidArticleNumber(" 1"), "leading whitespace");
    is_bool(false, IsValidArticleNumber("1 "), "trailing whitespace");
    is_bool(false, IsValidArticleNumber("wot"), "only non-digits");
    is_bool(false, IsValidArticleNumber("wot123"), "leading non-digits");
    is_bool(false, IsValidArticleNumber("123wot"), "trailing non-digits");
    is_bool(false, IsValidArticleNumber("00000000000000001"), "too long");
    is_bool(false, IsValidArticleNumber("2147483648"), "too big");
    is_bool(false, IsValidArticleNumber("-1"), "negative");

    is_bool(true, IsValidArticleNumber("0"), "zero");
    is_bool(true, IsValidArticleNumber("1"), "one");
    is_bool(true, IsValidArticleNumber("2147483647"), "maximum value");
    is_bool(true, IsValidArticleNumber("0000000000000001"), "maximum length");
}

/* IsValidRange mutates its argument, so we must wrap it. */
static int wrap_IsValidRange(const char *str) {
    char buffer[256];
    assert(strlen(str) < sizeof buffer);
    strlcpy(buffer, str, sizeof buffer);
    return IsValidRange(buffer);
}

static void
testIsValidRange(void)
{
    is_bool(false, wrap_IsValidRange(""), "empty article range");
    is_bool(false, wrap_IsValidRange("--"), "two dashes alone");
    is_bool(false, wrap_IsValidRange("1 - 2"), "forbidden whitespace");
    is_bool(false, wrap_IsValidRange("1-2-3"), "multiple bounds");
    is_bool(false, wrap_IsValidRange("1--2"), "two dashes");
    is_bool(false, wrap_IsValidRange("1-2-"), "excess trailing dash");
    is_bool(false, wrap_IsValidRange("-1-2"), "excess leading dash");

    is_bool(true, wrap_IsValidRange("-"), "single dash");
    is_bool(true, wrap_IsValidRange("1-"), "unbounded above");
    is_bool(true, wrap_IsValidRange("-2147483647"), "unbounded below");
    is_bool(true, wrap_IsValidRange("2-99"), "fully bounded");
    is_bool(true, wrap_IsValidRange("99-2"), "reverse bounds"); /* explicitly countenanced by RFC3977. */
}


int
main(void)
{
    plan(14+12);

    testIsValidArticleNumber();
    testIsValidRange();

    return 0;
}
