/*
**  Test suite for message identifiersi.
**
**  Written by Julien Élie in 2017.
**
**  Various bug fixes, code and documentation improvements since then
**  in 2017, 2021, 2023, 2026.
*/

#define LIBTEST_NEW_FORMAT 1

#include "inn/libinn.h"
#include "tap/basic.h"

void testDomains(void);
void testMessageIDs(bool stripspaces, bool laxsyntax);
void testStrippingSpaces(bool stripspaces);


/*
**  Test a bunch of message-IDs that are either bad or good depending on
**  how lax the syntax is.
*/
void
testMessageIDs(bool strip, bool lax)
{
    /* Always invalid, no matter how lax the syntax is. */
    is_bool(false, IsValidMessageID(NULL, strip, lax), "always bad ID 1");
    is_bool(false, IsValidMessageID("", strip, lax), "always bad ID 2");
    is_bool(false, IsValidMessageID("<invalid@test", strip, lax),
            "always bad ID 3");
    is_bool(false, IsValidMessageID("invalid@test>", strip, lax),
            "always bad ID 4");
    is_bool(false, IsValidMessageID("<inva\177lid@test>", strip, lax),
            "always bad ID 5");
    is_bool(false, IsValidMessageID("<inva lid@test>", strip, lax),
            "always bad ID 6");
    is_bool(false, IsValidMessageID("<invalid@te\tst>", strip, lax),
            "always bad ID 7");
    is_bool(false, IsValidMessageID("<inva\r\nlid@test>", strip, lax),
            "always bad ID 8");
    is_bool(false, IsValidMessageID("<inva>lid@test>", strip, lax),
            "always bad ID 9");
    is_bool(false, IsValidMessageID("<>", strip, lax), "always bad ID 10");
    /* 251 characters. */
    is_bool(
        false,
        IsValidMessageID(
            "<1234567890123456789012345678901234567890123456789012345678901234"
            "56789012345678901234567890123456789012345678901234567890123456789"
            "01234567890123456789012345678901234567890123456789012345678901234"
            "5678901234567890123456789012345678901234567890@12345678>",
            strip, lax),
        "always bad ID 11");

    /* Always valid, no matter how lax the syntax is. */
    is_bool(true, IsValidMessageID("<valid@test>", strip, lax),
            "always good ID 1");
    is_bool(true, IsValidMessageID("<v4l.#%-{T`?*!.id@te|st>", strip, lax),
            "always good ID 2");
    is_bool(true, IsValidMessageID("<a@b>", strip, lax), "always good ID 3");
    is_bool(true, IsValidMessageID("<a.valid.id@testing.fr>", strip, lax),
            "always good ID 4");
    is_bool(true, IsValidMessageID("<valid@[te.st]>", strip, lax),
            "always good ID 5");
    is_bool(true, IsValidMessageID("<valid@[te;s@<t]>", strip, lax),
            "always good ID 6");
    /* 250 characters. */
    is_bool(
        true,
        IsValidMessageID(
            "<1234567890123456789012345678901234567890123456789012345678901234"
            "56789012345678901234567890123456789012345678901234567890123456789"
            "01234567890123456789012345678901234567890123456789012345678901234"
            "5678901234567890123456789012345678901234567890@1234567>",
            strip, lax),
        "always good ID 7");

    /* Only valid with lax syntax. */
    is_bool(lax, IsValidMessageID("<inva..lid@test>", strip, lax),
            "good lax ID 1");
    is_bool(lax, IsValidMessageID("<inva...lid@test>", strip, lax),
            "good lax ID 2");
    is_bool(lax, IsValidMessageID("<invalid.@test>", strip, lax),
            "good lax ID 3");
    is_bool(lax, IsValidMessageID("<invalid@>", strip, lax), "good lax ID 4");
    is_bool(lax, IsValidMessageID("<@invalid>", strip, lax), "good lax ID 5");
    is_bool(lax, IsValidMessageID("<invalid@test.>", strip, lax),
            "good lax ID 6");
    is_bool(lax, IsValidMessageID("<inva[lid@test>", strip, lax),
            "good lax ID 7");
    is_bool(lax, IsValidMessageID("<invalid@t[es]t>", strip, lax),
            "good lax ID 8");
    is_bool(lax, IsValidMessageID("<invalid@[t@].[e<s].t>", strip, lax),
            "good lax ID 9");
    is_bool(lax, IsValidMessageID("<invalid@yEnc@test>", strip, lax),
            "good lax ID 10");
    is_bool(lax, IsValidMessageID("<invalid@yEnc@twice@test>", strip, lax),
            "good lax ID 11");
    is_bool(lax, IsValidMessageID("<invalid@te..st>", strip, lax),
            "good lax ID 12");
    is_bool(lax, IsValidMessageID("<invalid@te...st>", strip, lax),
            "good lax ID 13");
    is_bool(lax, IsValidMessageID("<invalid>", strip, lax), "good lax ID 14");
    is_bool(lax, IsValidMessageID("<[INVALID-3]M-ID>", strip, lax),
            "good lax ID 15");
    is_bool(lax, IsValidMessageID("<invalidbnews..42>", strip, lax),
            "good lax ID 16");
    is_bool(lax, IsValidMessageID("<invalid12@.UUCP>", strip, lax),
            "good lax ID 17");
    is_bool(lax, IsValidMessageID("<invalid50)@@m(id.UUCP>", strip, lax),
            "good lax ID 18");
    is_bool(lax, IsValidMessageID("<inva(lid@test>", strip, lax),
            "good lax ID 19");
    is_bool(lax, IsValidMessageID("<inva;lid@test>", strip, lax),
            "good lax ID 20");
    is_bool(lax, IsValidMessageID("<inva\"lid@test>", strip, lax),
            "good lax ID 21");
    is_bool(lax, IsValidMessageID("<inva<lid@test>", strip, lax),
            "good lax ID 22");
    is_bool(lax, IsValidMessageID("<invalid@test<>", strip, lax),
            "good lax ID 23");
}


