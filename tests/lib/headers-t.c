/*
** headers test suite.
*/

#define LIBTEST_NEW_FORMAT 1

#include "inn/libinn.h"
#include "tap/basic.h"

int
main(void)
{
    plan(9+3+11+8+14+7);

    ok(!IsValidHeaderName(NULL), "bad header field name 1");
    ok(!IsValidHeaderName(""), "bad header field name 2");
    ok(!IsValidHeaderName(":"), "bad header field name 3");
    ok(!IsValidHeaderName("Sub:ject"), "bad header field name 4");
    ok(!IsValidHeaderName("Subject:"), "bad header field name 5");
    /* \177 (octal notation) is DEL. */
    ok(!IsValidHeaderName("\177Subject"), "bad header field name 6");
    ok(!IsValidHeaderName("Sub ject"), "bad header field name 7");
    ok(!IsValidHeaderName("Sub\tject"), "bad header field name 8");
    ok(!IsValidHeaderName("Sub\r\nject"), "bad header field name 9");

    ok(IsValidHeaderName("Subject"), "good header field name 1");
    ok(IsValidHeaderName("subJECT"), "good header field name 2");
    ok(IsValidHeaderName("X-#%-T`?!"), "good header field name 3");

    ok(!IsValidHeaderBody(NULL), "bad header field body 1");
    ok(!IsValidHeaderBody(""), "bad header field body 2");
    ok(!IsValidHeaderBody("a\177b"), "bad header field body 3");
    ok(!IsValidHeaderBody("a\r\nb"), "bad header field body 4");
    ok(!IsValidHeaderBody("a\nb"), "bad header field body 5");
    ok(!IsValidHeaderBody("\n"), "bad header field body 6");
    ok(!IsValidHeaderBody("\r\n b"), "bad header field body 7");
    ok(!IsValidHeaderBody("a\r\n b\r\n"), "bad header field body 8");
    ok(!IsValidHeaderBody("a\n\tb\n \t\n c"), "bad header field body 9");
    ok(!IsValidHeaderBody("a\003b"), "bad header field body 10");
    ok(!IsValidHeaderBody("a\r b"), "bad header field body 11");

    ok(IsValidHeaderBody(":"), "good header field body 1");
    ok(IsValidHeaderBody("a b"), "good header field body 2");
    ok(IsValidHeaderBody("a\t\tb"), "good header field body 3");
    ok(IsValidHeaderBody("a\r\n b"), "good header field body 4");
    ok(IsValidHeaderBody("a\r\n\tb"), "good header field body 5");
    ok(IsValidHeaderBody("a\n   b"), "good header field body 6");
    ok(IsValidHeaderBody("a\n\tb\n \tc\n d"), "good header field body 7");
    ok(IsValidHeaderBody("\317\205\317\204\317\2068"),
       "good header field body 8");

    ok(!IsValidHeaderField(NULL), "bad header field 1");
    ok(!IsValidHeaderField(""), "bad header field 2");
    ok(!IsValidHeaderField(":"), "bad header field 3");
    ok(!IsValidHeaderField("Subject"), "bad header field 4");
    ok(!IsValidHeaderField("Subject:"), "bad header field 5");
    ok(!IsValidHeaderField("Sub:ject"), "bad header field 6");
    ok(!IsValidHeaderField("Subject: "), "bad header field 7");
    ok(!IsValidHeaderField("Subject:\ta"), "bad header field 8");
    ok(!IsValidHeaderField("Subject: a\n"), "bad header field 9");
    ok(!IsValidHeaderField("\177Subject: a"), "bad header field 10");
    ok(!IsValidHeaderField("Subject: a\177b"), "bad header field 11");
    ok(!IsValidHeaderField("Subject: a\nb"), "bad header field 12");
    ok(!IsValidHeaderField("UT\317\2068: a"), "bad header field 13");
    ok(!IsValidHeaderField("Control\004: a"), "bad header field 14");

    ok(IsValidHeaderField("Subject: a"), "good header field 1");
    ok(IsValidHeaderField("Subject: a\n\tb"), "good header field 2");
    ok(IsValidHeaderField("Sub: ject"), "good header field 3");
    ok(IsValidHeaderField("X-#%-T`?!: yeah"), "good header field 4");
    ok(IsValidHeaderField("Subject: a\r\n\tb"), "good header field 5");
    ok(IsValidHeaderField("Newsgroups: local.\317\205\317\204\317\2068"),
       "good header field 6");
    ok(IsValidHeaderField("Subject: \317\205\317\204\317\2068\r\n testing"),
       "good header field 7");

    return 0;
}
