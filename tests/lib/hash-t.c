/* $Id$ */
/* hash test suite. */

#include "config.h"
#include "clibrary.h"

#include "libinn.h"
#include "libtest.h"

int
main(void)
{
    HASH h1, h2;

    test_init(12);

    h1 = HashMessageID("<lhs@test.invalid>");
    h2 = HashMessageID("<lhs@TEST.invalid>");
    ok(1, HashCompare(&h1, &h2) == 0);
    h2 = HashMessageID("<lhs@test.INVALID>");
    ok(2, HashCompare(&h1, &h2) == 0);
    h2 = HashMessageID("<Lhs@test.invalid>");
    ok(3, HashCompare(&h1, &h2) != 0);
    h2 = HashMessageID("<lhS@test.invalid>");
    ok(4, HashCompare(&h1, &h2) != 0);
    h1 = HashMessageID("<test.invalid>");
    h2 = HashMessageID("<TEST.invalid>");
    ok(5, HashCompare(&h1, &h2) != 0);
    h2 = HashMessageID("<test.INVALID>");
    ok(6, HashCompare(&h1, &h2) != 0);
    h1 = HashMessageID("<postmaster@test.invalid>");
    h2 = HashMessageID("<POSTMASTER@test.invalid>");
    ok(7, HashCompare(&h1, &h2) == 0);
    h2 = HashMessageID("<PostMaster@test.invalid>");
    ok(8, HashCompare(&h1, &h2) == 0);
    h2 = HashMessageID("<postmasteR@test.invalid>");
    ok(9, HashCompare(&h1, &h2) == 0);
    h2 = HashMessageID("<postmaster@TEST.invalid>");
    ok(10, HashCompare(&h1, &h2) == 0);
    h2 = HashMessageID("<postmaster@test.INVALID>");
    ok(11, HashCompare(&h1, &h2) == 0);
    h1 = HashMessageID("<postmaster.test.invalid>");
    h2 = HashMessageID("<POSTMASTER.test.invalid>");
    ok(12, HashCompare(&h1, &h2) != 0);

    return 0;
}