/*
**  Test a few domains that are always either bad or good.  Note that the
**  IsValidDomain() function is already implicitly tested in all the tests for
**  IsValidMessageID().
*/
void
testDomains(void)
{
    is_bool(false, IsValidDomain(NULL), "bad domain 1");
    is_bool(false, IsValidDomain(""), "bad domain 2");
    is_bool(false, IsValidDomain("@test"), "bad domain 3");
    is_bool(false, IsValidDomain("inva\177lid"), "bad domain 4");
    is_bool(false, IsValidDomain("inva lid"), "bad domain 5");
    is_bool(false, IsValidDomain("inva\r\nlid"), "bad domain 6");
    is_bool(false, IsValidDomain("inva\"lid"), "bad domain 7");

    is_bool(true, IsValidDomain("test"), "good domain 1");
    is_bool(true, IsValidDomain("v4l.#%-{T`?*!.id.te|st"), "good domain 2");
    is_bool(true, IsValidDomain("[te.st]"), "good domain 3");
    is_bool(true, IsValidDomain("[te;s@<t]"), "good domain 4");
}


/*
**  Test stripping spaces.
*/
void
testStrippingSpaces(bool strip)
{
    is_bool(strip, IsValidMessageID(" \t\t <valid@test>\t  ", strip, false),
            strip ? "good ID stripspaces 1" : "bad ID stripspaces 1");
    /* 250 characters. */
    is_bool(
        strip,
        IsValidMessageID(
            " \t <123456789012345678901234567890123456789012345678901234567890"
            "12345678901234567890123456789012345678901234567890123456789012345"
            "67890123456789012345678901234567890123456789012345678901234567890"
            "12345678901234567890123456789012345678901234567890@1234567>\t  ",
            strip, false),
        strip ? "good ID stripspaces 2" : "bad ID stripspaces 2");
    /* 251 characters. */
    is_bool(
        false,
        IsValidMessageID(
            " \t <123456789012345678901234567890123456789012345678901234567890"
            "12345678901234567890123456789012345678901234567890123456789012345"
            "67890123456789012345678901234567890123456789012345678901234567890"
            "12345678901234567890123456789012345678901234567890@12345678>\t  ",
            strip, false),
        strip ? "good ID stripspaces 3" : "bad ID stripspaces 3");
}


int
main(void)
{
    plan(4 * (11 + 7 + 23) + (7 + 4) + 2 * 3);

    /* Test several message-IDs with and without stripping spaces and lax
     * syntax. */
    testMessageIDs(true, true);
    testMessageIDs(true, false);
    testMessageIDs(false, true);
    testMessageIDs(false, false);

    /* Also test the right-hand side. */
    testDomains();

    /* Additional tests for stripping spaces. */
    testStrippingSpaces(true);
    testStrippingSpaces(false);

    return 0;
}
