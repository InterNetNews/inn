/* $Id$ */
/* wire test suite. */

#include "config.h"
#include "clibrary.h"

#include "inn/wire.h"
#include "libinn.h"
#include "libtest.h"

int
main(void)
{
    const char c1[] = "Path: \r\nFrom: \r\n\r\n";
    const char c2[] = "Path: \r\nFrom: \r\n\r";
    const char c3[] = "Path: \r\nFrom: \r\n";
    const char c4[] = "Path: \r\nFrom: \r";
    const char c5[] = "Path: \r\nFrom: ";
    const char c6[] = "\r\n\r\n";
    const char c7[] = "\r\n\r";
    const char c8[] = "\r\n";
    const char c9[] = "\r";
    const char c10[] = "";

    puts("10");

    ok(1, wire_findbody(c1, sizeof c1 - 1) == c1 + sizeof c1 - 1);
    ok(2, wire_findbody(c2, sizeof c2 - 1) == NULL);
    ok(3, wire_findbody(c3, sizeof c3 - 1) == NULL);
    ok(4, wire_findbody(c4, sizeof c4 - 1) == NULL);
    ok(5, wire_findbody(c5, sizeof c5 - 1) == NULL);
    ok(6, wire_findbody(c6, sizeof c6 - 1) == c6 + sizeof c6 - 1);
    ok(7, wire_findbody(c7, sizeof c7 - 1) == NULL);
    ok(8, wire_findbody(c8, sizeof c8 - 1) == NULL);
    ok(9, wire_findbody(c9, sizeof c9 - 1) == NULL);
    ok(10, wire_findbody(c10, sizeof c10 - 1) == NULL);
    return 0;
}
